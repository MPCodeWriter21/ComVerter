#define PY_SSIZE_T_CLEAN
#include <Python.h>
#include "md_core.h"

static PyObject *_comverter_markdown_to_html(PyObject *module, PyObject *args, PyObject *kwargs) {
    (void)module;
    const char *markdown;
    Py_ssize_t md_len;
    PyObject *extensions = NULL;

    static char *kwlist[] = {"markdown", "extensions", NULL};

    if (!PyArg_ParseTupleAndKeywords(args, kwargs, "s#|O", kwlist, &markdown, &md_len, &extensions))
        return NULL;

    if (md_len < 0 || (markdown == NULL && md_len > 0)) {
        PyErr_SetString(PyExc_ValueError, "Invalid input: markdown text must be a valid string.");
        return NULL;
    }

    if (md_len == 0 || markdown == NULL) return PyUnicode_FromString("");

    g_gfm_extensions = 0;
    if (extensions && PyList_Check(extensions)) {
        Py_ssize_t n = PyList_Size(extensions);
        for (Py_ssize_t i = 0; i < n; i++) {
            PyObject *item = PyList_GetItem(extensions, i);
            if (PyUnicode_Check(item)) {
                const char *s = PyUnicode_AsUTF8(item);
                if (s && strcmp(s, "tagfilter") == 0)
                    g_gfm_extensions |= GFM_ENABLE_TAGFILTER;
                if (s && strcmp(s, "autolink") == 0)
                    g_gfm_extensions |= GFM_ENABLE_AUTOLINK;
            }
        }
    } else if (extensions && PyTuple_Check(extensions)) {
        Py_ssize_t n = PyTuple_Size(extensions);
        for (Py_ssize_t i = 0; i < n; i++) {
            PyObject *item = PyTuple_GetItem(extensions, i);
            if (PyUnicode_Check(item)) {
                const char *s = PyUnicode_AsUTF8(item);
                if (s && strcmp(s, "tagfilter") == 0)
                    g_gfm_extensions |= GFM_ENABLE_TAGFILTER;
                if (s && strcmp(s, "autolink") == 0)
                    g_gfm_extensions |= GFM_ENABLE_AUTOLINK;
            }
        }
    }

    char *result = markdown_to_html(markdown, (size_t)md_len);
    g_gfm_extensions = 0;
    if (!result) { PyErr_NoMemory(); return NULL; }

    PyObject *py_result = PyUnicode_FromString(result);
    free(result);
    return py_result;
}

static PyMethodDef _comverter_methods[] = {
    {"markdown_to_html", (PyCFunction)_comverter_markdown_to_html, METH_VARARGS | METH_KEYWORDS,
     "Convert Markdown text to HTML.\n\n"
     "Args:\n"
     "    markdown_text (str): The Markdown string to convert.\n"
     "    extensions (list, optional): List of GFM extensions to enable (e.g. [\"tagfilter\"]).\n"
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
