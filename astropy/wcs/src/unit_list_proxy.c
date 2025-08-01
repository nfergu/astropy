/*
 Author: Michael Droettboom
         mdroe@stsci.edu
*/

#define NO_IMPORT_ARRAY

#include <string.h> // strncmp
#include "astropy_wcs/pyutil.h"
#include "astropy_wcs/str_list_proxy.h"

/***************************************************************************
 * List-of-units proxy object
 ***************************************************************************/

#define MAXSIZE 68
#define ARRAYSIZE 72

static PyObject* PyUnitListProxyType;

typedef struct {
  PyObject_HEAD
  /*@null@*/ /*@shared@*/ PyObject* pyobject;
  Py_ssize_t size;
  char (*array)[ARRAYSIZE];
  PyObject* unit_class;
} PyUnitListProxy;

static void
PyUnitListProxy_dealloc(
    PyUnitListProxy* self) {

  PyObject_GC_UnTrack(self);
  Py_XDECREF(self->pyobject);
  PyTypeObject *tp = Py_TYPE((PyObject*)self);
  freefunc free_func = PyType_GetSlot(tp, Py_tp_free);
  free_func((PyObject*)self);
  Py_DECREF(tp);
}

/*@null@*/ static PyObject *
PyUnitListProxy_new(
    PyTypeObject* type,
    /*@unused@*/ PyObject* args,
    /*@unused@*/ PyObject* kwds) {

  PyUnitListProxy* self = NULL;

  allocfunc alloc_func = PyType_GetSlot(type, Py_tp_alloc);
  self = (PyUnitListProxy*)alloc_func(type, 0);
  if (self != NULL) {
    self->pyobject = NULL;
    self->unit_class = NULL;
  }
  return (PyObject*)self;
}

static int
PyUnitListProxy_traverse(
    PyUnitListProxy* self,
    visitproc visit,
    void *arg) {

  Py_VISIT(self->pyobject);
  Py_VISIT(self->unit_class);
  Py_VISIT((PyObject*)Py_TYPE((PyObject*)self));
  return 0;
}

static int
PyUnitListProxy_clear(
    PyUnitListProxy *self) {

  Py_CLEAR(self->pyobject);
  Py_CLEAR(self->unit_class);

  return 0;
}

/*@null@*/ PyObject *
PyUnitListProxy_New(
    /*@shared@*/ PyObject* owner,
    Py_ssize_t size,
    char (*array)[ARRAYSIZE]) {

  PyUnitListProxy* self = NULL;
  PyObject *units_module;
  PyObject *units_dict;
  PyObject *unit_class;

  units_module = PyImport_ImportModule("astropy.units");
  if (units_module == NULL) {
    return NULL;
  }

  units_dict = PyModule_GetDict(units_module);
  if (units_dict == NULL) {
    return NULL;
  }

  unit_class = PyDict_GetItemString(units_dict, "Unit");
  if (unit_class == NULL) {
    PyErr_SetString(PyExc_RuntimeError, "Could not import Unit class");
    return NULL;
  }

  Py_INCREF(unit_class);

  PyTypeObject* type = (PyTypeObject*)PyUnitListProxyType;
  allocfunc alloc_func = PyType_GetSlot(type, Py_tp_alloc);
  self = (PyUnitListProxy*)alloc_func(type, 0);
  if (self == NULL) {
    return NULL;
  }

  Py_XINCREF(owner);
  self->pyobject = owner;
  self->size = size;
  self->array = array;
  self->unit_class = unit_class;
  return (PyObject*)self;
}

static Py_ssize_t
PyUnitListProxy_len(
    PyUnitListProxy* self) {

  return self->size;
}

static PyObject*
_get_unit(
    PyObject *unit_class,
    PyObject *unit) {

  PyObject *args;
  PyObject *kw;
  PyObject *result;

  kw = Py_BuildValue("{s:s,s:s}", "format", "fits", "parse_strict", "warn");
  if (kw == NULL) {
      return NULL;
  }

  args = PyTuple_New(1);
  if (args == NULL) {
      Py_DECREF(kw);
      return NULL;
  }
  PyTuple_SetItem(args, 0, unit);
  Py_INCREF(unit);

  result = PyObject_Call(unit_class, args, kw);

  Py_DECREF(args);
  Py_DECREF(kw);
  return result;
}

/*@null@*/ static PyObject*
PyUnitListProxy_getitem(
    PyUnitListProxy* self,
    Py_ssize_t index) {

  PyObject *value;
  PyObject *result;

  if (index >= self->size || index < 0) {
    PyErr_SetString(PyExc_IndexError, "index out of range");
    return NULL;
  }

  value = PyUnicode_FromString(self->array[index]);

  result = _get_unit(self->unit_class, value);

  Py_DECREF(value);
  return result;
}

