/*
 * @brief: This module provides functions that are used by project
 *         reality's python modules and required an optimization.
 * @note: Make sure you test anything that you want to add and keep track of
 *        object reference counts. Refer to Doc/data/refcounts.dat.
 *        Refer to Misc/README.valgrind to compile python for valgrind and
 *        run the tests again with the following string:
 *        valgrind --log-file="logfile.out" --track-origins=yes
 * --show-leak-kinds=all \
 *          --trace-children=yes --leak-check=full --tool=memcheck \
 *          --suppressions=Misc/valgrind-python.supp ./python
 * {TEST_PATH}/test_realitycore.py Look for the string realitycore in the output
 * file and fix whatever causes it to appear there.
 */

#include "Python.h"

#define _USE_MATH_DEFINES
#include "float.h"
#include "math.h"

/* OpenBF2: Python.h includes math.h before _USE_MATH_DEFINES, so MSVC never sees M_PI. */
#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

/* OpenBF2: the 2.3.4 headers predate both. A 32-bit Py_ssize_t also matches PR's prebuilt 2.7. */
#if PY_VERSION_HEX < 0x02050000
typedef int Py_ssize_t;
#endif
#ifndef Py_RETURN_TRUE
#define Py_RETURN_TRUE return Py_INCREF(Py_True), Py_True
#define Py_RETURN_FALSE return Py_INCREF(Py_False), Py_False
#endif

/* ANSI C doesn't support fmin/fmax */
/* OpenBF2: MSVC 2013 added them to math.h. */
#if defined(_MSC_VER) && _MSC_VER < 1800
#define INLINE __inline

INLINE double fmax(double left, double right)
{
    return (left > right) ? left : right;
}

INLINE double fmin(double left, double right)
{
    return (left < right) ? left : right;
}
#endif

/* For clean coordinates unpacking */
#define X_INDEX 0
#define Y_INDEX 1
#define Z_INDEX 2

static PyObject *realitycore_calcDistance(PyObject *self, PyObject *args)
{
    PyObject *x1 = NULL, *x2 = NULL, *y1 = NULL, *y2 = NULL, *z1 = NULL,
             *z2 = NULL;
    double dx, dy, dz, r;

    if (!PyArg_ParseTuple(args, "(OOO)(OOO):calcDistance", &x1, &y1, &z1, &x2,
                          &y2, &z2))
    {
        return NULL;
    }

    dx = PyFloat_AsDouble(x1) - PyFloat_AsDouble(x2);
    dy = PyFloat_AsDouble(y1) - PyFloat_AsDouble(y2);
    dz = PyFloat_AsDouble(z1) - PyFloat_AsDouble(z2);

    r = sqrt((dx * dx) + (dy * dy) + (dz * dz));

    return PyFloat_FromDouble(r);
}

PyDoc_STRVAR(realitycore_calcDistance_doc,
             "calcDistance(pos1, pos2)\n"
             "pos1, pos2 - a tuple containing 3 floats\n"
             "Return the distance between two points.");

static PyObject *realitycore_calcDistanceSquared(PyObject *self, PyObject *args)
{
    PyObject *x1 = NULL, *x2 = NULL, *y1 = NULL, *y2 = NULL, *z1 = NULL,
             *z2 = NULL;
    double dx, dy, dz, r;

    if (!PyArg_ParseTuple(args, "(OOO)(OOO):calcDistanceSquared", &x1, &y1, &z1,
                          &x2, &y2, &z2))
    {
        return NULL;
    }

    dx = PyFloat_AsDouble(x1) - PyFloat_AsDouble(x2);
    dy = PyFloat_AsDouble(y1) - PyFloat_AsDouble(y2);
    dz = PyFloat_AsDouble(z1) - PyFloat_AsDouble(z2);

    r = (dx * dx) + (dy * dy) + (dz * dz);

    return PyFloat_FromDouble(r);
}

PyDoc_STRVAR(realitycore_calcDistanceSquared_doc,
             "calcDistanceSquared(pos1, pos2)\n"
             "pos1, pos2 - a tuple containing 3 floats\n"
             "Returns the squared distance between two points.");

static PyObject *realitycore_calcHorizDistance(PyObject *self, PyObject *args)
{
    PyObject *x1 = NULL, *x2 = NULL, *y1 = NULL, *y2 = NULL, *z1 = NULL,
             *z2 = NULL;
    double dx, dz, r;

    if (!PyArg_ParseTuple(args, "(OOO)(OOO):calcHorizDistance", &x1, &y1, &z1,
                          &x2, &y2, &z2))
    {
        return NULL;
    }

    dx = PyFloat_AsDouble(x1) - PyFloat_AsDouble(x2);
    dz = PyFloat_AsDouble(z1) - PyFloat_AsDouble(z2);

    r = sqrt((dx * dx) + (dz * dz));

    return PyFloat_FromDouble(r);
}

