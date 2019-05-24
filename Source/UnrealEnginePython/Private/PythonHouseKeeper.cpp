#pragma once
//#pragma optimize("", off)

#include "PythonHouseKeeper.h"
extern PyTypeObject ue_PyUObjectType;


void FUnrealEnginePythonHouseKeeper::AddReferencedObjects(FReferenceCollector& InCollector)
{
    InCollector.AddReferencedObjects(UObjectPyMapping);
}

FUnrealEnginePythonHouseKeeper *FUnrealEnginePythonHouseKeeper::Get()
{
    static FUnrealEnginePythonHouseKeeper *Singleton;
    if (!Singleton)
    {
        Singleton = new FUnrealEnginePythonHouseKeeper();
        // register a new delegate for the GC
#if ENGINE_MINOR_VERSION >= 18
        FCoreUObjectDelegates::GetPostGarbageCollect().AddRaw(Singleton, &FUnrealEnginePythonHouseKeeper::RunGCDelegate);
#else
        FCoreUObjectDelegates::PostGarbageCollect.AddRaw(Singleton, &FUnrealEnginePythonHouseKeeper::RunGCDelegate);
#endif

#if WITH_EDITOR
        // PIE blows up if we don't run GC before shutting down
        FEditorDelegates::PrePIEEnded.AddRaw(Singleton, &FUnrealEnginePythonHouseKeeper::OnPIEEvent);
        FEditorDelegates::EndPIE.AddRaw(Singleton, &FUnrealEnginePythonHouseKeeper::OnPIEEvent);
#endif
    }
    return Singleton;
}

void FUnrealEnginePythonHouseKeeper::OnPIEEvent(bool IsSimulating)
{
    FScopePythonGIL gil;
    RunGC();
}

void FUnrealEnginePythonHouseKeeper::RunGCDelegate()
{
    FScopePythonGIL gil;
    RunGC();
}

int32 FUnrealEnginePythonHouseKeeper::RunGC()
{
    int32 Garbaged = PyUObjectsGC();
    Garbaged += DelegatesGC();
    return Garbaged;
}

bool FUnrealEnginePythonHouseKeeper::IsValidPyUObject(ue_PyUObject *PyUObject)
{
    if (!PyUObject)
        return false;

    UObject *Object = PyUObject->ue_object;
    FPythonUObjectTracker *Tracker = UObjectPyMapping.Find(Object);
    if (!Tracker)
    {
        return false;
    }

    if (!Object->IsValidLowLevel())
        return false;

    return true;

}

ue_PyUObject *FUnrealEnginePythonHouseKeeper::WrapEngineObject(UObject *Object)
{
    if (!Object || !Object->IsValidLowLevel() || Object->IsPendingKillOrUnreachable())
    {
#if defined(UEPY_MEMORY_DEBUG)
        UE_LOG(LogPython, Error, _T("Cannot WrapEngineObject for invalid object %p"), Object);
#endif
        return nullptr;
    }

    // if somebody else already has a wrapper for this engine object, reuse it
    FPythonUObjectTracker *Tracker = UObjectPyMapping.Find(Object);
    if (Tracker)
        return Tracker->PyUObject;

    // we don't currently have a wrapper for this engine object, so create one
    ue_PyUObject *ue_py_object = (ue_PyUObject *)PyObject_New(ue_PyUObject, &ue_PyUObjectType);
    if (!ue_py_object)
        return nullptr;
#if defined(UEPY_MEMORY_DEBUG)
    UE_LOG(LogPython, Warning, _T("REF CREATE for %s (%p), py %p"), *Object->GetName(), Object, ue_py_object);
#endif
    ue_py_object->ue_object = Object;
    ue_py_object->py_proxy = nullptr;
    ue_py_object->auto_rooted = 0;
    ue_py_object->py_dict = PyDict_New();
    ue_py_object->owned = 0;

    UObjectPyMapping.Add(Object, FPythonUObjectTracker(Object, ue_py_object));
    return ue_py_object;
}

uint32 FUnrealEnginePythonHouseKeeper::PyUObjectsGC()
{
    // we periodically scan our list of tracked objects to see if any of the Python wrappers have a refcount of 1,
    // which means that the housekeeper is the only remaining ref so the wrapper should be destroyed.
    TArray<UObject *> objsToRemove;
    for (auto& entry : UObjectPyMapping)
    {
        UObject *Object = entry.Key;
        FPythonUObjectTracker &Tracker = entry.Value;
        if (Py_REFCNT(Tracker.PyUObject) <= 1)
        {
#if defined(UEPY_MEMORY_DEBUG)
            UE_LOG(LogPython, Warning, _T("REF DESTRY for %s (%p), py %p"), *Tracker.ObjName, Object, Tracker.PyUObject);
#endif
            objsToRemove.Add(Object);
            Py_CLEAR(Tracker.PyUObject);
        }
#if defined(UEPY_MEMORY_DEBUG)
        else if (!Object->IsValidLowLevel())
        {
            UE_LOG(LogPython, Error, _T("REF engine obj %s (%p) GC'd out from under us (py %p)"), *Tracker.ObjName, Object, Tracker.PyUObject);
        }
#endif
    }

    uint32 removed = 0;
    for (UObject *Object : objsToRemove)
    {
        removed++;
        UObjectPyMapping.Remove(Object);
    }
    return removed;
}

