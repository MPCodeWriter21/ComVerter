#define PY_SSIZE_T_CLEAN
#include <Python.h>

static PyObject *_comverter_hello_world(PyObject *module, PyObject *args)
{
    (void)module;
    if (!PyArg_ParseTuple(args, ""))
        return NULL;
    printf("Hello World!\n");
    Py_RETURN_NONE;
}

static PyMethodDef _comverter_methods[] = {
    {"hello_world", _comverter_hello_world, METH_VARARGS,
     "Print \"Hello World!\" to standard output."             },
    {         NULL,                   NULL,            0, NULL}
};

static PyModuleDef _comverter_module = {
    PyModuleDef_HEAD_INIT,
    .m_name = "_comverter",
    .m_doc = "ComVerter internal C extension module.",
    .m_size = 0,
    .m_methods = _comverter_methods,
};

PyMODINIT_FUNC PyInit__comverter(void) { return PyModuleDef_Init(&_comverter_module); }
