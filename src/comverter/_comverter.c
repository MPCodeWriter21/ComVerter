#define PY_SSIZE_T_CLEAN
#include "md_core.h"
#include <Python.h>

static void _comverter_parse_extensions(PyObject *extensions) {
    g_gfm_extensions = 0;
    if (!extensions) return;
    PyObject *seq = NULL;
    if (PyList_Check(extensions)) seq = extensions;
    else if (PyTuple_Check(extensions))
        seq = extensions;
    else
        return;
    Py_ssize_t n = PySequence_Length(seq);
    for (Py_ssize_t i = 0; i < n; i++) {
        PyObject *item = PySequence_GetItem(seq, i);
        if (PyUnicode_Check(item)) {
            const char *s = PyUnicode_AsUTF8(item);
            if (s && strcmp(s, "tagfilter") == 0)
                g_gfm_extensions |= GFM_ENABLE_TAGFILTER;
            if (s && strcmp(s, "autolink") == 0)
                g_gfm_extensions |= GFM_ENABLE_AUTOLINK;
        }
        Py_DECREF(item);
    }
}

static PyObject *_comverter_markdown_to_html(
    PyObject *module, PyObject *args, PyObject *kwargs
) {
    (void)module;
    const char *markdown;
    Py_ssize_t md_len;
    PyObject *extensions = NULL;

    static char *kwlist[] = {"markdown", "extensions", NULL};

    if (!PyArg_ParseTupleAndKeywords(
            args, kwargs, "s#|O", kwlist, &markdown, &md_len, &extensions
        ))
        return NULL;

    if (md_len < 0 || (markdown == NULL && md_len > 0)) {
        PyErr_SetString(
            PyExc_ValueError, "Invalid input: markdown text must be a valid string."
        );
        return NULL;
    }

    if (md_len == 0 || markdown == NULL) return PyUnicode_FromString("");

    _comverter_parse_extensions(extensions);

    char *result = markdown_to_html(markdown, (size_t)md_len);
    g_gfm_extensions = 0;
    if (!result) {
        PyErr_NoMemory();
        return NULL;
    }

    PyObject *py_result = PyUnicode_FromString(result);
    free(result);
    return py_result;
}

static PyObject *_comverter_markdown_file_to_html(
    PyObject *module, PyObject *args, PyObject *kwargs
) {
    (void)module;
    const char *input_path;
    const char *output_path;
    PyObject *extensions = NULL;

    static char *kwlist[] = {"input_file", "output_file", "extensions", NULL};

    if (!PyArg_ParseTupleAndKeywords(
            args, kwargs, "ss|O", kwlist, &input_path, &output_path, &extensions
        ))
        return NULL;

    FILE *fp = fopen(input_path, "rb");
    if (!fp) {
        PyErr_SetFromErrnoWithFilename(PyExc_FileNotFoundError, input_path);
        return NULL;
    }

    fseek(fp, 0, SEEK_END);
    long fsize = ftell(fp);
    if (fsize < 0) {
        fclose(fp);
        return PyErr_NoMemory();
    }
    fseek(fp, 0, SEEK_SET);

    char *markdown = malloc((size_t)fsize + 1);
    if (!markdown) {
        fclose(fp);
        return PyErr_NoMemory();
    }
    size_t bytes_read = fread(markdown, 1, (size_t)fsize, fp);
    fclose(fp);
    if ((long)bytes_read != fsize) {
        free(markdown);
        return PyErr_NoMemory();
    }
    markdown[fsize] = '\0';

    _comverter_parse_extensions(extensions);

    char *result = markdown_to_html(markdown, (size_t)fsize);
    g_gfm_extensions = 0;
    free(markdown);
    if (!result) return PyErr_NoMemory();

    fp = fopen(output_path, "wb");
    if (!fp) {
        free(result);
        PyErr_SetFromErrnoWithFilename(PyExc_OSError, output_path);
        return NULL;
    }
    size_t out_len = strlen(result);
    if (fwrite(result, 1, out_len, fp) != out_len) {
        fclose(fp);
        free(result);
        PyErr_SetFromErrnoWithFilename(PyExc_OSError, output_path);
        return NULL;
    }
    fclose(fp);
    free(result);

    Py_RETURN_NONE;
}

static PyMethodDef _comverter_methods[] = {
    {     "markdown_to_html",      (PyCFunction)_comverter_markdown_to_html,
     METH_VARARGS | METH_KEYWORDS,
     "Convert Markdown text to HTML. See comverter.markdown_to_html for full docs."                                             },
    {"markdown_file_to_html", (PyCFunction)_comverter_markdown_file_to_html,
     METH_VARARGS | METH_KEYWORDS,
     "Convert a Markdown file to HTML and write to an output file.\n"
     "\n"
     ":param input_file: Path to the input Markdown file.\n"
     ":type input_file: str\n"
     ":param output_file: Path where the HTML output is written.\n"
     ":type output_file: str\n"
     ":param extensions: GFM extensions to enable.\n"
     ":type extensions: list of str, optional\n"
     ":raises FileNotFoundError: If the input file does not exist.\n"
     ":raises OSError: If the output file cannot be written."                                                                   },
    {                   NULL,                                          NULL, 0,                                             NULL}
};

static PyModuleDef _comverter_module = {
    PyModuleDef_HEAD_INIT,
    .m_name = "_comverter",
    .m_doc = "ComVerter C extension. Provides markdown_to_html() and\n"
             "markdown_file_to_html() for CommonMark/GFM conversion.",
    .m_size = 0,
    .m_methods = _comverter_methods,
};

PyMODINIT_FUNC PyInit__comverter(void) { return PyModuleDef_Init(&_comverter_module); }
