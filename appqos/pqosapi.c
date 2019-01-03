#include <Python.h>
#include "pqos.h"

static PyObject *exception;

/**
 * @brief pqos_init libpqos call wrapper
 *
 * @param [in] self NULL
 * @param [in] args Python tuple object containing the arguments
 *
 * @return Python tuple object containing return value
 */
static PyObject *init(PyObject *self, PyObject *args)
{
    int ret;
    struct pqos_config cfg;

    memset(&cfg, 0, sizeof(cfg));
    cfg.interface = PQOS_INTER_OS;
    cfg.fd_log = STDOUT_FILENO;

    ret = pqos_init(&cfg);
    if (ret != PQOS_RETVAL_OK)
        PyErr_SetString(exception, "Unable to initialise PQoS library");

    return Py_BuildValue("i", ret);
}

/**
 * @brief pqos_fini libpqos call wrapper
 *
 * @param [in] self NULL
 * @param [in] args Python tuple object containing the arguments
 *
 * @return Python tuple object containing return value
 */
static PyObject *fini(PyObject *self, PyObject *args)
{
    int ret = pqos_fini();

    return Py_BuildValue("i", ret);
}

/**
 * @brief pqos_alloc_assoc_set libpqos call wrapper
 *
 * @param [in] self NULL
 * @param [in] args Python tuple object containing the arguments
 *
 * @return Python tuple object containing return value
 */
static PyObject *alloc_assoc_set(PyObject *self, PyObject *args)
{
    int ret;
    unsigned int core;
    unsigned int cos;

    if (!PyArg_ParseTuple(args, "II", &core, &cos))
        return Py_BuildValue("i", PQOS_RETVAL_PARAM);

    ret = pqos_alloc_assoc_set(core, cos);
    if (ret != PQOS_RETVAL_OK)
        PyErr_SetString(exception, "Failed to set Core Association");

    return Py_BuildValue("i", ret);
}

/**
 * @brief pqos_l3ca_set libpqos call wrapper
 *
 * @param [in] self NULL
 * @param [in] args Python tuple object containing the arguments
 *
 * @return Python tuple object containing return value
 */
static PyObject *l3ca_set(PyObject *self, PyObject *args)
{
    int ret;
    unsigned int socket;
    unsigned int cos;
    uint64_t ways_mask;
    struct pqos_l3ca ca;

    if (!PyArg_ParseTuple(args, "IIL", &socket, &cos, &ways_mask))
        return Py_BuildValue("i", PQOS_RETVAL_PARAM);

    ca.class_id = cos;
    ca.cdp = 0;
    ca.u.ways_mask = ways_mask;

    ret = pqos_l3ca_set(socket, 1, &ca);
    if (ret != PQOS_RETVAL_OK)
        PyErr_SetString(exception, "Failed to set COS CBM!");

    return Py_BuildValue("i", ret);
}

/**
 * @brief pqos_mba_set libpqos call wrapper
 *
 * @param [in] self NULL
 * @param [in] args Python tuple object containing the arguments
 *
 * @return Python tuple object containing return value
 */
static PyObject *mba_set(PyObject *self, PyObject *args)
{
    int ret;
    unsigned int socket;
    unsigned int cos;
    uint32_t max;
    struct pqos_mba mba;

    if (!PyArg_ParseTuple(args, "III", &socket, &cos, &max))
        return Py_BuildValue("i", PQOS_RETVAL_PARAM);

    mba.class_id = cos;
    mba.ctrl = 0;
    mba.mb_max = max;

    ret = pqos_mba_set(socket, 1, &mba, NULL);
    if (ret != PQOS_RETVAL_OK)
        PyErr_SetString(exception, "Failed to set MBA!");

    return Py_BuildValue("i", ret);
}

/**
 * @brief Python's "Module's Method Table".
 *        Bind Python function names to our C functions
 */
static PyMethodDef pqosapi_methods[] = {
    {"pqos_init", init, METH_VARARGS},
    {"pqos_fini", fini, METH_VARARGS},
    {"pqos_alloc_assoc_set", alloc_assoc_set, METH_VARARGS},
    {"pqos_l3ca_set", l3ca_set, METH_VARARGS},
    {"pqos_mba_set", mba_set, METH_VARARGS},
    {NULL, NULL}
};

/**
 * @brief Module’s initialisation function.
 *        Python calls this to let us initialise our module
 */
PyMODINIT_FUNC initpqosapi(void)
{
    PyObject *m;

    m = Py_InitModule("pqosapi", pqosapi_methods);
    exception = PyErr_NewException("pqosapi.error", NULL, NULL);
    Py_INCREF(exception);
    PyModule_AddObject(m, "pqosapi.error", exception);
}
