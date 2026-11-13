// Python argument unpacking helpers. Return 0 on success and 1 on failure, with a
// Python exception set; callers then return null. [INFERRED] Header placement.

#ifndef GAME_PYTHON_PY_PYARG_H                       // [INFERRED] guard style
#define GAME_PYTHON_PY_PYARG_H

#include <string>

#include "../../../Math/Vec3.h"

#include "../PyRef.h"                          // pulls in Python.h, as Objects.cpp does

namespace dice {
namespace hfe {

namespace world { class IObject; }

namespace python {

struct ObjectToken;   // struct, matching py/Objects.cpp's definition:
                      //   MSVC encodes the class-key in the mangled name (V vs U)

// the overload set. definitions ARE IN py/Objects.cpp - see the note above.
int parse_arg(PyObject* o, int& out);
int parse_arg(PyObject* o, double& out);
int parse_arg(PyObject* o, std::string& out);
int parse_arg(PyObject* o, PyObject*& out);
int parse_arg(PyObject* o, Vec3& out);
int parse_arg(PyObject* o, ObjectToken*& out);
// [CHOSEN, not recovered] Declared for PythonHost.cpp's object-taking host methods.
int parse_arg(PyObject* o, world::IObject*& out);
void* tokenControlBlock(ObjectToken* token);   // py/Objects.cpp - trig_create's retain

// Stops unpacking when a Python error is already pending, allowing callers to check after the last argument.
template <class T>
int py_arg(PyObject* args, int index, T& out)
{
    if (PyErr_Occurred())
        return 1;                    // sticky - see above

    if (!args)
    {
        PyErr_SetString(PyExc_RuntimeError, "no arguments");
        return 1;
    }

    PyObject* item = PyTuple_GetItem(args, index);
    if (!item)
        return 1;                    // PyTuple_GetItem already raised IndexError

    return parse_arg(item, out);
}

} // namespace python
} // namespace hfe
} // namespace dice

#endif // GAME_PYTHON_PY_PYARG_H
