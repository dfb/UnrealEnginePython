#include "UnrealEnginePython.h"
#include "PythonFunction.h"
#include "UEPyModule.h"
#include "FMPythonClass.h"
#if WITH_EDITOR
#include "Editor/BlueprintGraph/Public/BlueprintActionDatabase.h"
#endif


#define LOG(format, ...) UE_LOG(LogPython, Log, TEXT("[%s:%d] %s"), TEXT(__FUNCTION__), __LINE__, *FString::Printf(TEXT(format), ##__VA_ARGS__ ))
#define LWARN(format, ...) UE_LOG(LogPython, Warning, TEXT("[%s:%d] %s"), TEXT(__FUNCTION__), __LINE__, *FString::Printf(TEXT(format), ##__VA_ARGS__ ))
#define LERROR(format, ...) UE_LOG(LogPython, Error, TEXT("[%s:%d] %s"), TEXT(__FUNCTION__), __LINE__, *FString::Printf(TEXT(format), ##__VA_ARGS__ ))

#define VALID(obj) (obj != nullptr && obj->IsValidLowLevel())
#define USEFUL(obj) (VALID(obj) && !obj->IsPendingKill())

// creates a new UClass class inside UE4
// basically unreal_engine_new_uclass but uses a new class type in UE4
UClass *create_new_uclass(char *name, UClass *parent_class)
{
    bool is_overwriting = false;

    UObject *outer = GetTransientPackage();
    UClass *parent = UObject::StaticClass();
    if (parent_class)
    {
        parent = parent_class;
        outer = parent->GetOuter();
    }

    UClass *new_object = FindObject<UClass>(ANY_PACKAGE, UTF8_TO_TCHAR(name));
    if (!new_object)
    {
        new_object = NewObject<UFMPythonClass>(outer, UTF8_TO_TCHAR(name), RF_Public | RF_Transient | RF_MarkAsNative);
        if (!new_object)
            return nullptr;
    }
    else
    {
        UE_LOG(LogPython, Warning, TEXT("Preparing for overwriting class %s ..."), UTF8_TO_TCHAR(name));
        is_overwriting = true;
    }

    if (is_overwriting && new_object->Children)
    {
        UField *u_field = new_object->Children;
        while (u_field)
        {
            if (u_field->IsA<UFunction>())
            {
                UE_LOG(LogPython, Warning, TEXT("removing function %s"), *u_field->GetName());
                new_object->RemoveFunctionFromFunctionMap((UFunction *)u_field);
                FLinkerLoad::InvalidateExport(u_field);
            }
            u_field = u_field->Next;
        }
        new_object->ClearFunctionMapsCaches();
        new_object->PurgeClass(true);
        new_object->Children = nullptr;
        new_object->ClassAddReferencedObjects = parent->ClassAddReferencedObjects;
    }

    new_object->PropertiesSize = 0;

    new_object->ClassConstructor = parent->ClassConstructor;
    new_object->SetSuperStruct(parent);

    new_object->PropertyLink = parent->PropertyLink;
    new_object->ClassWithin = parent->ClassWithin;
    new_object->ClassConfigName = parent->ClassConfigName;

    new_object->ClassFlags |= (parent->ClassFlags & (CLASS_Inherit | CLASS_ScriptInherit));
    new_object->ClassFlags |= CLASS_Native;

#if WITH_EDITOR
    new_object->SetMetaData(FBlueprintMetadata::MD_AllowableBlueprintVariableType, TEXT("true"));
    if (new_object->IsChildOf<UActorComponent>())
    {
        new_object->SetMetaData(FBlueprintMetadata::MD_BlueprintSpawnableComponent, TEXT("true"));
    }
#endif

    new_object->ClassCastFlags = parent->ClassCastFlags;


    new_object->Bind();
    new_object->StaticLink(true);

    // it could be a class update
    if (is_overwriting && new_object->ClassDefaultObject)
    {
        new_object->GetDefaultObject()->RemoveFromRoot();
        new_object->GetDefaultObject()->ConditionalBeginDestroy();
        new_object->ClassDefaultObject = nullptr;
    }

#if WITH_EDITOR
    new_object->PostEditChange();
#endif

    new_object->GetDefaultObject()->PostInitProperties();

#if WITH_EDITOR
    new_object->PostLinkerChange();
#endif

    new_object->AssembleReferenceTokenStream();

#if WITH_EDITOR
    // this is required for avoiding startup crashes #405
    if (GEditor)
    {
        FBlueprintActionDatabase::Get().RefreshClassActions(new_object);
    }
#endif

    return new_object;
}