static PyObject*
PyUnitListProxy_richcmp(
	PyObject *a,
	PyObject *b,
	int op){
  PyUnitListProxy *lhs, *rhs;
  Py_ssize_t idx;
  int equal = 1;
  assert(a != NULL && b != NULL);
  if (!PyObject_TypeCheck(a, (PyTypeObject*)PyUnitListProxyType) ||
      !PyObject_TypeCheck(b, (PyTypeObject*)PyUnitListProxyType)) {
    Py_RETURN_NOTIMPLEMENTED;
  }
  if (op != Py_EQ && op != Py_NE) {
    Py_RETURN_NOTIMPLEMENTED;
  }

  /* The actual comparison of the two objects. unit_class is ignored because
   * it's not an essential property of the instances.
   */
  lhs = (PyUnitListProxy *)a;
  rhs = (PyUnitListProxy *)b;
  if (lhs->size != rhs->size) {
    equal = 0;
  }
  for (idx = 0; idx < lhs->size && equal == 1; idx++) {
    if (strncmp(lhs->array[idx], rhs->array[idx], ARRAYSIZE) != 0) {
      equal = 0;
    }
  }
  if ((op == Py_EQ && equal == 1) ||
      (op == Py_NE && equal == 0)) {
    Py_RETURN_TRUE;
  } else {
    Py_RETURN_FALSE;
  }
}

static int
PyUnitListProxy_setitem(
    PyUnitListProxy* self,
    Py_ssize_t index,
    PyObject* arg) {

  PyObject* value;
  PyObject* unicode_value;
  PyObject* bytes_value;

  if (index >= self->size || index < 0) {
    PyErr_SetString(PyExc_IndexError, "index out of range");
    return -1;
  }

  value = _get_unit(self->unit_class, arg);
  if (value == NULL) {
    return -1;
  }

  unicode_value = PyObject_CallMethod(value, "to_string", "s", "fits");
  if (unicode_value == NULL) {
    Py_DECREF(value);
    return -1;
  }
  Py_DECREF(value);

  if (PyUnicode_Check(unicode_value)) {
    bytes_value = PyUnicode_AsASCIIString(unicode_value);
    if (bytes_value == NULL) {
      Py_DECREF(unicode_value);
      return -1;
    }
    Py_DECREF(unicode_value);
  } else {
    bytes_value = unicode_value;
  }

  strncpy(self->array[index], PyBytes_AsString(bytes_value), MAXSIZE);
  Py_DECREF(bytes_value);

  return 0;
}

/*@null@*/ static PyObject*
PyUnitListProxy_repr(
    PyUnitListProxy* self) {

  return str_list_proxy_repr(self->array, self->size, MAXSIZE);
}

static PyType_Spec PyUnitListProxyType_spec = {
  .name = "astropy.wcs.UnitListProxy",
  .basicsize = sizeof(PyUnitListProxy),
  .itemsize = 0,
  .flags = Py_TPFLAGS_DEFAULT | Py_TPFLAGS_HAVE_GC | Py_TPFLAGS_IMMUTABLETYPE,
  .slots = (PyType_Slot[]){
    {Py_tp_dealloc, (destructor)PyUnitListProxy_dealloc},
    {Py_tp_repr, (reprfunc)PyUnitListProxy_repr},
    {Py_tp_str, (reprfunc)PyUnitListProxy_repr},
    {Py_tp_traverse, (traverseproc)PyUnitListProxy_traverse},
    {Py_tp_clear, (inquiry)PyUnitListProxy_clear},
    {Py_tp_richcompare, (richcmpfunc)PyUnitListProxy_richcmp},
    {Py_tp_new, (newfunc)PyUnitListProxy_new},
    {Py_sq_length, (lenfunc)PyUnitListProxy_len},
    {Py_sq_item, (ssizeargfunc)PyUnitListProxy_getitem},
    {Py_sq_ass_item, (ssizeobjargproc)PyUnitListProxy_setitem},
    {0, NULL},
  },
};

static PyObject* PyUnitListProxyType = NULL;

int
set_unit_list(
    PyObject* owner,
    const char* propname,
    PyObject* value,
    Py_ssize_t len,
    char (*dest)[ARRAYSIZE]) {

  PyObject*  unit  = NULL;
  PyObject*  proxy = NULL;
  Py_ssize_t i        = 0;

  if (check_delete(propname, value)) {
    return -1;
  }

  if (!PySequence_Check(value)) {
    PyErr_Format(
        PyExc_TypeError,
        "'%s' must be a sequence of strings",
        propname);
    return -1;
  }

  if (PySequence_Size(value) != len) {
    PyErr_Format(
        PyExc_ValueError,
        "len(%s) must be %u",
        propname,
        (unsigned int)len);
    return -1;
  }

  proxy = PyUnitListProxy_New(owner, len, dest);
  if (proxy == NULL) {
      return -1;
  }

  for (i = 0; i < len; ++i) {
    unit = PySequence_GetItem(value, i);
    if (unit == NULL) {
      Py_DECREF(proxy);
      return -1;
    }

    if (PySequence_SetItem(proxy, i, unit) == -1) {
      Py_DECREF(proxy);
      Py_DECREF(unit);
      return -1;
    }

    Py_DECREF(unit);
  }

  Py_DECREF(proxy);

  return 0;
}


int
_setup_unit_list_proxy_type(
    /*@unused@*/ PyObject* m) {

  PyUnitListProxyType = PyType_FromSpec(&PyUnitListProxyType_spec);
  if (PyUnitListProxyType == NULL) {
    return 1;
  }

  return 0;
}
