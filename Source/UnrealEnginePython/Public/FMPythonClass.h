#pragma once

#include "UnrealEnginePython.h"
#include "FMPythonClass.generated.h"

UCLASS()
class UFMPythonClass : public UClass
{
	GENERATED_BODY()

public:

	void SetPyConstructor(PyObject *callable);
	void CallPyConstructor(ue_PyUObject *self);

	// __dict__ is stored here
	//ue_PyUObject *py_uobject;

    // the Python class this UE4 class binds to
    PyObject *pyClass;

private:

	PyObject * py_constructor;
};

