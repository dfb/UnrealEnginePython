#include "FMPythonClass.h"

void UFMPythonClass::SetPyConstructor(PyObject *callable)
{
    py_constructor = callable;
    Py_INCREF(py_constructor);
}

void UFMPythonClass::CallPyConstructor(ue_PyUObject *self)
{
    if (!py_constructor)
        return;
    PyObject *ret = PyObject_CallObject(py_constructor, Py_BuildValue("(O)", self));
    if (!ret)
    {
        unreal_engine_py_log_error();
        return;
    }
    Py_DECREF(ret);
}


