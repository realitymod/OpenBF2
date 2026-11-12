// Python world-object methods and weak-token conversion. Methods extract the
// object from args[0] and ignore self. Conversion failure is nonzero and preserves
// a RuntimeError; independent argument conversions are all evaluated.
// Ids.h must precede headers redeclaring ids extern, or constants acquire external
// linkage and collide with their owning definitions.
#include "../../../Core/Ids.h"
#include "../../../Core/StringUtil.h"                 // getLowerCase - template-name filter
#include "../../../Core/WeakPtr.h"
#include "../PythonComp.h"
#include "../PythonHost.h"
#include "PyArg.h"
#include "../../Objects/IObjectTemplate.h"
#include "../../../Core/IBase.h"
#include "../../../IO/Console/Console.h"
#include "../../../Math/Mat4.h"
#include "../../../Math/Vec3.h"
#include "../../../Physics/IPhysicsNode.h"
#include "../../../Physics/SoldierPhysicsNode.h"
#include "../../../Scene/Object/ObjectTypes/Base/ObjectTemplateManager.h"
#include "../../../Scene/Object/ObjectManager.h"         // getAllObjectsFromGrid - the filter walk
#include <cstdio>                                        // [TEMP] the wrapper-integrity probes
#include <cstdlib>                                       // [TEMP] their getenv arm
#include "../../../Core/SmartItf.h"                      // classManager - omgr_getObjectsOfType
#include "../../Objects/ObjectLayoutAccess.h"            // the typed +0x14 template read
// the typed interfaces that retired the raw slot views
#include "../../Objects/IObject.h"
#include "../../Objects/IPlayer.h"
#include "../../Objects/IWeaponObject.h"
#include "../../Objects/Game/SupplyObject.h"
#include "../../Objects/Game/Soldier.h"
#include "../../Objects/ISoldier.h"
#include "../../Objects/IControlPoint.h"
#include "../../Objects/ControlPointTemplate.h"
#include "../../Objects/GameLogicComp/IArmorGLComp.h"
#include "../../../Scene/Object/ObjectTypes/Game/IPlayerControlObject.h"
#include <map>

#include <cstring>
#include <deque>
#include <list>
#include <string>
#include <utility>
#include <vector>