// hack for avoiding loops in class constructors (thanks to the Unreal.js project for the idea)
static UClass *ue_py_class_constructor_placeholder = nullptr;
static void UEPyClassConstructor(UClass *u_class, const FObjectInitializer &ObjectInitializer)
{
	if (UFMPythonClass *u_py_class_casted = Cast<UFMPythonClass>(u_class))
	{
		ue_py_class_constructor_placeholder = u_class;
	}
	u_class->ClassConstructor(ObjectInitializer);
	ue_py_class_constructor_placeholder = nullptr;
}


// creates and returns a new UFMPythonClass
static PyObject *create_subclass(PyObject *self, PyObject *args)
{
    char *className;
    PyObject *pyParentClass, *pyClass;
    if (!PyArg_ParseTuple(args, "sOO", &className, &pyParentClass, &pyClass))
        return NULL;

    UClass *parentClass = ue_py_check_type<UClass>(pyParentClass);
    if (!parentClass)
        return PyErr_Format(PyExc_Exception, "parent class must be a UClass");

    if (!PyObject_IsInstance(pyClass, (PyObject *)&PyType_Type))
        return PyErr_Format(PyExc_Exception, "python class must be a class object");

    // Create the class in UE4 and store info needed to create Python instances later
    UFMPythonClass *newClass = (UFMPythonClass *)create_new_uclass(className, parentClass);
    if (!newClass)
        return PyErr_Format(PyExc_Exception, "failed to create new UClass");
    Py_INCREF(pyClass);
    newClass->pyClass = pyClass;

    // TODO: any uprop the py class is declaring that is not already present in a parent class needs to be added to newClass
    // TODO: any ufunc the py class is declaring that is not already present in a parent class needs to be added to newClass
    // TODO: add to newClass a UFUNCTION for ReceiveDestroy (I guess?) that decrefs and clears our ref to the py instance we created
    // TODO: for each ufunc the py class is declaring, add a UFUNC to the new class that is a shim that gets the PyObj prop from the instance (see
    //  the constructor below) and calls the provided py callable

    // set up a constructor for the new class that instantiates the python class and calls its init
    newClass->ClassConstructor = [](const FObjectInitializer& objInitializer)
    {
        FScopePythonGIL gil;

        // do some hackery to prevent infinite recursion
        UClass *u_class = ue_py_class_constructor_placeholder ? ue_py_class_constructor_placeholder : objInitializer.GetClass();
        ue_py_class_constructor_placeholder = nullptr;
        UEPyClassConstructor(u_class->GetSuperClass(), objInitializer);

        UFMPythonClass *fmClass = Cast<UFMPythonClass>(u_class);
		if (!fmClass || !fmClass->pyClass)
			return; // nothing left to do for e.g. CDO

		// create and bind an instance of the Python class. By convention, the bridge class takes a single param that tells
        // the address of the corresponding UObject
        UObject *engineObj = objInitializer.GetObj();
        //LOG("ClassConstructor initializing UObject %llX", (unsigned long long)engineObj);
        PyObject *initArgs = Py_BuildValue("(K)", (unsigned long long)engineObj);
        PyObject *pyInst = PyObject_CallObject(fmClass->pyClass, initArgs);
        Py_DECREF(initArgs);
        if (!pyInst)
        {
            LERROR("Failed to instantiate Python class");
            return;
        }

        ue_PyUObject *pyObj = ue_get_python_uobject(engineObj);
        if (!pyObj)
        {
            unreal_engine_py_log_error();
            return;
        }

        pyObj->py_proxy = pyInst;
        // TODO: I think we want pyObj->py_dict and pyInst's dict to be the same
        // TODO: pyObj->creating = true?

        // inject any UPROPERTYs into pyObj.__dict__?
        // inject any UFUNCTIONs into pyobj.__dict__ too?
    };

    // set up the CDO of this class and verify that it will auto trigger a call to python's init

    Py_RETURN_UOBJECT(newClass);
}

// given a UFunction and a UObject instance, calls that function on that instance
static PyObject *call_ufunction_object(PyObject *self, PyObject *args)
{
    unsigned long long instAddr;
    PyObject *pyInst;
    PyObject *pyFuncObj;
    if (!PyArg_ParseTuple(args, "KOO", &instAddr, &pyInst, &pyFuncObj))
        return NULL;

    UObject *engineObj = (UObject *)instAddr;
    //LOG("CALL UFUNCTION on UObject %llX", instAddr);
    if (!USEFUL(engineObj))
    {
        LERROR("engineObj not useful");
        return NULL;
    }

    UFunction *engineFunc = ue_py_check_type<UFunction>(pyFuncObj);
    if (!engineFunc)
        return PyErr_Format(PyExc_Exception, "Invalid UFunction object");

    PyObject *pyArgs = PyTuple_New(0);
    PyObject *ret = py_ue_ufunction_call(engineFunc, engineObj, pyArgs, 0, NULL);
    Py_DECREF(pyArgs);
    return ret;
}

