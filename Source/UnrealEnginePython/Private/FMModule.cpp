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
UClass *create_new_uclass(char *name, UClass *parent_class, PyObject *pyClass)
{
    bool is_overwriting = false;

    UObject *outer = GetTransientPackage();
    UClass *parent = UObject::StaticClass();
    if (parent_class)
    {
        parent = parent_class;
        outer = parent->GetOuter();
    }

    UClass *newClass = FindObject<UClass>(ANY_PACKAGE, UTF8_TO_TCHAR(name));
    if (!newClass)
    {
        newClass = NewObject<UFMPythonClass>(outer, UTF8_TO_TCHAR(name), RF_Public | RF_Transient | RF_MarkAsNative);
        if (!newClass)
            return nullptr;
        Py_INCREF(pyClass);
        ((UFMPythonClass *)newClass)->pyClass = pyClass;
    }
    else
    {
        //UE_LOG(LogPython, Warning, TEXT("Preparing for overwriting class %s ..."), UTF8_TO_TCHAR(name));
        is_overwriting = true;
    }
    ((UFMPythonClass *)newClass)->creating = true; // this will be cleared later by the caller

    if (is_overwriting && newClass->Children)
    {
        UField *u_field = newClass->Children;
        while (u_field)
        {
            if (u_field->IsA<UFunction>())
            {
                newClass->RemoveFunctionFromFunctionMap((UFunction *)u_field);
                FLinkerLoad::InvalidateExport(u_field);
            }
            u_field = u_field->Next;
        }
        newClass->ClearFunctionMapsCaches();
        newClass->PurgeClass(true);
        newClass->Children = nullptr;
        newClass->ClassAddReferencedObjects = parent->ClassAddReferencedObjects;
    }

    newClass->PropertiesSize = 0;

    newClass->ClassConstructor = parent->ClassConstructor;
    newClass->SetSuperStruct(parent);

    newClass->PropertyLink = parent->PropertyLink;
    newClass->ClassWithin = parent->ClassWithin;
    newClass->ClassConfigName = parent->ClassConfigName;

    newClass->ClassFlags |= (parent->ClassFlags & (CLASS_Inherit | CLASS_ScriptInherit));
    newClass->ClassFlags |= CLASS_Native;

#if WITH_EDITOR
    newClass->SetMetaData(FBlueprintMetadata::MD_AllowableBlueprintVariableType, TEXT("true"));
    if (newClass->IsChildOf<UActorComponent>())
    {
        newClass->SetMetaData(FBlueprintMetadata::MD_BlueprintSpawnableComponent, TEXT("true"));
    }
#endif

    newClass->ClassCastFlags = parent->ClassCastFlags;


    newClass->Bind();
    newClass->StaticLink(true);

    // it could be a class update
    if (is_overwriting && newClass->ClassDefaultObject)
    {
        newClass->GetDefaultObject()->RemoveFromRoot();
        newClass->GetDefaultObject()->ConditionalBeginDestroy();
        newClass->ClassDefaultObject = nullptr;
    }

#if WITH_EDITOR
    newClass->PostEditChange();
#endif

    newClass->GetDefaultObject()->PostInitProperties();

#if WITH_EDITOR
    newClass->PostLinkerChange();
#endif

    newClass->AssembleReferenceTokenStream();

#if WITH_EDITOR
    // this is required for avoiding startup crashes #405
    if (GEditor)
    {
        FBlueprintActionDatabase::Get().RefreshClassActions(newClass);
    }
#endif

    return newClass;
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
    UFMPythonClass *newClass = (UFMPythonClass *)create_new_uclass(className, parentClass, pyClass);
    if (!newClass)
        return PyErr_Format(PyExc_Exception, "failed to create new UClass");

    // TODO: add to newClass a UFUNCTION for ReceiveDestroy (I guess?) that decrefs and clears our ref to the py instance we created

    // set up a constructor for the new class that instantiates the python class and calls its init
    newClass->ClassConstructor = [](const FObjectInitializer& objInitializer)
    {
        FScopePythonGIL gil;

        // do some hackery to prevent infinite recursion
        UClass *u_class = ue_py_class_constructor_placeholder ? ue_py_class_constructor_placeholder : objInitializer.GetClass();
        ue_py_class_constructor_placeholder = nullptr;
        UEPyClassConstructor(u_class->GetSuperClass(), objInitializer);

        // we want to create the Python object only for the actual object we're constructing, but ClassConstructor is called recursively as
        // we go up the acenstor chain of super classes, so guard against creating a Python object at other times
        UObject *engineObj = objInitializer.GetObj();
        if (u_class == engineObj->GetClass())
        {
            ue_PyUObject *pyObj = ue_get_python_uobject(engineObj);
            if (!pyObj)
            {
                unreal_engine_py_log_error();
                return;
            }

            UFMPythonClass *fmClass = Cast<UFMPythonClass>(u_class);
            if (!fmClass || !fmClass->pyClass)
            {
                LOG("EARLYEND newClass->ClassConstructor for u_class %s - no fmClass or no pyClass", *u_class->GetName());
                return; // TODO: does this ever happen? If so, isn't it an error?
            }

            // We don't want to create the CDO until after the Python metaclass has fully created the new class (Metabase.__new__ has
            // finished running). Because of the way object initialization in UE4 works, however, the CDO does get created here, but
            // it isn't fully formed (doesn't have its uprops yet), so below we manually kill it so that on a subsequent use it will
            // be recreated. Anyway, even though we can't prevent the CDO from being created here, we do want to prevent creation of
            // a corresponding Python instance here, just because it will trigger an error in the logs.
            if (fmClass->creating)
                pyObj->py_proxy = nullptr;
            else
            {
                // create and bind an instance of the Python class. By convention, the bridge class takes a single param that tells
                // the address of the corresponding UObject
                PyObject *initArgs = Py_BuildValue("(K)", (unsigned long long)engineObj);
                PyObject *pyInst = PyObject_CallObject(fmClass->pyClass, initArgs);
                Py_DECREF(initArgs);
                if (!pyInst)
                {
                    LERROR("failed to instantiate python class");
                    PyErr_Format(PyExc_Exception, "Failed to instantiate python clsas");
                    return;
                }
                pyObj->py_proxy = pyInst;
            }
        }

        // TODO: I think we want pyObj->py_dict and pyInst's dict to be the same?
    };

    // we want to force the CDO to be recreated next time it is accessed, so that the Python code has a chance to add in any uprops
    newClass->GetDefaultObject()->RemoveFromRoot();
    newClass->GetDefaultObject()->ConditionalBeginDestroy();
    newClass->ClassDefaultObject = nullptr;
    ((UFMPythonClass*)newClass)->creating = false;

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

// adds a UFUNCTION to a UClass that calls a Python callable
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

// gets a list of all UPROPERTYs exposed by a UClass and its parents
static PyObject *get_uproperty_names(PyObject *self, PyObject *args)
{
    PyObject *pyEngineClass;
    if (!PyArg_ParseTuple(args, "O", &pyEngineClass))
        return NULL;

    UClass *engineClass = ue_py_check_type<UClass>(pyEngineClass);
    if (!engineClass)
        return PyErr_Format(PyExc_Exception, "Invalid UClass");

	PyObject *propNames = PyList_New(0);
    for (TFieldIterator<UProperty> it(engineClass); it; ++it)
    {
        UProperty *prop = *it;
        FString propName = prop->GetFName().ToString();
		PyList_Append(propNames, PyUnicode_FromString(TCHAR_TO_UTF8(*prop->GetFName().ToString())));
    }

    return propNames;
}

// registers a new UProperty with the given UClass. Caller ensures the property does not already
// exist in this or an ancestor class
static PyObject *add_uproperty(PyObject *self, PyObject *args)
{
    PyObject *pyEngineClass, *pyPropType;
    char *propName;
    int isClass;
    if (!PyArg_ParseTuple(args, "OsOp", &pyEngineClass, &propName, &pyPropType, &isClass))
        return NULL;

    UClass *engineClass = ue_py_check_type<UClass>(pyEngineClass);
    if (!engineClass)
        return PyErr_Format(PyExc_Exception, "Invalid UClass");

	EObjectFlags propFlags = RF_Public | RF_MarkAsNative; // | RF_Transient;

    // Native types
    UProperty *newProp = nullptr;
    if (PyType_Check(pyPropType))
    {
        if (pyPropType == (PyObject *)&PyLong_Type)
            newProp = NewObject<UIntProperty>(engineClass, UTF8_TO_TCHAR(propName), propFlags);
        else if (pyPropType == (PyObject *)&PyBool_Type)
            newProp = NewObject<UBoolProperty>(engineClass, UTF8_TO_TCHAR(propName), propFlags);
        else if (pyPropType == (PyObject *)&PyFloat_Type)
            newProp = NewObject<UFloatProperty>(engineClass, UTF8_TO_TCHAR(propName), propFlags);
        else if (pyPropType == (PyObject *)&PyUnicode_Type)
            newProp = NewObject<UStrProperty>(engineClass, UTF8_TO_TCHAR(propName), propFlags);
        else if (pyPropType == (PyObject *)&ue_PyFVectorType)
        {
            newProp = NewObject<UStructProperty>(engineClass, UTF8_TO_TCHAR(propName), propFlags);
            ((UStructProperty*)newProp)->Struct = TBaseStructure<FVector>::Get();
        }
        else if (pyPropType == (PyObject *)&ue_PyFRotatorType)
        {
            newProp = NewObject<UStructProperty>(engineClass, UTF8_TO_TCHAR(propName), propFlags);
            ((UStructProperty*)newProp)->Struct = TBaseStructure<FRotator>::Get();
        }
        else if (pyPropType == (PyObject *)&ue_PyFTransformType)
        {
            newProp = NewObject<UStructProperty>(engineClass, UTF8_TO_TCHAR(propName), propFlags);
            ((UStructProperty*)newProp)->Struct = TBaseStructure<FTransform>::Get();
        }
        else if (pyPropType == (PyObject *)&ue_PyFLinearColorType)
        {
            newProp = NewObject<UStructProperty>(engineClass, UTF8_TO_TCHAR(propName), propFlags);
            ((UStructProperty*)newProp)->Struct = TBaseStructure<FLinearColor>::Get();
        }
        else if (pyPropType == (PyObject *)&ue_PyFColorType)
        {
            newProp = NewObject<UStructProperty>(engineClass, UTF8_TO_TCHAR(propName), propFlags);
            ((UStructProperty*)newProp)->Struct = TBaseStructure<FColor>::Get();
        }
        else
            return PyErr_Format(PyExc_Exception, "Cannot create uprop for %s - unsupported Python type", propName);
    }
    else if (ue_is_pyuobject(pyPropType))
    {
        UObject *engineObj = ((ue_PyUObject*)pyPropType)->ue_object;
        if (engineObj->IsA<UClass>())
        {
            UClass *uClass = (UClass *)engineObj;
            if (isClass) //uClass->IsChildOf<UClass>())
            {
                UClassProperty *propTemp = NewObject<UClassProperty>(engineClass, UTF8_TO_TCHAR(propName), propFlags);
                propTemp->PropertyClass = UClass::StaticClass();
                if (uClass == UClass::StaticClass())
                    propTemp->SetMetaClass(UObject::StaticClass());
                else
                    propTemp->SetMetaClass(uClass->GetClass());
                newProp = propTemp;
            }
            else
            {
                newProp = NewObject<UObjectProperty>(engineClass, UTF8_TO_TCHAR(propName), propFlags);
                ((UObjectProperty*)newProp)->SetPropertyClass(uClass);
            }

        }
        else if (engineObj->IsA<UEnum>())
        {
            newProp = NewObject<UEnumProperty>(engineClass, UTF8_TO_TCHAR(propName), propFlags);
            UNumericProperty *underlying = NewObject<UByteProperty>(newProp, TEXT("UnderlyingType"), propFlags);
            ((UEnumProperty*)newProp)->SetEnum((UEnum*)engineObj);
            newProp->AddCppProperty(underlying);
        }
        else if (engineObj->IsA<UStruct>())
        {
            newProp = NewObject<UStructProperty>(engineClass, UTF8_TO_TCHAR(propName), propFlags);
            ((UStructProperty*)newProp)->Struct = (UScriptStruct*)engineObj;
        }
        // TODO: support for scriptstruct, obj that implements an interface
        // TODO (maybe): support for FName, Text
        // TODO: array, map, set classes
    }
    else
        return PyErr_Format(PyExc_Exception, "Cannot create uprop for %s", propName);

    if (newProp)
    {   // TODO: check this
        uint64 flags = CPF_Edit | CPF_BlueprintVisible | CPF_ZeroConstructor;
        newProp->SetPropertyFlags(flags);
        newProp->ArrayDim = 1;
        UStruct *us = (UStruct *)engineClass;
        us->AddCppProperty(newProp);
        us->StaticLink(true);

    }
    else
    {
        LWARN("WARNING: did not add new property %s", UTF8_TO_TCHAR(propName));
    }

    //you are here
    Py_RETURN_NONE;
}

// used by __setattr__ on a python instance to set a value on a uprop - caller ensures the uprop exists
static PyObject *set_uproperty_value(PyObject *self, PyObject *args)
{
    unsigned long long instAddr;
    char *propName;
    PyObject *pyValue;
    int emitEvents;
    if (!PyArg_ParseTuple(args, "KsOp", &instAddr, &propName, &pyValue, &emitEvents))
        return nullptr;

    UObject *engineObj = (UObject *)instAddr;
    if (!USEFUL(engineObj))
        return PyErr_Format(PyExc_Exception, "Invalid UObject");

    UProperty *prop = engineObj->GetClass()->FindPropertyByName(FName(UTF8_TO_TCHAR(propName)));
    if (!prop)
        return PyErr_Format(PyExc_Exception, "Non-existent property %s", propName);

#if WITH_EDITOR
    if (emitEvents)
        engineObj->PreEditChange(prop);
#endif

    if (ue_py_convert_pyobject(pyValue, prop, (uint8 *)engineObj, 0))
    {
        if (emitEvents)
        {
            FPropertyChangedEvent PropertyEvent(prop, EPropertyChangeType::ValueSet);
            engineObj->PostEditChangeProperty(PropertyEvent);
        }
#if WITH_EDITOR
        // TODO: original had special code for archtype/CDO objects
#endif
        Py_RETURN_NONE;
    }

    // TODO: err if they try to write to a UFunction name

    return PyErr_Format(PyExc_Exception, "invalid value for UProperty");
}

// used by __getattr__ on a python instance to get a value from a uprop - caller ensures
// the given name is really a uprop
static PyObject *get_uproperty_value(PyObject *self, PyObject *args)
{
    unsigned long long instAddr;
    char *propName;
    if (!PyArg_ParseTuple(args, "Ks", &instAddr, &propName))
        return nullptr;

    UObject *engineObj = (UObject *)instAddr;
    if (!USEFUL(engineObj))
    {
        LERROR("engineObj not useful");
        return nullptr;
    }

    UProperty *prop = engineObj->GetClass()->FindPropertyByName(FName(UTF8_TO_TCHAR(propName)));
    if (!prop)
    {
        LERROR("Non-existent property");
        return nullptr;
    }

    return ue_py_convert_property(prop, (uint8 *)engineObj, 0);
}

static PyMethodDef module_methods[] = {
    {"create_subclass", create_subclass, METH_VARARGS, ""},
    {"call_ufunction_object", call_ufunction_object, METH_VARARGS, ""},
    {"get_py_proxy", get_py_proxy, METH_VARARGS, ""},
    {"add_ufunction", add_ufunction, METH_VARARGS, ""},
    {"get_ufunction_names", get_ufunction_names, METH_VARARGS, ""},
    {"get_ufunction_object", get_ufunction_object, METH_VARARGS, ""},
    {"get_uproperty_names", get_uproperty_names, METH_VARARGS, ""},
    {"add_uproperty", add_uproperty, METH_VARARGS, ""},
    {"set_uproperty_value", set_uproperty_value, METH_VARARGS, ""},
    {"get_uproperty_value", get_uproperty_value, METH_VARARGS, ""},
    { NULL, NULL },
};

// creates and sets up the special 'fm' module in the interpreter
void fm_init_module()
{
    //LOG("Initializing module");
	GAllowActorScriptExecutionInEditor = true; // without this, UFUNCTION calls in the editor often don't work - maybe that's intentional?
    PyObject *module = PyImport_AddModule("_fmsubclassing"); // Scripts/fm/__init__.py imports this
    PyObject *module_dict = PyModule_GetDict(module);

    for (PyMethodDef *m = module_methods; m->ml_name != NULL; m++)
    {
        PyObject *func = PyCFunction_New(m, NULL);
        PyDict_SetItemString(module_dict, m->ml_name, func);
        Py_DECREF(func);
    }
}