namespace dice {
namespace hfe {

namespace world {

class IObject;
class IPlayer;

// ds:_ZN4dice3hfe5world21objectTemplateManagerE - the global object_getTemplateProperty routes
// through. Declared here because no header carried it; it belongs in ObjectTemplateManager.h once
// something else needs it. Deliberately left undefined: an unresolved external IS the to-do list.
extern ObjectTemplateManager* objectTemplateManager;

// Python handles retain a weak control block whose null target marks a destroyed object.
// Use typed object interfaces and WeakPtr::Ctrl so adjusted subobjects and native pointer
// alignment follow the active compiler rather than fixed vtable or field offsets.
struct WeakRef
{
    void* m_ctrl;
};


} // namespace world

namespace io { class OldConsole; }

namespace python {

// Declared here because there is still no marshalling header. They are defined in PythonHost.cpp.
PyObject* to_py(const Vec3& v);
PyObject* to_py(world::IObject* obj);
PyObject* to_py(world::IPlayer* player);

// Python object handle holding a weak C++ reference. It does not extend object
// lifetime; callers can detect deletion with isValid. getWeakPtr returns by value.
struct ObjectToken
{
    world::WeakRef m_weak;
};

// Extracts an ObjectToken from the exact PyCObject token attribute. Returns 1 and
// sets RuntimeError on failure. The new attribute reference is intentionally not
// released, preserving the original per-call reference leak.
int parse_arg(PyObject* o, ObjectToken*& out)
{
    PyObject* attr = PyObject_GetAttrString(o, "token");     // leaked - see above
    if (!attr || attr->ob_type != &PyCObject_Type)
    {
        PyErr_SetString(PyExc_RuntimeError, "the token attribute is missing or not a PyCObject");
        return 1;
    }

    out = (ObjectToken*)PyCObject_AsVoidPtr(attr);
    if (!out)
        PyErr_SetString(PyExc_RuntimeError, "broken token");

    return PyErr_Occurred() != 0;
}

// Resolves an object token's weak reference. A destroyed target raises an error; scripts can test isValid first.
int parse_arg(PyObject* o, world::IObject*& out)
{
    ObjectToken* token = 0;
    if (parse_arg(o, token) || !token)
    {
        if (!PyErr_Occurred())
            PyErr_SetString(PyExc_RuntimeError,
                            "unknown marshalling error while extracting "
                            "dice::hfe::world::IObject token");
        return 1;
    }

    world::IObject* obj = 0;
    if (token->m_weak.m_ctrl)
        obj = static_cast<WeakPtr<world::IObject>::Ctrl*>(token->m_weak.m_ctrl)->target;
    out = obj;

    if (!obj)
    {
        PyErr_SetString(PyExc_RuntimeError,
                        "the specified object was deleted on the engine side "
                        "(use isValid() to be sure)");
        return 1;
    }
    return 0;
}

// integers ARE accepted here. Contrast parse_arg(Vec3&) below, which rejects them.
int parse_arg(PyObject* o, double& out)
{
    if (o->ob_type == &PyFloat_Type || PyType_IsSubtype(o->ob_type, &PyFloat_Type))
    {
        out = PyFloat_AsDouble(o);
    }
    else if (o->ob_type == &PyInt_Type || PyType_IsSubtype(o->ob_type, &PyInt_Type))
    {
        out = (double)PyInt_AsLong(o);
    }
    else
    {
        PyErr_SetString(PyExc_RuntimeError, "expected a float (integers are acceptable too)");
        return 1;
    }
    return PyErr_Occurred() != 0;
}

// Requires three Python floats or float subclasses; integer vector elements are rejected.
int parse_arg(PyObject* o, Vec3& out)
{
    if (o->ob_type != &PyTuple_Type && !PyType_IsSubtype(o->ob_type, &PyTuple_Type))
    {
        PyErr_SetString(PyExc_RuntimeError, "[vec3] expected a tuple");
        return 1;
    }
    if (PyTuple_Size(o) != 3)
    {
        PyErr_SetString(PyExc_RuntimeError, "[vec3] expected a tuple of size 3");
        return 1;
    }

    float* dst = &out.x;
    for (int i = 0; i < 3; ++i)
    {
        PyObject* item = PyTuple_GetItem(o, i);
        if (!item ||
            (item->ob_type != &PyFloat_Type && !PyType_IsSubtype(item->ob_type, &PyFloat_Type)))
        {
            PyErr_SetString(PyExc_RuntimeError, "[vec3] expected a float");
            return 1;
        }
        *dst++ = (float)PyFloat_AsDouble(item);
    }
    return 0;
}

// Inlined into py_arg<int> (0x8156260) and py_arg<std::string> (0x8156170) in the binary rather
// than emitted separately; written out here so the template below stays one shape.
int parse_arg(PyObject* o, int& out)
{
    if (o->ob_type != &PyInt_Type && !PyType_IsSubtype(o->ob_type, &PyInt_Type))
    {
        PyErr_SetString(PyExc_RuntimeError, "expected an integer");
        return 1;
    }
    out = PyInt_AsLong(o);
    return PyErr_Occurred() != 0;
}

int parse_arg(PyObject* o, std::string& out)
{
    if (o->ob_type != &PyString_Type && !PyType_IsSubtype(o->ob_type, &PyString_Type))
    {
        PyErr_SetString(PyExc_RuntimeError, "expected a string");
        return 1;
    }
    const char* s = PyString_AsString(o);
    out.assign(s, strlen(s));        // the binary really does call strlen and assign(ptr, len)
    return PyErr_Occurred() != 0;
}

// py_arg<_object*> (0x81563B0) has no type check at all - it just stores the item.
int parse_arg(PyObject* o, PyObject*& out)
{
    out = o;
    return 0;
}

// [CHOSEN, not recovered] Exposes the token's weak control block for trigger
// ownership without duplicating its private layout.
void* tokenControlBlock(ObjectToken* token)
{
    return token ? token->m_weak.m_ctrl : 0;
}

// . The IObject* form. Identical in shape to py_arg<T> and emitted as its own
// symbol rather than an instantiation - which is why it has its own name.
int py_argo(PyObject* args, int index, world::IObject*& out)
{
    if (PyErr_Occurred())
        return 1;

    if (!args)
    {
        PyErr_SetString(PyExc_RuntimeError, "no arguments");
        return 1;
    }

    PyObject* item = PyTuple_GetItem(args, index);
    if (!item)
        return 1;

    return parse_arg(item, out);
}

// Releases the shared weak-control block and frees its Python object token.
void destruct_token(void* p)
{
    ObjectToken* token = (ObjectToken*)p;
    if (!token)
        return;

    int* ctrl = (int*)token->m_weak.m_ctrl;
    if (ctrl && --ctrl[0] == 0)
        ::operator delete(ctrl);

    ::operator delete(token);
}

// Binds a method as an instance attribute, allowing each wrapper to expose its object's supported operations.
bool addSingleMethod(PyObject* instance, PyMethodDef* def)
{
    PyObject* fn = PyCFunction_NewEx(def, instance, NULL);
    if (!fn)
        return false;

    PyObject* bound = PyMethod_New(fn, instance, (PyObject*)instance->ob_type);
    if (!bound)
    {
        Py_DECREF(fn);
        return false;
    }

    if (PyObject_SetAttrString(instance, def->ml_name, bound) == -1)
    {
        Py_DECREF(bound);
        Py_DECREF(fn);
        return false;
    }

    Py_DECREF(bound);
    Py_DECREF(fn);
    return true;
}

// Python object methods use fixed script-facing names, which differ from C++ symbols.
// PCO methods omit the pco_ prefix; control-point methods retain cp_. Preserve names
// and table order for game-mode scripts.

PyObject* object_getPosition(PyObject*, PyObject*);
PyObject* object_setPosition(PyObject*, PyObject*);
PyObject* object_getRotation(PyObject*, PyObject*);
PyObject* object_setRotation(PyObject*, PyObject*);
PyObject* object_getChildren(PyObject*, PyObject*);
PyObject* object_getParent(PyObject*, PyObject*);
PyObject* object_getDamage(PyObject*, PyObject*);
PyObject* object_setDamage(PyObject*, PyObject*);
PyObject* object_getTemplateProperty(PyObject*, PyObject*);
PyObject* object_isValid(PyObject*, PyObject*);
PyObject* object_pco_getPlayers(PyObject*, PyObject*);
PyObject* object_pco_getIsRemoteControlled(PyObject*, PyObject*);
PyObject* object_pco_getIsWreck(PyObject*, PyObject*);
PyObject* object_pco_getTeam(PyObject*, PyObject*);
PyObject* object_cp_getParam(PyObject*, PyObject*);
PyObject* object_cp_setParam(PyObject*, PyObject*);
PyObject* object_projectile_setTimeToLive(PyObject*, PyObject*);   // [CHOSEN, not recovered]
PyObject* object_getVelocity(PyObject*, PyObject*);                // [CHOSEN, not recovered]
PyObject* object_setVelocity(PyObject*, PyObject*);                // [CHOSEN, not recovered]
PyObject* object_soldier_getOnGround(PyObject*, PyObject*);        // [CHOSEN, not recovered]
PyObject* object_supply_getSupplyAmmo(PyObject*, PyObject*);       // [CHOSEN, not recovered]
PyObject* object_supply_setSupplyAmmo(PyObject*, PyObject*);       // [CHOSEN, not recovered]
PyObject* object_weapon_setHeat(PyObject*, PyObject*);             // [CHOSEN, not recovered]
PyObject* object_soldier_getStamina(PyObject*, PyObject*);         // [CHOSEN, not recovered]
PyObject* object_soldier_setStamina(PyObject*, PyObject*);         // [CHOSEN, not recovered]
PyObject* object_soldier_setStaminaDissipation(PyObject*, PyObject*);   // [CHOSEN, not recovered]
PyObject* object_soldier_setStaminaRecovery(PyObject*, PyObject*);      // [CHOSEN, not recovered]
PyObject* object_soldier_getPose(PyObject*, PyObject*);            // [CHOSEN, not recovered]

PyMethodDef meth_getPosition        = { "getPosition",        object_getPosition,        METH_VARARGS, 0 };
PyMethodDef meth_setPosition        = { "setPosition",        object_setPosition,        METH_VARARGS, 0 };
PyMethodDef meth_getRotation        = { "getRotation",        object_getRotation,        METH_VARARGS, 0 };
PyMethodDef meth_setRotation        = { "setRotation",        object_setRotation,        METH_VARARGS, 0 };
PyMethodDef meth_getChildren        = { "getChildren",        object_getChildren,        METH_VARARGS, 0 };
PyMethodDef meth_getParent          = { "getParent",          object_getParent,          METH_VARARGS, 0 };
PyMethodDef meth_getDamage          = { "getDamage",          object_getDamage,          METH_VARARGS, 0 };
PyMethodDef meth_setDamage          = { "setDamage",          object_setDamage,          METH_VARARGS, 0 };
PyMethodDef meth_getTemplateProperty= { "getTemplateProperty",object_getTemplateProperty,METH_VARARGS, 0 };
PyMethodDef meth_isValid            = { "isValid",            object_isValid,            METH_VARARGS, 0 };
// name != symbol, see above
PyMethodDef meth_pco_getPlayers     = { "getOccupyingPlayers", object_pco_getPlayers,     METH_VARARGS, 0 };
PyMethodDef meth_pco_getIsRemoteControlled
                                    = { "getIsRemoteControlled", object_pco_getIsRemoteControlled,
                                                                                         METH_VARARGS, 0 };
PyMethodDef meth_pco_getIsWreck     = { "getIsWreck",         object_pco_getIsWreck,     METH_VARARGS, 0 };
PyMethodDef meth_pco_getTeam        = { "getTeam",            object_pco_getTeam,        METH_VARARGS, 0 };
PyMethodDef meth_cp_getParam        = { "cp_getParam",        object_cp_getParam,        METH_VARARGS, 0 };
PyMethodDef meth_cp_setParam        = { "cp_setParam",        object_cp_setParam,        METH_VARARGS, 0 };
PyMethodDef meth_projectile_setTimeToLive
                                    = { "setTimeToLive",      object_projectile_setTimeToLive,
                                                                                         METH_VARARGS, 0 };
PyMethodDef meth_getVelocity        = { "getVelocity",        object_getVelocity,        METH_VARARGS, 0 };
PyMethodDef meth_setVelocity        = { "setVelocity",        object_setVelocity,        METH_VARARGS, 0 };
PyMethodDef meth_soldier_getOnGround= { "getOnGround",        object_soldier_getOnGround,METH_VARARGS, 0 };
PyMethodDef meth_supply_getSupplyAmmo
                                    = { "getSupplyAmmo",      object_supply_getSupplyAmmo,
                                                                                         METH_VARARGS, 0 };
PyMethodDef meth_supply_setSupplyAmmo
                                    = { "setSupplyAmmo",      object_supply_setSupplyAmmo,
                                                                                         METH_VARARGS, 0 };
PyMethodDef meth_weapon_setHeat     = { "setHeat",            object_weapon_setHeat,     METH_VARARGS, 0 };
PyMethodDef meth_soldier_getStamina = { "getStamina",         object_soldier_getStamina, METH_VARARGS, 0 };
PyMethodDef meth_soldier_setStamina = { "setStamina",         object_soldier_setStamina, METH_VARARGS, 0 };
PyMethodDef meth_soldier_setStaminaDissipation
                                    = { "setStaminaDissipation", object_soldier_setStaminaDissipation,
                                                                                         METH_VARARGS, 0 };
PyMethodDef meth_soldier_setStaminaRecovery
                                    = { "setStaminaRecovery", object_soldier_setStaminaRecovery,
                                                                                         METH_VARARGS, 0 };
PyMethodDef meth_soldier_getPose    = { "getPose",            object_soldier_getPose,    METH_VARARGS, 0 };

namespace {

// The object's physics node when it moves on its own, else 0 (no node, or a static one).
world::IPhysicsNode* dynamicPhysicsNode(world::IObject* obj)
{
    world::IPhysicsNode* node =
        static_cast<world::IPhysicsNode*>(world::object_detail::LayoutAccess::physicsNode(obj));
    return (node && node->getIsMobile()) ? node : 0;
}

world::Soldier* asSoldier(world::IObject* obj)
{
    return static_cast<world::Soldier*>(
        static_cast<world::ISoldier*>(obj->queryInterface(world::IID_ISoldier)));
}

// Raises RuntimeError and returns false when the value is negative.
bool checkSprintTime(double seconds)
{
    if (seconds >= 0.0)
        return true;
    PyErr_SetString(PyExc_RuntimeError, "sprint time must not be negative");
    return false;
}

bool isSupplyObject(world::IObject* obj)
{
    return world::object_detail::LayoutAccess::objectTemplate(obj)->getClassID()
        == world::CID_SupplyObjectTemplate;
}

} // namespace

// Adds common methods and capability-specific methods/flags to a Python object.
// Armor uses the component field; PCO and control-point capabilities use interface
// queries. Always returns true, even if an individual method binding fails.
bool addObjectMethods(PyObject* instance, world::IObject* obj)
{
    world::IObject* o = obj;

    addSingleMethod(instance, &meth_isValid);
    addSingleMethod(instance, &meth_getTemplateProperty);
    addSingleMethod(instance, &meth_getParent);
    addSingleMethod(instance, &meth_getChildren);
    addSingleMethod(instance, &meth_getPosition);
    addSingleMethod(instance, &meth_setPosition);
    addSingleMethod(instance, &meth_getRotation);
    addSingleMethod(instance, &meth_setRotation);

    long hasArmor = 0;
    if (obj->getArmorComponent())                  // IObject +0x3C, a field test
    {
        addSingleMethod(instance, &meth_getDamage);
        addSingleMethod(instance, &meth_setDamage);
        hasArmor = 1;
    }
    PyObject_SetAttrString(instance, "hasArmor", PyInt_FromLong(hasArmor));

    long isPco = 0;
    if (o->queryInterface(world::IID_IPlayerControlObject))
    {
        addSingleMethod(instance, &meth_pco_getPlayers);
        addSingleMethod(instance, &meth_pco_getIsRemoteControlled);
        addSingleMethod(instance, &meth_pco_getIsWreck);
        addSingleMethod(instance, &meth_pco_getTeam);
        isPco = 1;
    }
    PyObject_SetAttrString(instance, "isPlayerControlObject", PyInt_FromLong(isPco));

    long isCp = 0;
    if (o->queryInterface(world::IID_IControlPoint))
    {
        addSingleMethod(instance, &meth_cp_getParam);
        addSingleMethod(instance, &meth_cp_setParam);
        isCp = 1;
    }
    PyObject_SetAttrString(instance, "isControlPoint", PyInt_FromLong(isCp));

    // [CHOSEN, not recovered] Project Reality's projectile lifetime method; not in retail.
    if (o->queryInterface(world::IID_IProjectileObject))
        addSingleMethod(instance, &meth_projectile_setTimeToLive);

    // [CHOSEN, not recovered] Project Reality's physics methods; not in retail.
    long hasDynamicPhysics = 0;
    if (dynamicPhysicsNode(obj))
    {
        addSingleMethod(instance, &meth_getVelocity);
        addSingleMethod(instance, &meth_setVelocity);
        if (o->queryInterface(world::IID_ISoldier))
            addSingleMethod(instance, &meth_soldier_getOnGround);
        hasDynamicPhysics = 1;
    }
    PyObject_SetAttrString(instance, "hasDynamicPhysics", PyInt_FromLong(hasDynamicPhysics));

    // [CHOSEN, not recovered] Project Reality's supply crate and weapon heat methods; not in retail.
    if (isSupplyObject(obj))
    {
        addSingleMethod(instance, &meth_supply_getSupplyAmmo);
        addSingleMethod(instance, &meth_supply_setSupplyAmmo);
    }
    if (o->queryInterface(world::IID_IWeaponObject))
        addSingleMethod(instance, &meth_weapon_setHeat);

    // [CHOSEN, not recovered] Project Reality's soldier stamina and pose methods; not in retail.
    if (o->queryInterface(world::IID_ISoldier))
    {
        addSingleMethod(instance, &meth_soldier_getStamina);
        addSingleMethod(instance, &meth_soldier_setStamina);
        addSingleMethod(instance, &meth_soldier_setStaminaDissipation);
        addSingleMethod(instance, &meth_soldier_setStaminaRecovery);
        addSingleMethod(instance, &meth_soldier_getPose);
    }

    return true;        // always
}

// [CHOSEN, not recovered] projectile.setTimeToLive(seconds): the projectile removes itself after
// the given time, through the template message 29 GenericProjectile posts for its own lifetime.
PyObject* object_projectile_setTimeToLive(PyObject*, PyObject* args)
{
    world::IObject* obj = 0;
    double seconds = 0.0;
    int e0 = py_argo(args, 0, obj);
    int e1 = py_arg<double>(args, 1, seconds);
    if (e0 + e1)
        return 0;

    world::objectManager->postMessage(obj, static_cast<world::TemplateMessage>(29),
                                      static_cast<float>(seconds));
    Py_INCREF(Py_None);
    return Py_None;
}

// [CHOSEN, not recovered] obj.getVelocity(): the physics node's positional speed, (0, 0, 0) while
// the object is attached to a parent.
PyObject* object_getVelocity(PyObject*, PyObject* args)
{
    world::IObject* obj = 0;
    if (py_argo(args, 0, obj))
        return 0;

    return to_py(dynamicPhysicsNode(obj)->getPositionalSpeed());
}

// [CHOSEN, not recovered] obj.setVelocity((x, y, z)): wakes the physics node, then sets its
// positional speed, clamped to the engine's maximum.
PyObject* object_setVelocity(PyObject*, PyObject* args)
{
    world::IObject* obj = 0;
    Vec3 v;
    v.x = v.y = v.z = 0.0f;

    int e0 = py_argo(args, 0, obj);
    int e1 = py_arg<Vec3>(args, 1, v);
    if (e0 + e1)
        return 0;

    world::IPhysicsNode* node = dynamicPhysicsNode(obj);
    node->setIsAwake();
    node->setPositionalSpeed(v);
    Py_INCREF(Py_None);
    return Py_None;
}

// [CHOSEN, not recovered] soldier.getOnGround(): 1 when the soldier touched something since the
// last physics update, else 0. Soldiers always get a SoldierPhysicsNode
// (SimpleObjectTemplate::buildMobilePhysicsNode).
PyObject* object_soldier_getOnGround(PyObject*, PyObject* args)
{
    world::IObject* obj = 0;
    if (py_argo(args, 0, obj))
        return 0;

    world::SoldierPhysicsNode* node = static_cast<world::SoldierPhysicsNode*>(
        world::object_detail::LayoutAccess::physicsNode(obj));
    return PyInt_FromLong(node->getFrictionCount() != 0 ? 1 : 0);
}

// [CHOSEN, not recovered] supply.getSupplyAmmo(): the crate's shared storage, the pool that
// rearming and healing spill into.
PyObject* object_supply_getSupplyAmmo(PyObject*, PyObject* args)
{
    world::IObject* obj = 0;
    if (py_argo(args, 0, obj))
        return 0;

    return PyFloat_FromDouble(static_cast<world::SupplyObject*>(obj)->getSharedStorage());
}

// [CHOSEN, not recovered] supply.setSupplyAmmo(amount): sets the crate's shared storage, raising a
// negative amount to 0.
PyObject* object_supply_setSupplyAmmo(PyObject*, PyObject* args)
{
    world::IObject* obj = 0;
    double amount = 0.0;
    int e0 = py_argo(args, 0, obj);
    int e1 = py_arg<double>(args, 1, amount);
    if (e0 + e1)
        return 0;

    static_cast<world::SupplyObject*>(obj)->setSharedStorage(
        amount < 0.0 ? 0.0f : static_cast<float>(amount));
    Py_INCREF(Py_None);
    return Py_None;
}

// [CHOSEN, not recovered] weapon.setHeat(heat): sets the weapon's heat, from which it cools down as
// after firing.
PyObject* object_weapon_setHeat(PyObject*, PyObject* args)
{
    world::IObject* obj = 0;
    double heat = 0.0;
    int e0 = py_argo(args, 0, obj);
    int e1 = py_arg<double>(args, 1, heat);
    if (e0 + e1)
        return 0;

    static_cast<world::IWeaponObject*>(obj->queryInterface(world::IID_IWeaponObject))
        ->setHeat(static_cast<float>(heat));
    Py_INCREF(Py_None);
    return Py_None;
}

// [CHOSEN, not recovered] soldier.getStamina(): sprint stamina, 0.0 to 1.0.
PyObject* object_soldier_getStamina(PyObject*, PyObject* args)
{
    world::IObject* obj = 0;
    if (py_argo(args, 0, obj))
        return 0;

    return PyFloat_FromDouble(asSoldier(obj)->getSprintStamina());
}

// [CHOSEN, not recovered] soldier.setStamina(stamina): sets sprint stamina, which the soldier's
// networkable sends to clients. Raises RuntimeError outside 0.0 to 1.0.
PyObject* object_soldier_setStamina(PyObject*, PyObject* args)
{
    world::IObject* obj = 0;
    double stamina = 0.0;
    int e0 = py_argo(args, 0, obj);
    int e1 = py_arg<double>(args, 1, stamina);
    if (e0 + e1)
        return 0;

    if (stamina < 0.0 || stamina > 1.0)
    {
        PyErr_SetString(PyExc_RuntimeError, "stamina must be between 0 and 1");
        return 0;
    }
    asSoldier(obj)->setSprintStamina(static_cast<float>(stamina));
    Py_INCREF(Py_None);
    return Py_None;
}

// [CHOSEN, not recovered] soldier.setStaminaDissipation(seconds): how long a full sprint lasts,
// replacing the template's SprintDissipationTime until a player next enters the soldier, as on
// spawn. Clients keep predicting with the template value. Raises RuntimeError when negative.
PyObject* object_soldier_setStaminaDissipation(PyObject*, PyObject* args)
{
    world::IObject* obj = 0;
    double seconds = 0.0;
    int e0 = py_argo(args, 0, obj);
    int e1 = py_arg<double>(args, 1, seconds);
    if (e0 + e1 || !checkSprintTime(seconds))
        return 0;

    asSoldier(obj)->setSprintDissipationTime(static_cast<float>(seconds));
    Py_INCREF(Py_None);
    return Py_None;
}

// [CHOSEN, not recovered] soldier.setStaminaRecovery(seconds): how long stamina takes to recover,
// replacing the template's SprintRecoverTime until a player next enters the soldier, as on spawn.
// Clients keep predicting with the template value. Raises RuntimeError when negative.
PyObject* object_soldier_setStaminaRecovery(PyObject*, PyObject* args)
{
    world::IObject* obj = 0;
    double seconds = 0.0;
    int e0 = py_argo(args, 0, obj);
    int e1 = py_arg<double>(args, 1, seconds);
    if (e0 + e1 || !checkSprintTime(seconds))
        return 0;

    asSoldier(obj)->setSprintRecoverTime(static_cast<float>(seconds));
    Py_INCREF(Py_None);
    return Py_None;
}

// [CHOSEN, not recovered] soldier.getPose(): 0 standing, 1 crouching, 2 prone, 3 swimming.
PyObject* object_soldier_getPose(PyObject*, PyObject* args)
{
    world::IObject* obj = 0;
    if (py_argo(args, 0, obj))
        return 0;

    return PyInt_FromLong(static_cast<long>(asSoldier(obj)->getPose()));
}

// Returns an owned reference to the object's cached Python wrapper, preserving identity.
// Null objects or wrapping failures return a new reference to None. The token is weak.
// The original ownership handoff remains late: class-call or token-attribute failure
// can release the token twice after PyCObject has taken ownership.
PyObject* wrapObject(world::IObject* obj)
{
    if (!obj)
    {
        Py_INCREF(Py_None);
        return Py_None;
    }

    world::IObject* o = obj;

    // fast path: already wrapped, return the same object
    IBase* existing = o->queryComponent(IID_IPythonComp, IID_IPythonComp);
    if (existing)
    {
        PyObject* cached = static_cast<PythonComp*>(existing)->getPythonObject();   // vtable +0x38
        Py_INCREF(cached);
        return cached;
    }

    ObjectToken* token = new ObjectToken;                   // operator new(4)
    // The token takes its own weak-control-block reference before the temporary WeakPtr releases its reference.
    {
        WeakPtr<world::IObject> weak = o->getWeakPtr();     // IObject vtable +0x1C, by value
        token->m_weak.m_ctrl = weak.m_ctrl;
        if (token->m_weak.m_ctrl)
            ++static_cast<int*>(token->m_weak.m_ctrl)[0];   // the token's own reference
    }

    PyObject* cobj = PyCObject_FromVoidPtr(token, destruct_token);
    if (!cobj)
    {
        delete token;                                       // correct here: nothing else owns it
        Py_INCREF(Py_None);
        return Py_None;
    }
    // THE `token = 0;` belongs here. See the header block. Left where the binary puts it.

    PyObject* instance = PyObject_CallObject(pyHostObjectClass(), NULL);   // PythonHostData +0x10
    if (!instance)
    {
        Py_DECREF(cobj);                                    // runs destruct_token
        delete token;                                       // double free - preserved
        Py_INCREF(Py_None);
        return Py_None;
    }

    if (!addObjectMethods(instance, obj))                   // cannot actually fail
    {
        Py_DECREF(instance);
        Py_DECREF(cobj);
        delete token;                                       // double free - preserved
        Py_INCREF(Py_None);
        return Py_None;
    }

    if (PyObject_SetAttrString(instance, "token", cobj) == -1)
    {
        Py_DECREF(instance);
        Py_DECREF(cobj);                                    // runs destruct_token
        delete token;                                       // double free - preserved
        Py_INCREF(Py_None);
        return Py_None;
    }

    token = 0;              // ownership hand-off recorded, one step too late

    // IObject +0x14 is the template; its vtable +0x2C is getName(). The c_str() that follows is
    // inlined as everywhere else in this tree.
    {
        const std::string& templateName = obj->getTemplate()->getName();
        PyObject_SetAttrString(instance, "templateName", PyString_FromString(templateName.c_str()));
    }

    // PythonComp takes this reference; the later release drops only its local construction reference.
    Py_INCREF(instance);
    PythonComp* comp = new PythonComp(instance);            // operator new(0x0C)
    if (!o->setComponent(IID_IPythonComp, comp))            // IObject vtable +0x58, returns bool
    {
        comp->release();
        Py_INCREF(Py_None);
        return Py_None;
    }
    comp->release();                                        // the object holds the only reference

    Py_INCREF(instance);
    Py_DECREF(instance);
    Py_DECREF(cobj);
    return instance;
}

// Builds a tuple of wrappers, transferring each new wrapper reference directly into tuple storage.
PyObject* buildObjectList(const std::deque<world::IObject*>& objects)
{
    PyObject* tuple = PyTuple_New((int)objects.size());
    if (!tuple)
    {
        PyErr_SetString(PyExc_RuntimeError, "out of memory creating object result tuple");
        return 0;
    }

    int i = 0;
    for (std::deque<world::IObject*>::const_iterator it = objects.begin(); it != objects.end(); ++it)
        PyTuple_SET_ITEM(tuple, i++, wrapObject(*it));

    return tuple;
}

// Matches object-grid entries by their template class ID.
struct FilterClassID
{
    unsigned int classId;
    explicit FilterClassID(unsigned int id) : classId(id) {}