PyDoc_STRVAR(realitycore_calcHorizDistance_doc,
             "calcHorizDistance(pos1, pos2)\n"
             "pos1, pos2 - a tuple containing 3 floats\n"
             "Returns the horizontal distance between two points.");

static PyObject *realitycore_calcHorizDistanceSquared(PyObject *self,
                                                      PyObject *args)
{
    PyObject *x1 = NULL, *x2 = NULL, *y1 = NULL, *y2 = NULL, *z1 = NULL,
             *z2 = NULL;
    double dx, dz, r;

    if (!PyArg_ParseTuple(args, "(OOO)(OOO):calcHorizDistanceSquared", &x1, &y1,
                          &z1, &x2, &y2, &z2))
    {
        return NULL;
    }

    dx = PyFloat_AsDouble(x1) - PyFloat_AsDouble(x2);
    dz = PyFloat_AsDouble(z1) - PyFloat_AsDouble(z2);

    r = (dx * dx) + (dz * dz);

    return PyFloat_FromDouble(r);
}

PyDoc_STRVAR(realitycore_calcHorizDistanceSquared_doc,
             "calcHorizDistanceSquared(pos1, pos2)\n"
             "pos1, pos2 - tuples containing 3 floats\n"
             "Returns the squared horizontal distance between two points.");

static PyObject *realitycore_calcPosFromPosRot(PyObject *self, PyObject *args)
{
    double posx, posy, posz, angle;
    PyObject *distance = NULL, *x = NULL, *y = NULL, *z = NULL, *r = NULL,
             *xrot = NULL, *yrot = NULL, *zrot = NULL;

    if (!PyArg_ParseTuple(args, "(OOO)(OOO)O:calcPosFromPosRot", &x, &y, &z,
                          &xrot, &yrot, &zrot, &distance))
    {
        return NULL;
    }

    angle = PyFloat_AsDouble(xrot) * M_PI / 180.0;

    posx = PyFloat_AsDouble(x) + sin(angle) * PyFloat_AsDouble(distance);
    posz = PyFloat_AsDouble(z) + cos(angle) * PyFloat_AsDouble(distance);
    posy = PyFloat_AsDouble(y);

    r = PyTuple_New(3);
    if (NULL == r)
    {
        return NULL;
    }

    /* INCREF for the members isn't required since the tuple is packed with
     * doubles */
    PyTuple_SetItem(r, 0, PyFloat_FromDouble(posx));
    PyTuple_SetItem(r, 1, PyFloat_FromDouble(posy));
    PyTuple_SetItem(r, 2, PyFloat_FromDouble(posz));

    return r;
}

PyDoc_STRVAR(realitycore_calcPosFromPosRot_doc,
             "calcPosFromPosRot(pos1, rot1, distance)\n"
             "pos1 tuple3\n"
             "rot1 tuple3\n"
             "distance float\n"
             "return tuple3\n"
             "Returns a new position based on postion, rotation and distance.\n"
             "NOTE: C-api code calling this function must DECREF the returned\n"
             "\tobject if it's the last one using it");

static PyObject *realitycore_isClose(PyObject *self, PyObject *args)
{
    PyObject *pos1 = NULL, *pos2 = NULL, *distance = NULL, *pos_args = NULL,
             *calc_distance = NULL;

    if (!PyArg_UnpackTuple(args, "isClose", 3, 3, &pos1, &pos2, &distance))
    {
        return NULL;
    }

    pos_args = PyTuple_New(2);
    if (NULL == pos_args)
    {
        return NULL;
    }

    /* We have to increase the reference count for pos1&2 since PyTuple_New
     *  barrows their reference and they get removed with him by the GC */
    Py_INCREF(pos1);
    Py_INCREF(pos2);
    PyTuple_SetItem(pos_args, 0, pos1);
    PyTuple_SetItem(pos_args, 1, pos2);

    calc_distance = realitycore_calcDistanceSquared(self, pos_args);
    if (pow(PyFloat_AsDouble(distance), 2) >= PyFloat_AsDouble(calc_distance))
    {
        Py_DECREF(calc_distance);
        Py_DECREF(pos_args);
        Py_RETURN_TRUE;
    }
    else
    {
        Py_DECREF(calc_distance);
        Py_DECREF(pos_args);
        Py_RETURN_FALSE;
    }
}

