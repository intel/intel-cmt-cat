/*
 * BSD LICENSE
 *
 * Copyright(c) 2019 Intel Corporation. All rights reserved.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 *   * Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 *   * Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in
 *     the documentation and/or other materials provided with the
 *     distribution.
 *   * Neither the name of Intel Corporation nor the names of its
 *     contributors may be used to endorse or promote products derived
 *     from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 */

#include <Python.h>
#include <linux/perf_event.h>
#include <sys/ioctl.h>
#include <sys/types.h>
#include <sys/syscall.h>

/**
 * @brief perf_event_open syscall wrapper
 *
 * @param [in] self NULL
 * @param [in] args Python tuple object containing the arguments
 *
 * @return Python tuple object containing return value
 */
static PyObject *py_perfEventOpen(PyObject *self, PyObject *args)
{
    int fd;
    pid_t pid = -1;
    int cpu = -1;
    int group_fd = -1;
    unsigned long flags = 0;
    uint32_t type;
    uint64_t config;
    struct perf_event_attr attrs;

    if (!PyArg_ParseTuple(args, "llliil", &type, &config, &pid, &cpu, &group_fd,
                          &flags))
        return Py_BuildValue("i", -1);

    memset(&attrs, 0, sizeof(struct perf_event_attr));
    attrs.type = type;
    attrs.size = sizeof(struct perf_event_attr);
    attrs.config = config;
    attrs.disabled = 1;
    /* Set read_format so it will be possible to tell if multiplexing happens */
    attrs.read_format = PERF_FORMAT_TOTAL_TIME_ENABLED |
                        PERF_FORMAT_TOTAL_TIME_RUNNING | PERF_FORMAT_ID;

    fd = syscall(__NR_perf_event_open, &attrs, pid, cpu, group_fd, flags);
    if (fd != -1)
        ioctl(fd, PERF_EVENT_IOC_RESET, 0);

    return Py_BuildValue("i", fd);
}

/**
 * @brief Python's "Module's Method Table".
 *        Bind Python function names to our C functions
 */
static PyMethodDef myModule_methods[] = {
        {"perf_event_open", py_perfEventOpen, METH_VARARGS},
        {NULL, NULL}
};

/**
 * @brief Module’s initialisation function.
 *        Python calls this to let us initialise our module
 */
PyMODINIT_FUNC initperfapi(void)
{
        (void) Py_InitModule("perfapi", myModule_methods);
}
