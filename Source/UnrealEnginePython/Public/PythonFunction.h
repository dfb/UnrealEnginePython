#pragma once

#include "UnrealEnginePython.h"
#include "PythonFunction.generated.h"

UCLASS()
class UPythonFunction : public UFunction
{
	GENERATED_BODY()

public:
	~UPythonFunction();
	void SetPyCallable(PyObject *callable);

	DECLARE_FUNCTION(CallPythonCallable);

	PyObject *py_callable;
    bool use_proxy; // if true, instead of using py_obj for self, use py_obj->py_proxy
};