    bool operator()(world::IObject* object) const
    {
        return world::object_detail::LayoutAccess::objectTemplate(object)->getClassID() == classId;
    }
};

struct FilterTemplateName
{
    std::string name;
    explicit FilterTemplateName(const std::string& value) : name(value) {}

    bool operator()(world::IObject* object) const
    {
        const std::string& templateName =
            world::object_detail::LayoutAccess::objectTemplate(object)->getName();
        return getLowerCase(templateName) == name;
    }
};

template <typename Filter>
static void filterObjects_impl(Filter filter,
                               const std::map<unsigned int, world::IObject*>& grid,
                               std::deque<world::IObject*>& out)
{
    for (std::map<unsigned int, world::IObject*>::const_iterator it = grid.begin();
         it != grid.end(); ++it)
    {
        world::IObject* const object = it->second;
        if (filter(object))
            out.push_back(object);
    }
}

template <typename Filter>
static void filterObjects(Filter filter, std::deque<world::IObject*>& out)
{
    filterObjects_impl(filter, world::objectManager->getAllObjectsFromGrid(), out);   // +0x34
}

// omgr_getObjectsOfType - ELF 0x81584F0, read in full. host.omgr_getObjectsOfType(typeName).
// AN unknown type name sets THE RuntimeError AND keeps going - the ELF does not return after
// "unknown object type"; it filters on whatever getClassID left in the out-param and returns the
// (empty) tuple with the error pending. Shipped shape, kept.
PyObject* omgr_getObjectsOfType(PyObject*, PyObject* args)
{
    std::string name;
    if (py_arg<std::string>(args, 0, name))
        return 0;

    unsigned int classId = 0;
    if (!classManager->getClassID(name, classId))        // vt +0x2C
        PyErr_SetString(PyExc_RuntimeError, "unknown object type");   // and continue - see above

    std::deque<world::IObject*> matches;
    filterObjects(FilterClassID(classId), matches);
    PyObject* const list = buildObjectList(matches);

    return list;
}

// omgr_getObjectsOfTemplate - ELF 0x81586C0 (440 B), PE 0x536A70. The requested name and each
// object's template name are compared in lower case; the result uses the same immutable wrapper
// tuple and object-grid walk as omgr_getObjectsOfType.
PyObject* omgr_getObjectsOfTemplate(PyObject*, PyObject* args)
{
    std::string name;
    if (py_arg<std::string>(args, 0, name))
        return 0;

    std::deque<world::IObject*> matches;
    filterObjects(FilterTemplateName(getLowerCase(name)), matches);
    return buildObjectList(matches);
}

// Recursively collects occupants, skipping parented soldiers to avoid duplicate passengers.
// [UNRESOLVED] Object flag bit zero suppresses the entire subtree.
void getAllPlayers(world::IObject* obj, std::list<world::IPlayer*>& out)
{
    if ((obj->getFlags() & 1u) != 0)                                    // +0x04 & 1
        return;

    if (obj->getTemplate()->getClassID() == world::CID_SoldierTemplate &&
        obj->getParent() != 0)
        return;                                                          // see above

    world::IObject* o = obj;
    void* pcoRaw = o->queryInterface(world::IID_IPlayerControlObject);
    if (pcoRaw)
    {
        world::IPlayerControlObject* pco = static_cast<world::IPlayerControlObject*>(pcoRaw);
        world::IPlayer* player = pco->getOccupingPlayer();               // +0x10 (dice's spelling)
        // against the ELF (0x8158060): the second call is
        // `(*(*v5 + 108))(v5)` - ON THE player (v5), not on the pco. IPlayer +0x6C is getIsAlive.
        // The old transcription read it as a pco method named "isOccupying", which does not exist:
        // the pco's own +0x6C is getWeapons.
        if (player && player->getIsAlive())                              // IPlayer +0x6C
            out.push_back(player);
    }

    for (world::IObject* child = o->getChild(); child; child = child->getSibling())
        getAllPlayers(child, out);
}

PyObject* object_getPosition(PyObject*, PyObject* args)
{
    world::IObject* obj = 0;
    if (py_argo(args, 0, obj))
        return 0;

    return to_py(obj->getAbsolutePosition());                            // PE +0x68 / ELF +0x6C
}

// always absolute, even for a child object - unlike the rotation pair below, which switches on
// whether the object has a parent. That asymmetry is in the binary, not a transcription slip.
PyObject* object_setPosition(PyObject*, PyObject* args)
{
    world::IObject* obj = 0;
    Vec3 p;
    p.x = p.y = p.z = 0.0f;

    int e0 = py_argo(args, 0, obj);
    int e1 = py_arg<Vec3>(args, 1, p);
    if (e0 + e1)                                                          // summed, not short-circuit
        return 0;

    obj->setAbsolutePosition(p);                                         // PE +0x6C / ELF +0x70
    Py_INCREF(Py_None);
    return Py_None;
}

// Returns negated, reversed engine ZXY angles: (-c, -b, -a). Child objects use
// relative rotation; roots use absolute rotation. setRotation uses the same convention.
PyObject* object_getRotation(PyObject*, PyObject* args)
{
    world::IObject* obj = 0;
    if (py_argo(args, 0, obj))
        return 0;

    world::IObject* o = obj;
    const Mat4& m = obj->getParent() ? o->getRelativeTransformation()   // +0xE8
                                     : o->getAbsoluteTransformation();  // +0x84

    Vec3 e;
    getRotationZXY(m, e.x, e.y, e.z);

    Vec3 result;
    result.x = -e.z;
    result.y = -e.y;
    result.z = -e.x;
    return to_py(result);
}

// Replaces rotation while preserving translation by updating the existing matrix.
PyObject* object_setRotation(PyObject*, PyObject* args)
{
    world::IObject* obj = 0;
    Vec3 e;
    e.x = e.y = e.z = 0.0f;

    int e0 = py_argo(args, 0, obj);
    int e1 = py_arg<Vec3>(args, 1, e);
    if (e0 + e1)
        return 0;

    world::IObject* o = obj;
    const bool relative = obj->getParent() != 0;

    Mat4 m = relative ? o->getRelativeTransformation()      // +0xE8
                      : o->getAbsoluteTransformation();     // +0x84  - copied, not aliased

    setRotationZXY(m, -e.z, -e.y, -e.x);                    // reversed and negated, as above

    if (relative)
        o->setRelativeTransformation(m);                    // +0xEC
    else
        o->setAbsoluteTransformation(m);                    // +0x8C

    Py_INCREF(Py_None);
    return Py_None;
}

// . First child via the virtual, the rest through the sibling chain.
PyObject* object_getChildren(PyObject*, PyObject* args)
{
    world::IObject* obj = 0;
    if (py_argo(args, 0, obj))
        return 0;

    std::deque<world::IObject*> children;
    for (world::IObject* c = obj->getChild();                            // PE +0xEC / ELF +0xF0
         c;
         c = c->getSibling())                                              // +0x2C
        children.push_back(c);

    return buildObjectList(children);
}

// A field read, NOT A virtual - IObject +0x28. to_py(IObject*) tail-jumps to wrapObject, so a
// root object yields None rather than NULL.
PyObject* object_getParent(PyObject*, PyObject* args)
{
    world::IObject* obj = 0;
    if (py_argo(args, 0, obj))
        return 0;

    return to_py(obj->getParent());
}

// Returns None if the armor component disappeared after wrapping; scripts must handle that case.
PyObject* object_getDamage(PyObject*, PyObject* args)
{
    world::IObject* obj = 0;
    if (py_argo(args, 0, obj))
        return 0;

    world::IArmorGLComp* armor = static_cast<world::IArmorGLComp*>(obj->getArmorComponent());
    if (!armor)
    {
        Py_INCREF(Py_None);
        return Py_None;
    }

    return PyFloat_FromDouble(armor->getHitPoints());          // +0x3C
}

// silently does nothing when there is no armor component - and still returns None, so the script
// cannot tell the write was dropped. The double is narrowed to float before the call.
PyObject* object_setDamage(PyObject*, PyObject* args)
{
    world::IObject* obj = 0;
    double value = 0.0;

    int e0 = py_argo(args, 0, obj);
    int e1 = py_arg<double>(args, 1, value);
    if (e0 + e1)
        return 0;

    world::IArmorGLComp* armor = static_cast<world::IArmorGLComp*>(obj->getArmorComponent());
    if (armor)
        armor->setHitPoints((float)value);                     // +0x38

    Py_INCREF(Py_None);
    return Py_None;
}

// Reads a template property by executing its console getter and returns text.
// Rejects whitespace in the property name to prevent command injection. Temporarily
// uses access level 2 and restores the previous access afterward; rejection returns None.
PyObject* object_getTemplateProperty(PyObject*, PyObject* args)
{
    world::IObject* obj = 0;
    std::string name;

    int e0 = py_argo(args, 0, obj);
    int e1 = py_arg<std::string>(args, 1, name);
    if (e0 + e1)
        return 0;

    // Reject whitespace before interpolating the name into a command.
    if (name.find_first_of(" \n\r\t") != std::string::npos)
    {
        Py_INCREF(Py_None);
        return Py_None;
    }

    world::objectTemplateManager->setActiveObjectTemplate(
        obj->getTemplate());                                        // manager vtable +0x54

    const int savedAccess = io::mainConsole->getAccess();
    io::mainConsole->setAccess(2);

    std::string result;
    std::string command = "objectTemplate." + name;
    command.append("\n", strlen("\n"));
    io::mainConsole->executeLine(command, result, false, false, false, false, false);

    io::mainConsole->setAccess(savedAccess);

    return PyString_FromString(result.c_str());
}

// Tests the token directly, returning false for a destroyed target instead of raising a lookup error.
PyObject* object_isValid(PyObject*, PyObject* args)
{
    ObjectToken* token = 0;
    if (py_arg<ObjectToken*>(args, 0, token))
        return 0;

    world::IObject* obj = 0;
    if (token->m_weak.m_ctrl)
        obj = static_cast<WeakPtr<world::IObject>::Ctrl*>(token->m_weak.m_ctrl)->target;

    return PyInt_FromLong(obj != 0);
}

// The std::list header node is heap-allocated (operator new(0xC), self-linked) - the pre-3.4
// libstdc++ pattern this tree has now seen four times. See Game/Objects/ObjectTemplate.h.
// If PyTuple_New fails it returns NULL without raising, so the interpreter reports
// "SystemError: NULL result without error".
PyObject* object_pco_getPlayers(PyObject*, PyObject* args)
{
    world::IObject* obj = 0;
    if (py_argo(args, 0, obj))
        return 0;

    std::list<world::IPlayer*> players;
    getAllPlayers(obj, players);

    PyObject* tuple = PyTuple_New((int)players.size());
    if (tuple)
    {
        int i = 0;
        for (std::list<world::IPlayer*>::const_iterator it = players.begin();
             it != players.end(); ++it)
            PyTuple_SetItem(tuple, i++, to_py(*it));       // players cross as an integer id
    }
    return tuple;
}

// Checks direct children for a remote-controlled template.
// The unused deque still performs its original allocation and destruction.
PyObject* object_pco_getIsRemoteControlled(PyObject*, PyObject* args)
{
    world::IObject* obj = 0;
    if (py_argo(args, 0, obj))
        return 0;

    std::deque<world::IObject*> unused;      // never used - see above. Allocates and frees.

    for (world::IObject* c = obj->getChild();
         c;
         c = c->getSibling())
    {
        if (c->getTemplate()->getClassID() == world::CID_RemoteControlledObjectTemplate)
            return PyInt_FromLong(1);
    }
    return PyInt_FromLong(0);
}

// Returns whether armor destruction state is exactly 1; objects without armor return false.
PyObject* object_pco_getIsWreck(PyObject*, PyObject* args)
{
    world::IObject* obj = 0;
    if (py_argo(args, 0, obj))
        return 0;

    world::IArmorGLComp* armor = static_cast<world::IArmorGLComp*>(obj->getArmorComponent());
    const long isWreck = (armor && armor->getStatus() == 1) ? 1 : 0;
    return PyInt_FromLong(isWreck);
}

// NO NULL check ON THE queryInterface result. If the object is not a PlayerControlObject this
// dereferences null. addObjectMethods only attaches the method when the query succeeded, so it is
// guarded by construction - but the wrapper is cached and outlives that check, so an object that
// stops being a PCO keeps a method that will crash. Preserved.
PyObject* object_pco_getTeam(PyObject*, PyObject* args)
{
    world::IObject* obj = 0;
    if (py_argo(args, 0, obj))
        return 0;

    world::IPlayerControlObject* pco = static_cast<world::IPlayerControlObject*>(
        obj->queryInterface(world::IID_IPlayerControlObject));

    return PyInt_FromLong(pco->getTeam());                 // +0x98, no null check
}

// Reads named runtime/template control-point attributes. supplyValueNeeded,
// supplyValue, areaValue, allowCaptureByTeam, and supplyGroupsNeeded require args[2].
// Unknown names raise RuntimeError.
PyObject* object_cp_getParam(PyObject*, PyObject* args)
{
    world::IObject* obj = 0;
    std::string name;

    int e0 = py_argo(args, 0, obj);
    int e1 = py_arg<std::string>(args, 1, name);
    if (e0 + e1)
        return 0;

    world::IControlPoint* cp = static_cast<world::IControlPoint*>(
        obj->queryInterface(world::IID_IControlPoint));
    if (!cp)
    {
        PyErr_SetString(PyExc_RuntimeError, "object isn't a control point");
        return 0;
    }

    world::IControlPointTemplate* cpt = static_cast<world::IControlPointTemplate*>(
        obj->getTemplate()->queryInterface(world::IID_IControlPointTemplate));

    // the runtime control point
    if (name.compare("team") == 0)                    return PyInt_FromLong(cp->getTeam());
    if (name.compare("takeOverChangePerSecond") == 0) return PyFloat_FromDouble(cp->getTakeOverChangePerSecond());
    if (name.compare("flag") == 0)                    return PyInt_FromLong(cp->getFlag());
    if (name.compare("capturingState") == 0)          return PyInt_FromLong(cp->getCapturingState());

    // the authored template
    if (name.compare("onlyTakeableByTeam") == 0)      return PyInt_FromLong(cpt->getOnlyTakeableByTeam());
    if (name.compare("unableToChangeTeam") == 0)      return PyInt_FromLong(cpt->getUnableToChangeTeam() != 0);
    if (name.compare("isHemisphere") == 0)            return PyInt_FromLong(cpt->getIsHemisphere() != 0);
    if (name.compare("supplyGroupId") == 0)           return PyInt_FromLong(cpt->getGroupId());
    if (name.compare("timeToLoseControl") == 0)       return PyFloat_FromDouble(cpt->getTimeToLoseControl());
    if (name.compare("timeToGetControl") == 0)        return PyFloat_FromDouble(cpt->getTimeToGetControl());
    if (name.compare("enemyTicketLossWhenCaptured") == 0)
                                                      return PyInt_FromLong(cpt->getEnemyTicketLossWhenCaptured());
    if (name.compare("hasGreyCapturingState") == 0)   return PyInt_FromLong(cpt->getHasGreyCapturingState() != 0);

    // the ones that read a third argument
    if (name.compare("supplyValueNeeded") == 0)
    {
        int group = 0;
        if (py_arg<int>(args, 2, group)) return 0;
        return PyInt_FromLong(cpt->getSupplyValueNeeded(group));
    }
    if (name.compare("supplyValue") == 0)
    {
        int group = 0;
        if (py_arg<int>(args, 2, group)) return 0;
        return PyInt_FromLong(cpt->getSupplyValue(group));
    }
    if (name.compare("areaValue") == 0)
    {
        int which = 0;
        if (py_arg<int>(args, 2, which)) return 0;
        if (which == 1) return PyInt_FromLong(cpt->getAreaValueTeam1());  // +0x84
        if (which == 2) return PyInt_FromLong(cpt->getAreaValueTeam2());  // +0x8C
        // The selector is a team number; only teams 1 and 2 are supported.
        PyErr_SetString(PyExc_RuntimeError, "invalid team");
        return 0;
    }
    if (name.compare("allowCaptureByTeam") == 0)
    {
        int team = 0;
        if (py_arg<int>(args, 2, team)) return 0;
        return PyInt_FromLong(cp->getAllowCaptureByTeam(team) != 0);
    }
    if (name.compare("supplyGroupsNeeded") == 0)
    {
        int team = 0;
        if (py_arg<int>(args, 2, team)) return 0;

        // signature by the typed interface: it is an OUT-parameter walk
        // (`bool getSupplyGroupsNeeded(int team, std::vector<SupplyGroup>&)`, IControlPoint +0x44),
        // not a by-value return - the raw slot view had guessed the latter. Each element is a
        // {int group; bool needed;} pair, and each becomes a (groupId, needed) 2-tuple.
        std::vector<world::SupplyGroup> groups;
        cp->getSupplyGroupsNeeded(team, groups);

        PyObject* tuple = PyTuple_New((int)groups.size());
        if (tuple)
        {
            int i = 0;
            for (std::vector<world::SupplyGroup>::const_iterator it = groups.begin();
                 it != groups.end(); ++it)
            {
                PyObject* pair = PyTuple_New(2);
                if (pair)
                {
                    PyTuple_SetItem(pair, 0, PyInt_FromLong(it->group));
                    PyTuple_SetItem(pair, 1, PyInt_FromLong(it->needed ? 1 : 0));
                }
                PyTuple_SetItem(tuple, i++, pair);   // pair may be NULL; the binary stores it anyway
            }
        }
        return tuple;
    }

    PyErr_SetString(PyExc_RuntimeError, "bad attribute");
    return 0;
}

// Writes six runtime control-point properties without changing templates.
// allowCaptureByTeam also takes a team and flag; flag writes always pass false as the second setFlag argument.
PyObject* object_cp_setParam(PyObject*, PyObject* args)
{
    world::IObject* obj = 0;
    std::string name;

    int e0 = py_argo(args, 0, obj);
    int e1 = py_arg<std::string>(args, 1, name);
    if (e0 + e1)
        return 0;

    world::IControlPoint* cp = static_cast<world::IControlPoint*>(
        obj->queryInterface(world::IID_IControlPoint));
    if (!cp)
    {
        PyErr_SetString(PyExc_RuntimeError, "object isn't a control point");
        return 0;
    }

    if (name.compare("team") == 0)
    {
        int team = 0;
        if (py_arg<int>(args, 2, team)) return 0;
        cp->setTeam(team);                                               // +0x18
    }
    else if (name.compare("playerId") == 0)
    {
        int id = 0;
        if (py_arg<int>(args, 2, id)) return 0;
        cp->setPlayer(id);                                               // +0x20 (typed name)
    }
    else if (name.compare("takeOverChangePerSecond") == 0)
    {
        double v = 0.0;
        if (py_arg<double>(args, 2, v)) return 0;
        cp->setTakeOverChangePerSecond((float)v);                        // +0x3C
    }
    else if (name.compare("allowCaptureByTeam") == 0)
    {
        int team = 0, allow = 0;
        int a0 = py_arg<int>(args, 2, team);
        int a1 = py_arg<int>(args, 3, allow);
        if (a0 + a1) return 0;
        cp->setAllowCaptureByTeam(team, allow > 0);                      // +0x30, note `> 0`
    }
    else if (name.compare("flag") == 0)
    {
        int flag = 0;
        if (py_arg<int>(args, 2, flag)) return 0;
        cp->setFlag(flag, false);                                        // +0x4C, second arg hardcoded
    }
    else if (name.compare("capturingState") == 0)
    {
        int state = 0;
        if (py_arg<int>(args, 2, state)) return 0;
        cp->setCapturingState(state);                                    // +0x54
    }
    else
    {
        PyErr_SetString(PyExc_RuntimeError, "bad attribute");
        return 0;
    }

    Py_INCREF(Py_None);
    return Py_None;
}

} // namespace python
} // namespace hfe
} // namespace dice