// given a UObject that was instantiated via a bridge class, return its python proxy object
static PyObject *get_py_proxy(PyObject *self, PyObject *args)
{
    PyObject *pyEngineObj;
    if (!PyArg_ParseTuple(args, "O", &pyEngineObj))
        return NULL;

    UObject *engineObj = ue_py_check_type<UObject>(pyEngineObj);
    if (!engineObj)
        return PyErr_Format(PyExc_Exception, "That object is not a UObject");

    ue_PyUObject *pyObj = ue_get_python_uobject(engineObj);
    if (!pyObj)
        return PyErr_Format(PyExc_Exception, "No py obj for that UObject");

    Py_INCREF(pyObj->py_proxy);
    return pyObj->py_proxy;
}

// adds a UFUNCTION to a UClass that either calls a Python callable or the superclass UFUNCTION
static PyObject *add_ufunction(PyObject *self, PyObject *args)
{
    PyObject *pyEngineClass;
    char *funcName;
    PyObject *pyFunc;
    if (!PyArg_ParseTuple(args, "OsO", &pyEngineClass, &funcName, &pyFunc))
        return NULL;

    UClass *engineClass = ue_py_check_type<UClass>(pyEngineClass);
    if (!engineClass)
        return PyErr_Format(PyExc_Exception, "Provide the UClass to attach the function to");

    // If no Python function is given, it means direct the call to the super call
    // TODO: remove this, I don't think we use it because it doesn't work :)
    if (pyFunc == Py_None)
        engineClass = engineClass->GetSuperClass();

    // TODO: compute correct set of function flags
    uint32 funcFlags = FUNC_Native | FUNC_BlueprintCallable | FUNC_Public;
    UPythonFunction *newFunc = (UPythonFunction *)unreal_engine_add_function(engineClass, funcName, pyFunc, funcFlags);
    if (newFunc)
    {
        newFunc->use_proxy = true; // hack: we store 'self' in py_proxy
        return Py_BuildValue("");
    }
    else
        return PyErr_Format(PyExc_Exception, "Failed to add a UFUNCTION to a callable");
}

// gets a list of all UFunctions exposed by a UClass and its parents
static PyObject *get_ufunction_names(PyObject *self, PyObject *args)
{
    PyObject *pyEngineClass;
    if (!PyArg_ParseTuple(args, "O", &pyEngineClass))
        return NULL;

    UClass *engineClass = ue_py_check_type<UClass>(pyEngineClass);
    if (!engineClass)
        return PyErr_Format(PyExc_Exception, "Invalid UClass");

	PyObject *funcNames = PyList_New(0);
    for (TFieldIterator<UFunction> it(engineClass); it; ++it)
    {
        UFunction* func = *it;
        FString funcName = func->GetFName().ToString();
        //LOG("PARENT CLASS HAS: %s", *func->GetFName().ToString());
		PyList_Append(funcNames, PyUnicode_FromString(TCHAR_TO_UTF8(*func->GetFName().ToString())));
    }

    return funcNames;
}

// looks up and returns a UFunction object on a UClass
static PyObject *get_ufunction_object(PyObject *self, PyObject *args)
{
    PyObject *pyEngineClass;
    char *funcName;
    if (!PyArg_ParseTuple(args, "Os", &pyEngineClass, &funcName))
        return NULL;

    UClass *engineClass = ue_py_check_type<UClass>(pyEngineClass);
    if (!engineClass)
        return PyErr_Format(PyExc_Exception, "Provide the UClass to attach the function to");

    UFunction *engineFunc = engineClass->FindFunctionByName(UTF8_TO_TCHAR(funcName));
    if (!engineFunc)
        return PyErr_Format(PyExc_Exception, "Function does not exist on that class");

    Py_RETURN_UOBJECT(engineFunc);
}

static PyMethodDef module_methods[] = {
    {"create_subclass", create_subclass, METH_VARARGS, ""},
    {"call_ufunction_object", call_ufunction_object, METH_VARARGS, ""},
    {"get_py_proxy", get_py_proxy, METH_VARARGS, ""},
    {"add_ufunction", add_ufunction, METH_VARARGS, ""},
    {"get_ufunction_names", get_ufunction_names, METH_VARARGS, ""},
    {"get_ufunction_object", get_ufunction_object, METH_VARARGS, ""},
    { NULL, NULL },
};

// creates and sets up the special 'fm' module in the interpreter
void fm_init_module()
{
    //LOG("Initializing module");
	GAllowActorScriptExecutionInEditor = true; // without this, UFUNCTION calls in the editor often don't work - maybe that's intentional?
    PyObject *module = PyImport_AddModule("fm");
    PyObject *module_dict = PyModule_GetDict(module);

    for (PyMethodDef *m = module_methods; m->ml_name != NULL; m++)
    {
        PyObject *func = PyCFunction_New(m, NULL);
        PyDict_SetItemString(module_dict, m->ml_name, func);
        Py_DECREF(func);
    }
}
