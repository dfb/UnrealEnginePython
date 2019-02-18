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


UObject* UFMPythonClass::CreateDefaultObject()
{
    bool savedLoad = GIsInitialLoad;

    // this is a total hack but be need to go around the default
    // package loading logic when loading default objects
    GIsInitialLoad = false;
    UObject *ret = Super::CreateDefaultObject();
    GIsInitialLoad = savedLoad;

    return ret;
}