PyDoc_STRVAR(realitycore_isClose_doc,
             "isClose(pos1, pos2, distance)\n"
             "pos1 tuple3\n"
             "pos2 tuple3\n"
             "distance float\n"
             "Checks if pos2 is within the distance of pos1");

static PyObject *realitycore_isCloseHoriz(PyObject *self, PyObject *args)
{
    PyObject *pos1 = NULL, *pos2 = NULL, *distance = NULL, *pos_args = NULL,
             *calc_distance = NULL;

    if (!PyArg_UnpackTuple(args, "isCloseHoriz", 3, 3, &pos1, &pos2, &distance))
    {
        return NULL;
    }

    pos_args = PyTuple_New(2);
    if (NULL == pos_args)
    {
        return NULL;
    }

    /* We have to increase the reference count for pos1&2 since PyTuple_New
     *  borrows their reference and they get removed with him by the GC */
    Py_INCREF(pos1);
    Py_INCREF(pos2);
    PyTuple_SetItem(pos_args, 0, pos1);
    PyTuple_SetItem(pos_args, 1, pos2);

    calc_distance = realitycore_calcHorizDistanceSquared(self, pos_args);
    if (pow(PyFloat_AsDouble(distance), 2) >= PyFloat_AsDouble(calc_distance))
    {
        Py_DECREF(calc_distance);
        Py_DECREF(pos_args);
        Py_RETURN_TRUE;
    }
    else
    {
        Py_DECREF(calc_distance);
        Py_DECREF(pos_args);
        Py_RETURN_FALSE;
    }
}

PyDoc_STRVAR(realitycore_isCloseHoriz_doc,
             "isCloseHoriz(pos1, pos2, distance)\n"
             "pos1 tuple3\n"
             "pos2 tuple3\n"
             "distance float\n"
             "Checks if pos2 is within the distance of pos1 horizontally");

static PyObject *realitycore_isPointInPolygon(PyObject *self, PyObject *args)
{
    PyObject *pos = NULL, *polygon = NULL;
    Py_ssize_t num_vertices = 0;
    double vert1_x, vert1_y, vert2_x, vert2_y, pos_x, pos_y;
    /* This is used to indicate if the number of edges the ray crosses is odd or
     * even. At each crossing, the ray switches between inside and outside so
     * you can just have it flip from zero to one over and over and check the
     * final value when exiting. This is called the Jordan curve theorem. */
    int odd_num_edges = 0;
    Py_ssize_t i, j; /* Required to be done separately because the windows
                        version uses ANSI C */

    if (!PyArg_UnpackTuple(args, "isPointInPolygon", 2, 2, &pos, &polygon))
    {
        return NULL;
    }

    pos_x = PyFloat_AsDouble(PyTuple_GetItem(pos, X_INDEX));
    pos_y = PyFloat_AsDouble(PyTuple_GetItem(pos, Z_INDEX));
    num_vertices = PyList_Size(polygon);

    for (i = 0, j = num_vertices - 1; i < num_vertices; j = i++)
    {
        vert1_x = PyFloat_AsDouble(
            PyTuple_GetItem(PyList_GetItem(polygon, i), X_INDEX));
        vert1_y = PyFloat_AsDouble(
            PyTuple_GetItem(PyList_GetItem(polygon, i), Y_INDEX));
        vert2_x = PyFloat_AsDouble(
            PyTuple_GetItem(PyList_GetItem(polygon, j), X_INDEX));
        vert2_y = PyFloat_AsDouble(
            PyTuple_GetItem(PyList_GetItem(polygon, j), Y_INDEX));

        if (((vert1_y > pos_y) != (vert2_y > pos_y)) &&
            (pos_x <
             ((vert2_x - vert1_x) * (pos_y - vert1_y) / (vert2_y - vert1_y) +
              vert1_x)))
        {
            odd_num_edges = !odd_num_edges;
        }
    }

    if (odd_num_edges)
    {
        Py_RETURN_TRUE;
    }
    else
    {
        Py_RETURN_FALSE;
    }
}

PyDoc_STRVAR(realitycore_isPointInPolygon_doc,
             "isPointInPolygon(pos, polygon)\n"
             "pos tuple3\n"
             "polygon listn of tuple2\n"
             "Checks if pos is inside the given polygon. Based on the W. "
             "Randolph Franklin method.");

/* Internal function for getting the shortest distance from a line between two
 * points to a point. */