int32 FUnrealEnginePythonHouseKeeper::DelegatesGC()
{
    int32 Garbaged = 0;
#if defined(UEPY_MEMORY_DEBUG)
    UE_LOG(LogPython, Display, TEXT("Garbage collecting %d UObject delegates"), PyDelegatesTracker.Num());
#endif
    for (int32 i = PyDelegatesTracker.Num() - 1; i >= 0; --i)
    {
        FPythonDelegateTracker &Tracker = PyDelegatesTracker[i];
        if (!Tracker.Owner.IsValid(true))
        {
            Tracker.Delegate->RemoveFromRoot();
            PyDelegatesTracker.RemoveAt(i);
            Garbaged++;
        }

    }

#if defined(UEPY_MEMORY_DEBUG)
    UE_LOG(LogPython, Display, TEXT("Garbage collecting %d Slate delegates"), PySlateDelegatesTracker.Num());
#endif

    for (int32 i = PySlateDelegatesTracker.Num() - 1; i >= 0; --i)
    {
        FPythonSWidgetDelegateTracker &Tracker = PySlateDelegatesTracker[i];
        if (!Tracker.Owner.IsValid())
        {
            PySlateDelegatesTracker.RemoveAt(i);
            Garbaged++;
        }

    }
    return Garbaged;
    }

UPythonDelegate *FUnrealEnginePythonHouseKeeper::FindDelegate(UObject *Owner, PyObject *PyCallable)
{
    for (int32 i = PyDelegatesTracker.Num() - 1; i >= 0; --i)
    {
        FPythonDelegateTracker &Tracker = PyDelegatesTracker[i];
        if (Tracker.Owner.Get() == Owner && Tracker.Delegate->UsesPyCallable(PyCallable))
            return Tracker.Delegate;
    }
    return nullptr;
}

UPythonDelegate *FUnrealEnginePythonHouseKeeper::NewDelegate(UObject *Owner, PyObject *PyCallable, UFunction *Signature)
{
    UPythonDelegate *Delegate = NewObject<UPythonDelegate>();

    Delegate->AddToRoot();
    Delegate->SetPyCallable(PyCallable);
    Delegate->SetSignature(Signature);

    FPythonDelegateTracker Tracker(Delegate, Owner);
    PyDelegatesTracker.Add(Tracker);

    return Delegate;
}

TSharedRef<FPythonSlateDelegate> FUnrealEnginePythonHouseKeeper::NewSlateDelegate(TSharedRef<SWidget> Owner, PyObject *PyCallable)
{
    TSharedRef<FPythonSlateDelegate> Delegate = MakeShareable(new FPythonSlateDelegate());
    Delegate->SetPyCallable(PyCallable);

    FPythonSWidgetDelegateTracker Tracker(Delegate, Owner);
    PySlateDelegatesTracker.Add(Tracker);

    return Delegate;
}

TSharedRef<FPythonSlateDelegate> FUnrealEnginePythonHouseKeeper::NewDeferredSlateDelegate(PyObject *PyCallable)
{
    TSharedRef<FPythonSlateDelegate> Delegate = MakeShareable(new FPythonSlateDelegate());
    Delegate->SetPyCallable(PyCallable);

    return Delegate;
}

TSharedRef<FPythonSmartDelegate> FUnrealEnginePythonHouseKeeper::NewPythonSmartDelegate(PyObject *PyCallable)
{
    TSharedRef<FPythonSmartDelegate> Delegate = MakeShareable(new FPythonSmartDelegate());
    Delegate->SetPyCallable(PyCallable);

    PyStaticSmartDelegatesTracker.Add(Delegate);

    return Delegate;
}

void FUnrealEnginePythonHouseKeeper::TrackDeferredSlateDelegate(TSharedRef<FPythonSlateDelegate> Delegate, TSharedRef<SWidget> Owner)
{
    FPythonSWidgetDelegateTracker Tracker(Delegate, Owner);
    PySlateDelegatesTracker.Add(Tracker);
}

TSharedRef<FPythonSlateDelegate> FUnrealEnginePythonHouseKeeper::NewStaticSlateDelegate(PyObject *PyCallable)
{
    TSharedRef<FPythonSlateDelegate> Delegate = MakeShareable(new FPythonSlateDelegate());
    Delegate->SetPyCallable(PyCallable);

    PyStaticSlateDelegatesTracker.Add(Delegate);

    return Delegate;
}

