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
    cfg.interface = PQOS_INTER_MSR;
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
 * @brief pqos_cap_get libpqos call wrapper to get CAT support status
 *
 * @param [in] self NULL
 *
 * @return Python tuple object containing return value
 */
static PyObject *cat_supported(PyObject *self, PyObject *unused)
{
    int ret;
    const struct pqos_cap *p_cap = NULL;
    const struct pqos_capability *p_cap_l3ca;

    /* Get capability pointer */
    ret = pqos_cap_get(&p_cap, NULL);
    if (ret != PQOS_RETVAL_OK) {
        PyErr_SetString(exception, "Error retrieving PQoS capabilities!");
        return NULL;
    }

    /* Get L3CA capabilities */
    (void)pqos_cap_get_type(p_cap, PQOS_CAP_TYPE_L3CA, &p_cap_l3ca);

    return Py_BuildValue("i", (int)(p_cap_l3ca != NULL));
}

/**
 * @brief pqos_cap_get libpqos call wrapper to get MBA support status
 *
 * @param [in] self NULL
 *
 * @return Python tuple object containing return value
 */
static PyObject *mba_supported(PyObject *self, PyObject *unused)
{
    int ret;
    const struct pqos_cap *p_cap = NULL;
    const struct pqos_capability *p_cap_mba;
    int supported = 1;

    /* Get capability pointer */
    ret = pqos_cap_get(&p_cap, NULL);
    if (ret != PQOS_RETVAL_OK || p_cap == NULL) {
        PyErr_SetString(exception, "Error retrieving PQoS capabilities!");
        return NULL;
    }

    /* Get MBA capabilities */
    (void)pqos_cap_get_type(p_cap, PQOS_CAP_TYPE_MBA, &p_cap_mba);

    return Py_BuildValue("i", (int)(p_cap_mba != NULL));
}

/**
 * @brief pqos_cap_get libpqos call wrapper to get number of cores in the sys
 *
 * @param [in] self NULL
 *
 * @return Python tuple object containing return value
 */
static PyObject *num_cores(PyObject *self, PyObject *unused)
{
    int ret;
    const struct pqos_cpuinfo *p_cpu = NULL;

    /* Get capability pointer */
    ret = pqos_cap_get(NULL, &p_cpu);
    if (ret != PQOS_RETVAL_OK || p_cpu == NULL) {
        PyErr_SetString(exception, "Error retrieving CPU topology!");
        return NULL;
    }

    return Py_BuildValue("i", p_cpu->num_cores);
}

/**
 * @brief  pqos_cpu_get_sockets libpqos call wrapper to get sockets list
 *
 * @param [in] self NULL
 *
 * @return Python tuple object containing return value
 */
static PyObject *cpu_get_sockets(PyObject *self, PyObject *unused)
{
    int ret;
    const struct pqos_cpuinfo *p_cpu = NULL;
    unsigned int sock_count = 0;
    unsigned int *p_sockets = NULL;
    PyObject *sockets = NULL;
    unsigned int i;

    /* Get capability pointer */
    ret = pqos_cap_get(NULL, &p_cpu);
    if (ret != PQOS_RETVAL_OK || p_cpu == NULL) {
        PyErr_SetString(exception, "Error retrieving CPU topology!");
        return NULL;
    }

    /* Get socket ids */
    p_sockets = pqos_cpu_get_sockets(p_cpu, &sock_count);
    if (p_sockets == NULL) {
        PyErr_SetString(exception, "Error retrieving socket ids!");
        return NULL;
    }

    sockets = PyList_New(sock_count);

    for (i = 0; i < sock_count; i++)
        PyList_SetItem(sockets, i, Py_BuildValue("I", p_sockets[i]));

    free(p_sockets);

    return sockets;
}


/**
 * @brief pqos_l3ca_get_cos_num libpqos call wrapper
 * to get number of COSes for L3 CAT
 *
 * @param [in] self NULL
 *
 * @return Python tuple object containing return value
 */
static PyObject *l3ca_num_cos(PyObject *self, PyObject *unused)
{
    int ret;
    const struct pqos_cap *p_cap = NULL;
    unsigned int l3ca_cos_num = 0;

    /* Get capability pointer */
    ret = pqos_cap_get(&p_cap, NULL);
    if (ret != PQOS_RETVAL_OK || p_cap == NULL) {
        PyErr_SetString(exception, "Error retrieving PQoS capabilities!");
        return NULL;
    }

    /* Get COS num for L3 CAT  */
    ret = pqos_l3ca_get_cos_num(p_cap, &l3ca_cos_num);
    if (ret != PQOS_RETVAL_OK && ret != PQOS_RETVAL_RESOURCE) {
        PyErr_SetString(exception, "Error retrieving num of COS for L3 CAT!");
        return NULL;
    }

    return Py_BuildValue("I", l3ca_cos_num);
}


/**
 * @brief pqos_mba_get_cos_num libpqos call wrapper
 * to get number of COSes for MBA
 *
 * @param [in] self NULL
 *
 * @return Python tuple object containing return value
 */
static PyObject *mba_num_cos(PyObject *self, PyObject *unused)
{
    int ret;
    const struct pqos_cap *p_cap = NULL;
    unsigned int mba_cos_num = 0;

    /* Get capability pointer */
    ret = pqos_cap_get(&p_cap, NULL);
    if (ret != PQOS_RETVAL_OK || p_cap == NULL) {
        PyErr_SetString(exception, "Error retrieving PQoS capabilities!");
        return NULL;
    }

    /* Get COS num for MBA  */
    ret = pqos_mba_get_cos_num(p_cap, &mba_cos_num);
    if (ret != PQOS_RETVAL_OK && ret != PQOS_RETVAL_RESOURCE) {
        PyErr_SetString(exception, "Error retrieving num of COS for MBA!");
        return NULL;
    }

    return Py_BuildValue("I", mba_cos_num);
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
    {"pqos_is_mba_supported", mba_supported, METH_NOARGS},
    {"pqos_is_cat_supported", cat_supported, METH_NOARGS},
    {"pqos_get_num_cores", num_cores, METH_NOARGS},
    {"pqos_cpu_get_sockets", cpu_get_sockets, METH_NOARGS},
    {"pqos_get_l3ca_num_cos", l3ca_num_cos, METH_NOARGS},
    {"pqos_get_mba_num_cos", mba_num_cos, METH_NOARGS},
    {NULL, NULL}
};


#if PY_MAJOR_VERSION >= 3
static struct PyModuleDef pqosapi = {
    PyModuleDef_HEAD_INIT,
    "pqosapi",
    "wrapper for pqos library",
    -1,
    pqosapi_methods
};

/**
 * @brief Module’s initialisation function.
 *        Python calls this to let us initialise our module
 */
PyMODINIT_FUNC PyInit_pqosapi(void)
{
    PyObject *m;

    m = PyModule_Create(&pqosapi);
    if (m == NULL)
        return m;

    exception = PyErr_NewException("pqosapi.error", NULL, NULL);
    Py_INCREF(exception);
    PyModule_AddObject(m, "pqosapi.error", exception);

    return m;
}

#else
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
#endif
