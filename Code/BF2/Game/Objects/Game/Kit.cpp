// Kit inventory, unlock handling, and lifecycle.
#include "Kit.h"

#include <cstdio>
#include <cstdlib>

#include "KitTemplate.h"
#include "../IItemTemplate.h"
#include "../../../Core/Debug.h"                  // the two attested kit.cpp Debug lines (529, 544)
#include "../../../Core/Ids.h"
#include "../../../Core/StringUtil.h"             // compareIgnoreCase, formatString
#include "../../../Math/Mat4.h"
#include "../../../Scene/Object/ObjectManager.h"  // postMessage / removeAllMessagesTo
#include "../../../Scene/Object/ObjectTypes/Base/ObjectTemplateManager.h"
#include "../../../Scene/Object/ObjectUtil.h"     // setNetworkableComp, setGeometry
#include "../../IGame.h"                          // game, destroyObject
#include "../IItemContainerObject.h"
#include "../IObjectTemplate.h"
#include "../IPlayer.h"
#include "../ItemContainerTemplate.h"
#include "../ObjectLayoutAccess.h"                // m_geometry (+0x38) - updateGeometry's gate
#include <cstdlib>                                // the shipped aborts
#include <set>                                    // updateUnlockStatus' two working sets
#if defined(_MSC_VER)
#include <intrin.h>                               // retail Win32 missing-template breakpoint
#endif

