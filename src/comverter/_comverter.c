#define PY_SSIZE_T_CLEAN
#include <Python.h>
#include "md_core.h"

static PyObject *_comverter_markdown_to_html(PyObject *module, PyObject *args) {
    (void)module;
    const char *markdown;
    Py_ssize_t md_len;

    if (!PyArg_ParseTuple(args, "s#", &markdown, &md_len)) return NULL;

    if (md_len < 0 || (markdown == NULL && md_len > 0)) {
        PyErr_SetString(PyExc_ValueError, "Invalid input: markdown text must be a valid string.");
        return NULL;
    }

    if (md_len == 0 || markdown == NULL) return PyUnicode_FromString("");

    char *result = markdown_to_html(markdown, (size_t)md_len);
    if (!result) { PyErr_NoMemory(); return NULL; }

    PyObject *py_result = PyUnicode_FromString(result);
    free(result);
    return py_result;
}

static PyMethodDef _comverter_methods[] = {
    {"markdown_to_html", _comverter_markdown_to_html, METH_VARARGS,
     "Convert Markdown text to HTML.\n\n"
     "Args:\n"
     "    markdown_text (str): The Markdown string to convert.\n"
     "Returns:\n"
     "    str: The resulting HTML string.\n"
     "Raises:\n"
     "    TypeError: If the input is not a string.\n"
     "    ValueError: If the input is invalid."},
    {NULL, NULL, 0, NULL}
};

static PyModuleDef _comverter_module = {
    PyModuleDef_HEAD_INIT,
    .m_name = "_comverter",
    .m_doc = "ComVerter internal C extension module for format conversion.",
    .m_size = 0,
    .m_methods = _comverter_methods,
};

PyMODINIT_FUNC PyInit__comverter(void) { return PyModuleDef_Init(&_comverter_module); }
