#include <Python.h>
#include <structmember.h>

#include <stdio.h>


#include <atrac3plus_pqf.h>

typedef struct {
    PyObject_HEAD;
    at3plus_pqf_a_ctx_t ctx;
} AnalyzeCtxObject;

static PyObject *
AnalyzeCtx_new(PyTypeObject *type, PyObject *args, PyObject *kwds)
{
    AnalyzeCtxObject *self;
    self = (AnalyzeCtxObject*)type->tp_alloc(type, 0);
    if (self == NULL) {
        fprintf(stderr, "Unable to allocate analyze context\n");
        return NULL;
    }
    return (PyObject*)self;
}

static int
AnalyzeCtx_init(AnalyzeCtxObject *self, PyObject *args, PyObject *kwds)
{
    self->ctx = at3plus_pqf_create_a_ctx();
    return 0;
}

static void
AnalyzeCtx_dealloc(AnalyzeCtxObject *self)
{
    at3plus_pqf_free_a_ctx(self->ctx);
}

static PyObject* prototype(PyObject* self, PyObject* unused)
{
    int i;
    const float* proto = at3plus_pqf_get_proto();

    PyObject *my_list = PyList_New(0);
    if(my_list == NULL) {
        return NULL;
    }

    for (i = 0; i < at3plus_pqf_proto_sz; i++) {
        PyObject *o = Py_BuildValue("f", proto[i]);
        if(PyList_Append(my_list, o) == -1) {
            return NULL;
        }
    }

    return my_list;
}

static PyObject* analyzectx_do(PyObject* self, PyObject* args)
{
    int i = 0;
    int len = 0;
    float* in;
    float* out;

    AnalyzeCtxObject* ctx;
    PyObject *seq;

    if (!PyArg_ParseTuple(args, "O", &seq)) {
        return NULL;
    }

    seq = PySequence_Fast(seq, "argument must be iterable");

    if (!seq) {
        return NULL;
    }

    len = PySequence_Fast_GET_SIZE(seq);

    if (len != at3plus_pqf_frame_sz) {
        fprintf(stderr, "wrong frame size, expected: %d, got: %d", (int)at3plus_pqf_frame_sz, len);
	return NULL;
    }

    in = (float*)malloc(len * sizeof(float));
    if(!in) {
        Py_DECREF(seq);
        return PyErr_NoMemory(  );
    }

    for (i = 0; i < len; i++) {
        PyObject *fitem;
        PyObject *item = PySequence_Fast_GET_ITEM(seq, i);
        if(!item) {
            Py_DECREF(seq);
            free(in);
            return NULL;
        }
        fitem = PyNumber_Float(item);
        if(!fitem) {
            Py_DECREF(seq);
            free(in);
            PyErr_SetString(PyExc_TypeError, "all items must be numbers");
            return NULL;
        }
        in[i] = PyFloat_AS_DOUBLE(fitem);
        Py_DECREF(fitem);
    }

    PyObject *my_list = PyList_New(0);
    if(my_list == NULL) {
        Py_DECREF(seq);
        free(in);
        return NULL;
    }

    out = (float*)calloc(at3plus_pqf_frame_sz, sizeof(float));
    if (out == NULL) {
        Py_DECREF(seq);
        free(in);
        return NULL;
    }

    ctx = (AnalyzeCtxObject*)self;

    at3plus_pqf_do_analyse(ctx->ctx, in, out);

    for (i = 0; i < len; i++) {
        PyObject *o = Py_BuildValue("f", out[i]);
        if(PyList_Append(my_list, o) == -1) {
            Py_DECREF(seq);
            free(in);
            return NULL;
        }
    }

    free(in);
    return my_list;
}

static PyMethodDef methods[] = {
    { "prototype", prototype, METH_NOARGS, "Show atrac3plus pqf filter prototype"},
    { NULL, NULL, 0, NULL }
};

static PyMethodDef AnalyzeCtx_methods[] = {
    {"do", (PyCFunction)analyzectx_do, METH_VARARGS, NULL},
    {NULL}
};

static PyTypeObject AnalyzeCtxType = {
    PyVarObject_HEAD_INIT(NULL, 0)
    .tp_name = "pqf.AnalyzeCtx",
    .tp_doc = "AnalyzeCtx",
    .tp_basicsize = sizeof(AnalyzeCtxObject),
    .tp_itemsize = 0,
    .tp_flags = Py_TPFLAGS_DEFAULT | Py_TPFLAGS_BASETYPE,
    .tp_new = AnalyzeCtx_new,
    .tp_init = (initproc)AnalyzeCtx_init,
    .tp_dealloc = (destructor)AnalyzeCtx_dealloc,
    .tp_methods = AnalyzeCtx_methods,
};

static struct PyModuleDef module = {
    PyModuleDef_HEAD_INIT,
    .m_name = "pqf",
    .m_doc = "pqf library test binding",
    .m_size = -1,
    .m_methods = methods
};

PyMODINIT_FUNC PyInit_pqf(void)
{
    PyObject *m;
    if (PyType_Ready(&AnalyzeCtxType) < 0)
        return NULL;

    m = PyModule_Create(&module);
    if (m == NULL)
        return NULL;

    Py_INCREF(&AnalyzeCtxType);
    PyModule_AddObject(m, "AnalyzeCtx", (PyObject*)&AnalyzeCtxType);
    PyModule_AddIntConstant(m, "frame_sz", at3plus_pqf_frame_sz); 
    return m;
}