namespace dice {
namespace hfe {
namespace world {

namespace {

// in the attested kit.cpp TU. Despite the name this
// collects only direct children, by name through the manager; it does not use the weak cache.
void findTemplateChildsOfType(std::vector<BundleTemplate::TemplateInfo*>& children,
                             std::vector<IObjectTemplate*>& out,
                             unsigned int classId, unsigned int interfaceId)
{
    for (std::vector<BundleTemplate::TemplateInfo*>::iterator it = children.begin();
         it != children.end(); ++it)
    {
        IObjectTemplate* const objectTemplate = objectTemplateManager->getTemplate((*it)->m_templateName);
        if (!objectTemplate)
        {
            // [PLATFORM] PE traps then skips this entry if a debugger resumes; ELF aborts.
#if defined(_WIN32)
            __debugbreak();
            continue;
#else
            std::abort();
#endif
        }
        if ((!classId && !interfaceId) || objectTemplate->queryInterface(interfaceId) ||
            objectTemplate->getClassID() == classId)
            out.push_back(objectTemplate);
    }
}

// Geometry selection and LOD selection are different interfaces. Kit indices select geometry records.
// [PLATFORM] Use the verified per-port geometry slots below.
#ifdef _MSC_VER
typedef int  (__thiscall *GeometryGetIntFn)(void*);
typedef void (__thiscall *GeometrySetIntFn)(void*, int);
typedef void (__thiscall *GeometrySetBoneTransformsFn)(void*, const Mat4*, unsigned int);
const unsigned int kGeometrySetGeomIndexSlot     = 0x6C;
const unsigned int kGeometryGetGeomIndexSlot     = 0x70;
const unsigned int kGeometryGetGeomCountSlot     = 0x74;
const unsigned int kGeometrySetBoneTransformsSlot = 0x54;
#else
typedef int  (*GeometryGetIntFn)(void*);
typedef void (*GeometrySetIntFn)(void*, int);
typedef void (*GeometrySetBoneTransformsFn)(void*, const Mat4*, unsigned int);
const unsigned int kGeometrySetGeomIndexSlot     = 0x70;
const unsigned int kGeometryGetGeomIndexSlot     = 0x74;
const unsigned int kGeometryGetGeomCountSlot     = 0x78;
const unsigned int kGeometrySetBoneTransformsSlot = 0x58;
#endif

int geometryGetGeomCount(void* geometry)
{
    if (!geometry)
        return 0;
    void** const vtable = *reinterpret_cast<void***>(geometry);
    return reinterpret_cast<GeometryGetIntFn>(
        vtable[kGeometryGetGeomCountSlot / 4])(geometry);
}

int geometryGetGeomIndex(void* geometry)
{
    if (!geometry)
        return 0;
    void** const vtable = *reinterpret_cast<void***>(geometry);
    return reinterpret_cast<GeometryGetIntFn>(
        vtable[kGeometryGetGeomIndexSlot / 4])(geometry);
}

void geometrySetGeomIndex(void* geometry, int index)
{
    if (!geometry)
        return;
    void** const vtable = *reinterpret_cast<void***>(geometry);
    reinterpret_cast<GeometrySetIntFn>(
        vtable[kGeometrySetGeomIndexSlot / 4])(geometry, index);
}


// Clears the geometry's bone palette with a null pointer and zero count when a kit is dropped.
// The renderer then uses its identity palette.
void geometrySetBoneTransforms(void* geometry, const Mat4* transforms, unsigned int boneCount)
{
    if (!geometry)
        return;
    void** const vtable = *reinterpret_cast<void***>(geometry);
    reinterpret_cast<GeometrySetBoneTransformsFn>(
        vtable[kGeometrySetBoneTransformsSlot / 4])(geometry, transforms, boneCount);
}

// removeAllKitItems - 0x8183ED0, 93 B. TU-local (the ELF's own anonymous namespace, keyed to
// Kit's constructor). Detaches every kit item from `object`, recursing through item containers so
// their contents are detached from the container rather than from the kit.
// the sibling is cached before the call - removeChild unlinks the node being walked.
void removeAllKitItems(IObject* object)
{
    for (IObject* child = object->getChild(); child; )                     // slot +0xF0
    {
        IObject* const next = child->getSibling();                         // +0x2C
        if (child->getTemplate()->getClassID() == CID_ItemContainerTemplate)
            removeAllKitItems(child);
        else
            object->removeChild(child);                                    // slot +0xF8
        child = next;
    }
}

} // namespace

// . Signed maximum with a zero floor, over item containers.
int KitTemplate::getHighestUnlockLevel()
{
    std::vector<IObjectTemplate*> children;
    findTemplateChildsOfType(m_templateInfos, children, CID_ItemContainerTemplate, 0);
    int highest = 0;
    for (std::vector<IObjectTemplate*>::iterator it = children.begin(); it != children.end(); ++it)
    {
        ItemContainerTemplate* const container = static_cast<ItemContainerTemplate*>(
            (*it)->queryImplementation(CID_ItemContainerTemplate)); // no null guard in either port
        if (container->m_unlockLevel > highest)
            highest = container->m_unlockLevel;
    }
    return highest;
}

const KitTemplate* Kit::kitTemplate() const { return m_kitTemplate; }
// ELF8186080 (mangled KitTemplate::getUnlockItems(int)), client PE595C70.
// The return points into this object's own vector (ELF+1E4/clientPE+2B8).
std::vector<IObjectTemplate*>* KitTemplate::getUnlockItems(int level)
{
    m_unlockItems.resize(0, static_cast<IObjectTemplate*>(0));
    if (!level) {
        std::vector<IObjectTemplate*> children;
        findTemplateChildsOfType(m_templateInfos, children, 0, IID_IItemTemplate);
        for (auto it = children.begin(); it != children.end(); ++it) {
            IItemTemplate* const item = static_cast<IItemTemplate*>(
                (*it)->queryInterface(IID_IItemTemplate));
            if (item && item->getItemIndex() >= 0)
                m_unlockItems.push_back(*it);
        }
    } else {
        std::vector<IObjectTemplate*> containers;
        findTemplateChildsOfType(m_templateInfos, containers, CID_ItemContainerTemplate, 0);
        ItemContainerTemplate* selected = 0;
        for (auto it = containers.begin(); it != containers.end(); ++it) {
            ItemContainerTemplate* const container = static_cast<ItemContainerTemplate*>(*it);
            if (container->m_unlockLevel == level) {
                selected = container;
                break;
            }
        }
        if (selected) {
            std::vector<IObjectTemplate*> children;
            findTemplateChildsOfType(selected->m_templateInfos, children, 0, IID_IItemTemplate);
            if (!children.empty())
                for (auto it = children.begin(); it != children.end(); ++it)
                    m_unlockItems.push_back(*it);
        }
    }
    return &m_unlockItems;
}


// Initializes empty kit containers and an unselected unlock level.
// An ability restore rate of -1 disables the meter; other rates start the meter full.
Kit::Kit(const KitTemplate* objectTemplate)
    : Item(objectTemplate)
    , m_partOwners()
    , m_itemContainers()
    , m_recycleBin(0)
    , m_currentAbility(objectTemplate->m_abilityRestoreRate != -1.0f ? 1.0f : -1.0f)
    , m_chosenUnlockLevel(-1)
    , m_geomIndex(objectTemplate->m_defaultGeomIndex)
    , m_defaultGeomIndex(objectTemplate->m_defaultGeomIndex)
    , m_dropGeomIndex(objectTemplate->m_dropGeom)
    , m_kitTemplate(objectTemplate)
{
}

// the two map destructors and Item's, all compiler-emitted. Nothing of its own.
Kit::~Kit()
{
}

// Builds part ownership and replacement maps, then a disabled recycle-bin child and the initial unlock selection.
// Requires children with unique item indices; violated invariants abort.
void Kit::init()
{
    Bundle::init();
    setGeomIndex(-1);

    IObject* child = getChild();                                           // slot +0xF0
    if (!child)
        std::abort();                                                      // as shipped
    do
    {
        if (child->getTemplate()->getClassID() != CID_MaskObjectTemplate)
            child->updateFlags(1, 0);                                      // slot +0x60
        if (IItemObject* const item =
                static_cast<IItemObject*>(child->queryInterface(IID_IItemObject)))
        {
            if (checkIfPartExists(child, item->getItemIndex()))             // IItemObject +0x30
                std::abort();                                              // as shipped
        }
        child->updateFlags(0, 256);
        // must BE qualified: Object declares a same-named descendant-only virtual, which hides
        // this self-inclusive namespace helper inside any member of the hierarchy.
        ::dice::hfe::world::updateFlagsRecursive(child, 0, 0x100u);

        if (child->getTemplate()->getClassID() == CID_ItemContainerTemplate)
        {
            m_itemContainers.insert(
                std::make_pair(child, std::vector<IObject*>()));

            for (IObject* part = child->getChild();
                 part && part->queryInterface(IID_IKitPart);
                 part = part->getSibling())
            {
                part->updateFlags(1, 0);
                part->updateFlags(0, 256);
                ::dice::hfe::world::updateFlagsRecursive(part, 0, 0x100u);
                m_partOwners.insert(std::make_pair(part, child));
            }
        }
        else
        {
            m_partOwners.insert(std::make_pair(child, static_cast<IObject*>(0)));
        }
        child = child->getSibling();
    }
    while (child);

    // phase 2: name-match each container's replacement list against the registered parts
    for (std::map<IObject*, std::vector<IObject*> >::iterator container = m_itemContainers.begin();
         container != m_itemContainers.end(); ++container)
    {
        const ItemContainerTemplate* const containerTemplate =
            static_cast<const ItemContainerTemplate*>(container->first->getTemplate());
        for (std::list<std::string>::const_iterator name = containerTemplate->m_items.begin();
             name != containerTemplate->m_items.end(); ++name)
        {
            for (std::map<IObject*, IObject*>::iterator part = m_partOwners.begin();
                 part != m_partOwners.end(); ++part)
            {
                if (!compareIgnoreCase(part->first->getTemplate()->getName(), *name))
                    container->second.push_back(part->first);
            }
        }
    }

    // phase 3: the recycle bin
    IObjectTemplate* recycleBinTemplate =
        objectTemplateManager->getTemplate(std::string("recycle bin"),      // +0x28
                                           CID_ItemContainerTemplate);
    if (!recycleBinTemplate)
        recycleBinTemplate = objectTemplateManager->createTemplate(         // +0x58
            CID_ItemContainerTemplate, std::string("recycle bin"));
    m_recycleBin = recycleBinTemplate->createObject();                      // unchecked, as shipped
    addChild(m_recycleBin);                                                 // slot +0xF4
    m_recycleBin->updateFlags(1, 0);

    updateUnlockStatus();
    updateGeometry(false);
}

// Reparents parts between the kit and recycle bin according to the unlock level.
// Unlocked containers replace named defaults. An already placed container-owned part wins an index conflict.
// Requires consistent ownership entries and detached parts.
void Kit::updateUnlockStatus()
{
    removeAllKitItems(this);

    std::set<IObject*> unlockedContainers;
    std::set<IObject*> unlockedParts;

    for (std::map<IObject*, std::vector<IObject*> >::iterator it = m_itemContainers.begin();
         it != m_itemContainers.end(); ++it)
    {
        IObject* const container = it->first;
        if (container->getTemplate()->getClassID() != CID_ItemContainerTemplate)
            std::abort();                                                   // as shipped
        const ItemContainerTemplate* const containerTemplate =
            static_cast<const ItemContainerTemplate*>(container->getTemplate());
        if (m_chosenUnlockLevel >= containerTemplate->m_unlockLevel)
        {
            unlockedContainers.insert(container);
            unlockedParts.insert(it->second.begin(), it->second.end());
        }
    }

    std::map<int, IObject*> placedByItemIndex;

    for (std::map<IObject*, IObject*>::iterator it = m_partOwners.begin();
         it != m_partOwners.end(); ++it)
    {
        IObject* const part  = it->first;
        IObject* const owner = it->second;

        bool use = true;
        if (unlockedParts.find(part) != unlockedParts.end()
            || (owner && unlockedContainers.find(owner) == unlockedContainers.end()))
            use = false;

        IItemObject* itemFace = 0;
        if (part)
            itemFace = static_cast<IItemObject*>(part->queryInterface(IID_IItemObject));

        if (use && itemFace && part->queryInterface(IID_IKitPart))
        {
            const std::map<int, IObject*>::iterator placed =
                placedByItemIndex.find(itemFace->getItemIndex());
            if (placed != placedByItemIndex.end())
            {
                IObject* const existing =
                    placedByItemIndex.find(itemFace->getItemIndex())->second;   // a second find, as compiled
                if (m_partOwners.find(existing)->second)                        // unchecked, as shipped
                {
                    use = false;                                                // the container's part wins
                }
                else
                {
                    removeChild(existing);                                      // evict the default
                    m_recycleBin->addChild(existing);
                }
            }
        }

        if (part->getParent())                                                  // +0x28
            std::abort();                                                       // as shipped

        if (use)
        {
            addChild(part);
            if (itemFace && part->queryInterface(IID_IKitPart))
                placedByItemIndex[itemFace->getItemIndex()] = part;
        }
        else
        {
            m_recycleBin->addChild(part);
        }
    }
}

// Initializes unlock selection from the player on first pickup and adopts the soldier faction's kit geometry.
// Requires a valid object and an unparented kit; player may be null.
void Kit::handlePickup(IPlayer* player, IObject* obj)
{
    if (getParent())
        std::abort();                                                       // as shipped
    if (obj->getTemplate()->getClassID() != CID_SoldierTemplate)
        return;

    // [CHOSEN, not recovered] Project Reality drops the updateUnlockStatus() call retail makes here:
    // it crashes now and then on pickup, and PR does not use unlocks. The level is still stored,
    // but the kit keeps its default parts.
    if (m_chosenUnlockLevel == -1 && player)
        m_chosenUnlockLevel = player->getChosenUnlockLevel(player->getKit(), player->getTeam());

    giveSoldierKitItems(obj, player);

    const std::string& soldierName = obj->getTemplate()->getName();          // +0x2C
    const std::string& kitName     = getTemplate()->getName();

    const std::string geometryName =
        soldierName.substr(0, soldierName.find('_') + 1) + "Kits";
    const std::string replacementName =
        soldierName.substr(0, soldierName.find('_') + 1)
        + kitName.substr(kitName.find('_') + 1);

    setGeometry(this, geometryName);
    if (const IObjectTemplate* const replacement =
            objectTemplateManager->getTemplate(replacementName))             // +0x24
    {
        // raw-typed as a KitTemplate purely because the name says so - the ELF reads +0x198 with
        // no class check at all. Keeping the cast honest by naming the assumption here.
        const int geom = static_cast<const KitTemplate*>(replacement)->m_defaultGeomIndex;
        m_defaultGeomIndex = geom;
        m_geomIndex        = geom;
    }

    if (player)
        setOwnerId(player->getId());                                         // Item's virtual, +0x188

    updateGeometry(true);

    if (getParent() == obj || getParent())
        std::abort();                                                        // as shipped
    obj->addChild(this);
    setRelativeTransformation(Mat4::getIdentity());
    disableItem();
}

// handleDrop - 0x8184D40, 179 B. The mirror, and it does NOT go through Item::handleDrop at all -
// no removeItem, no container. Clear flag bit 0, unparent, restore the template's default geometry,
// re-enable (which starts the time-to-live timer), disown.
// `obj` is dereferenced unguarded, and a kit that still has a parent after removeChild aborts.
void Kit::handleDrop(IPlayer* /*player*/, IObject* obj)
{
    updateFlags(0, 1);
    if (getParent())
    {
        obj->removeChild(this);
        if (getParent())
            std::abort();                                                    // as shipped
    }
    // the return value of the template's getGeometry() is discarded and setGeometry is called with
    // it - reproduced as the compiled pair; the string overload of setGeometry is empty in this
    // build anyway (see ObjectUtil).
    setGeometry(this, const_cast<KitTemplate*>(
                          static_cast<const KitTemplate*>(getTemplate()))->getGeometry());
    m_defaultGeomIndex = kitTemplate()->m_defaultGeomIndex;
    m_geomIndex        = m_defaultGeomIndex;
    updateGeometry(false);
    enableItem();
    setOwnerId(-1);
}

// Enabling a dropped kit starts its expiry timer; disabling cancels queued messages.
// Timer operations are server-only. Enabling while parented aborts.
bool Kit::enableItem()
{
    if (getParent())
        std::abort();                                                        // as shipped
    if (isEnabled())                                                         // Item's virtual
        return false;

    Item::enableItem();
    setGeomIndex(-1);
    const KitTemplate* const objectTemplate =
        static_cast<const KitTemplate*>(getTemplate());                      // +0x14 not +0x184
    if (game->queryInterface(IID_IGameServer))
        objectManager->postMessage(this, static_cast<TemplateMessage>(29),   // +0xD0
                                   objectTemplate->m_timeToLiveAfterDeath);
    return true;
}

void Kit::disableItem()
{
    Item::disableItem();
    if (game->queryInterface(IID_IGameServer))
        objectManager->removeAllMessagesTo(this);                            // +0xD4
}

// Server messages 24/29 destroy kits with remaining children; no base forwarding.
// Pickup must cancel these messages because carried kits may retain children.
void Kit::handleMessage(TemplateMessage message, IPlayer* /*player*/, bool /*remote*/)
{
    if (message != static_cast<TemplateMessage>(24) && message != static_cast<TemplateMessage>(29))
        return;
    if (!getChild())
        return;
    if (!game->queryInterface(IID_IGameServer))
        return;
    game->destroyObject(this);                                               // IGame +0xA0
}

// giveSoldierKitItems / addObjectToSoldier - 0x8185500 (102 B) / 0x8185570 (351 B).
// Every kit child that is neither an item container nor a mask is re-parented onto the soldier by
// re-running the item's own pickup against it.
// the sibling is cached before the call - addObjectToSoldier unlinks the node being walked.
void Kit::giveSoldierKitItems(IObject* soldier, IPlayer* player)
{
    for (IObject* child = getChild(); child; )
    {
        IObject* const next = child->getSibling();
        const unsigned int classId = child->getTemplate()->getClassID();
        if (classId != CID_ItemContainerTemplate && classId != CID_MaskObjectTemplate)
            addObjectToSoldier(soldier, player, child);
        child = next;
    }
}

void Kit::addObjectToSoldier(IObject* soldier, IPlayer* player, IObject* obj)
{
    if (!obj || !obj->queryInterface(IID_IItemObject))
    {
        Debug(DEBUGTYPE_6, "Game/Objects/Game/kit.cpp", 529) << "Unknown part in kit!";
        return;
    }
    // Preserve the interface query even though its result is unused.
    if (soldier)
        soldier->queryInterface(IID_IItemContainerObject);

    obj->getParent()->removeChild(obj);                                      // unchecked, as shipped
    IItemObject* const item =
        static_cast<IItemObject*>(obj->queryInterface(IID_IItemObject));     // queried a second time
    item->handlePickup(player, soldier);                                     // IItemObject +0x10
}

// initNetworkableComp - 0x81856D0, 371 B. A kit replicates through a kitnetworkable - the zero-bit
// wire class whose entire presence on the wire is the CreateKitEvent.
// AN empty networkableInfo aborts. Shipped, and unreachable on stock content: every kit template
// carries one. The info string is fetched twice, as compiled.
// (This body previously lived on RW-44's KitStandIn in KitTemplate.cpp; it moves here unchanged.)
void Kit::initNetworkableComp()
{
    const IObjectTemplate* const objectTemplate = getTemplate();             // +0x14
    if (!objectTemplate)
        return;
    if (objectTemplate->getNetworkableInfo().empty())
        std::abort();                                                        // as shipped
    if (!objectTemplate->getNetworkableInfo().empty()                        // fetch #2, as compiled
        && !setNetworkableComp(this, CID_KitNetworkable))
    {
        Debug(DEBUGTYPE_6, "Game/Objects/Game/kit.cpp", 544)
            << formatString("Failed to create KitNetworkable");
    }
}

// disableCachedObject - 0x8185880, 91 B. Going back into the object cache resets the unlock level
// to -1, the ability to its template default and the geometry to the template's, then re-runs the
// unlock decision so the recycled kit comes back as a virgin one.
// This is why a kit handed out by the cache is never stale, and why the -1 the CreateKitEvent
// carries holds for recycled kits too.
void Kit::disableCachedObject()
{
    m_chosenUnlockLevel = -1;
    const KitTemplate* const objectTemplate = m_kitTemplate;                 // +0x184
    m_currentAbility = (objectTemplate->m_abilityRestoreRate != -1.0f) ? 1.0f : -1.0f;
    m_geomIndex = objectTemplate->m_defaultGeomIndex;
    updateUnlockStatus();
    SimpleObject::disableCachedObject();
}

// Selects carried or dropped geometry, clearing the bone palette for dropped kits.
// Only the carried path resets an out-of-range stored geometry index.
void Kit::updateGeometry(bool pickedUp)
{
    void* const geometry = object_detail::LayoutAccess::geometry(this);      // +0x38
    if (!geometry)
        return;

    int wanted;
    if (pickedUp)
    {
        if (geometryGetGeomCount(geometry) <= m_geomIndex)
        {
            m_geomIndex = m_kitTemplate->m_defaultGeomIndex;
            return;
        }
        wanted = m_geomIndex;
    }
    else
    {
        // A dropped kit is no longer skinned to anyone: clear the bone palette before switching
        // to the drop geom. ELF `[vptr+0x58]` / PE `[vptr+0x54]`, both with (0, 0).
        geometrySetBoneTransforms(geometry, 0, 0u);
        if (geometryGetGeomCount(geometry) <= m_dropGeomIndex)
            return;
        wanted = m_dropGeomIndex;
    }
    if (geometryGetGeomIndex(geometry) != wanted)
        geometrySetGeomIndex(geometry, wanted);
}

// . -1 means "the default". Only a change re-runs the geometry, and the arm it runs
// is chosen by whether the kit is currently carried (has a parent).
void Kit::setGeomIndex(int index)
{
    const int previous = m_geomIndex;
    m_geomIndex = (index == -1) ? m_defaultGeomIndex : index;
    if (previous != m_geomIndex)
        updateGeometry(getParent() != 0);
}

// . Does any other child of this kit already claim `itemIndex`? init's duplicate
// assert is the only caller. walks the direct children only, not the whole subtree.
bool Kit::checkIfPartExists(IObject* skip, int itemIndex) const
{
    for (IObject* child = getChild(); child; child = child->getSibling())
    {
        if (child == skip)
            continue;
        if (IItemObject* const item =
                static_cast<IItemObject*>(child->queryInterface(IID_IItemObject)))
        {
            if (item->getItemIndex() == itemIndex)
                return true;
        }
    }
    return false;
}

// Restore rate -1.0f disables the kit ability meter.
void Kit::updateAbility()
{
    const KitTemplate* const objectTemplate =
        static_cast<const KitTemplate*>(getTemplate());
    if (objectTemplate->m_abilityRestoreRate != -1.0f)
    {
        float ability = objectTemplate->m_abilityUpdate + m_currentAbility;
        if (ability > 1.0f)
            ability = 1.0f;
        m_currentAbility = ability;
    }
}

float Kit::getAbilityUpdate() const
{
    return static_cast<const KitTemplate*>(getTemplate())->m_abilityUpdate;
}

float Kit::getCurrentAbility() const     { return m_currentAbility; }
void  Kit::setCurrentAbility(float a)    { m_currentAbility = a; }

// . Spend `cost` if there is enough; otherwise change nothing and answer false.
bool Kit::taxAbility(float cost)
{
    if (m_currentAbility < cost)
        return false;
    m_currentAbility -= cost;
    return true;
}

// through the kit's OWN template pointer, as compiled.
unsigned int Kit::getCreationIndex() const
{
    return static_cast<unsigned int>(m_kitTemplate->m_creationIndex);
}

// the address of the template's vehicle-mod container, not a copy.
std::vector<std::pair<std::string, VehicleMod> >* Kit::getKitVehicleMods()
{
    return &const_cast<KitTemplate*>(m_kitTemplate)->m_kitVehicleMods;
}

// a tail call into the template.
int Kit::getType() const
{
    return static_cast<const KitTemplate*>(getTemplate())->getKitType();
}

} // namespace world
} // namespace hfe
} // namespace dice