static double REALITYCORE__nearestPointDistance(double x,
                                                double y,
                                                double x1,
                                                double y1,
                                                double x2,
                                                double y2)
{
    double a = x - x1;
    double b = y - y1;
    double c = x2 - x1;
    double d = y2 - y1;

    /* The dot product divided by length squared gives you the projection
     * distance from (x1, y1). This is the fraction of the line that the point
     * (x,y) is closest to */
    double dot = a * c + b * d;
    double square_len = c * c + d * d;
    double proj_dist = -1;
    double xx, yy;

    if (0 != square_len)
    {
        proj_dist = dot / square_len;
    }

    if (0 > proj_dist)
    {
        xx = x1;
        yy = y1;
    }
    else if (1 < proj_dist)
    {
        xx = x2;
        yy = y2;
    }
    else
    {
        /* This the projection of the point (x,y) onto the segment
         * (x1,y1)-(x2,y2) */
        xx = x1 + proj_dist * c;
        yy = y1 + proj_dist * d;
    }

    /* sqrt of dx and dy to the point */
    return sqrt(pow(x - xx, 2) + pow(y - yy, 2));
}

static PyObject *realitycore_calcDistanceToPolygon(PyObject *self,
                                                   PyObject *args)
{
    PyObject *pos = NULL, *polygon = NULL;
    Py_ssize_t num_vertices = 0;
    double vert1_x, vert1_y, vert2_x, vert2_y, pos_x, pos_y;
    double min_distance = DBL_MAX;
    Py_ssize_t i;

    if (!PyArg_UnpackTuple(args, "calcDistanceToPolygon", 2, 2, &pos, &polygon))
    {
        return NULL;
    }

    pos_x = PyFloat_AsDouble(PyTuple_GetItem(pos, X_INDEX));
    pos_y = PyFloat_AsDouble(PyTuple_GetItem(pos, Z_INDEX));
    num_vertices = PyList_Size(polygon);

    for (i = 0; i < num_vertices; i++)
    {
        vert1_x = PyFloat_AsDouble(
            PyTuple_GetItem(PyList_GetItem(polygon, i), X_INDEX));
        vert1_y = PyFloat_AsDouble(
            PyTuple_GetItem(PyList_GetItem(polygon, i), Y_INDEX));

        /* Connect the last vert with the first vert when i reaches the last
         * index */
        vert2_x = PyFloat_AsDouble(PyTuple_GetItem(
            PyList_GetItem(polygon, ((i + 1) % num_vertices)), X_INDEX));
        vert2_y = PyFloat_AsDouble(PyTuple_GetItem(
            PyList_GetItem(polygon, ((i + 1) % num_vertices)), Y_INDEX));

        min_distance = fmin(min_distance, REALITYCORE__nearestPointDistance(
                                              pos_x, pos_y, vert1_x, vert1_y,
                                              vert2_x, vert2_y));
    }

    return PyFloat_FromDouble(min_distance);
}

PyDoc_STRVAR(
    realitycore_calcDistanceToPolygon_doc,
    "calcDistanceToPolygon(pos, polygon)\n"
    "pos tuple3\n"
    "polygon listn of tuple2\n"
    "Calculates the distance from a given position to a given polygon. "
    "Negative distance is returned if it's from within the polygon");

static PyMethodDef realitycore_methods[] = {
    {"calcDistance",             realitycore_calcDistance,             METH_VARARGS, realitycore_calcDistance_doc},
    {"calcDistanceSquared",      realitycore_calcDistanceSquared,      METH_VARARGS, realitycore_calcDistanceSquared_doc},
    {"calcHorizDistance",        realitycore_calcHorizDistance,        METH_VARARGS, realitycore_calcHorizDistance_doc},
    {"calcHorizDistanceSquared", realitycore_calcHorizDistanceSquared, METH_VARARGS, realitycore_calcHorizDistanceSquared_doc},
    {"isClose",                  realitycore_isClose,                  METH_VARARGS, realitycore_isClose_doc},
    {"isCloseHoriz",             realitycore_isCloseHoriz,             METH_VARARGS, realitycore_isCloseHoriz_doc},
    {"calcPosFromPosRot",        realitycore_calcPosFromPosRot,        METH_VARARGS, realitycore_calcPosFromPosRot_doc},
    {"isPointInPolygon",         realitycore_isPointInPolygon,         METH_VARARGS, realitycore_isPointInPolygon_doc},
    {"calcDistanceToPolygon",    realitycore_calcDistanceToPolygon,    METH_VARARGS, realitycore_calcDistanceToPolygon_doc},
    {NULL,      NULL} /* Sentinel */
};

PyDoc_STRVAR(realitycore_module_doc,
             "This module provides core functions used by project\n"
             "reality's python modules that required optimizations.");

PyMODINIT_FUNC init_realitycore(void)
{
    PyObject *m;

    m = Py_InitModule3("_realitycore", realitycore_methods,
                       realitycore_module_doc);
    if (m == NULL)
    {
        goto finally;
    }

finally:
    return;
}
