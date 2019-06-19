//#pragma optimize("", off)

#include "PythonHouseKeeper.h"
extern PyTypeObject ue_PyUObjectType;

/*
Notes / gotchas
- The housekeeper is a singleton, that is created when the engine starts, but *not* when PIE starts/restarts. As a result,
  even if we manage references to engine objects correctly, there are still legitimate cases where engine objects get
  deleted out from under us (e.g. PIE, then stop, then PIE again - the housekeeper can have nullptrs in its object map).
  (the engine tracks refs to objects in uprops or in engine structures - like TMap - so it is able to null our pointer
  to a UObject that has been deleted)
- The editor gets really annoyed if we keep ahold of an object that was created for the PIE world even after PIE is done,
  but we don't currently have a good way to force all Python code to get rid of such references yet, so on PIE shutdown
  we forcibly delete PIE objects we're still tracking.
*/
#define LOG(format, ...) UE_LOG(LogPython, Log, TEXT("[%s:%d] %s"), TEXT(__FUNCTION__), __LINE__, *FString::Printf(TEXT(format), ##__VA_ARGS__ ))
#define LWARN(format, ...) UE_LOG(LogPython, Warning, TEXT("[%s:%d] %s"), TEXT(__FUNCTION__), __LINE__, *FString::Printf(TEXT(format), ##__VA_ARGS__ ))
#define LERROR(format, ...) UE_LOG(LogPython, Error, TEXT("[%s:%d] %s"), TEXT(__FUNCTION__), __LINE__, *FString::Printf(TEXT(format), ##__VA_ARGS__ ))

void FUnrealEnginePythonHouseKeeper::AddReferencedObjects(FReferenceCollector& InCollector)
{
    // When informing the engine of what objects we're tracking, skip any that we'll stop tracking in the next
    // cleanup sweep
	for (auto& entry : UObjectPyMapping)
	{
		UObject *Object = entry.Key;
		FPythonUObjectTracker &Tracker = entry.Value;
		if (Tracker.PyUObject && Py_REFCNT(Tracker.PyUObject) > 1 && Object && Object->IsValidLowLevel())
			InCollector.AddReferencedObject(Object);
#if WITH_EDITOR
        else
        {
            LWARN("Not adding %s (%p) for py %p (rc %d)", *Tracker.ObjName, Object, Tracker.PyUObject, Tracker.PyUObject ? Py_REFCNT(Tracker.PyUObject) : -1);
        }
#endif
	}
}
void FUnrealEnginePythonHouseKeeper::DumpState()
{
#if WITH_EDITOR
    int i=0;
    PyObject *gc = PyImport_ImportModule("gc");
    PyObject *get_refs = PyObject_GetAttrString(gc, "get_referrers");
	for (auto& entry : UObjectPyMapping)
    {
		UObject *Object = entry.Key;
		FPythonUObjectTracker &Tracker = entry.Value;
        LOG("[%02X] %s (%p, val? %d) py %p (rc %d)", i++, *Tracker.ObjName, Object, Object != nullptr && Object->IsValidLowLevel(), Tracker.PyUObject, Tracker.PyUObject ? Py_REFCNT(Tracker.PyUObject) : -1);
        if (Py_REFCNT(Tracker.PyUObject) > 1 && (!Object || !Object->IsValidLowLevel()))
        {
            PyObject *args = Py_BuildValue("(O)", Tracker.PyUObject);
            PyObject *ret = PyObject_Call(get_refs, args, nullptr);
            PyObject *iter = PyObject_GetIter(ret);
            while (true)
            {
                PyObject *next = PyIter_Next(iter);
                if (!next)
                    break;
                PyObject *type = PyObject_Type(next);
                PyObject *typeRepr = PyObject_Repr(type);
                PyObject *repr = PyObject_Repr(next);
                char reprStr[256];
                Py_ssize_t size=0;
                const char *reprU = PyUnicode_AsUTF8AndSize(repr, &size);
                if (size > 255)
                    size = 255;
                strncpy(reprStr, reprU, size);
                reprStr[size] = 0;

                LOG("  ref: %p (%s) : %s", next, UTF8_TO_TCHAR(UEPyUnicode_AsUTF8(typeRepr)), UTF8_TO_TCHAR(reprStr));
                Py_DECREF(typeRepr);
                Py_DECREF(type);
                Py_DECREF(repr);
                Py_DECREF(next);
            }
            Py_DECREF(iter);
            Py_XDECREF(ret);
            Py_DECREF(args);
        }
    }
    Py_DECREF(get_refs);
    Py_DECREF(gc);
#endif
}

FUnrealEnginePythonHouseKeeper *FUnrealEnginePythonHouseKeeper::Get()
{
    static FUnrealEnginePythonHouseKeeper *Singleton;
    if (!Singleton)
    {
        Singleton = new FUnrealEnginePythonHouseKeeper();
        // register a new delegate for the GC
#if ENGINE_MINOR_VERSION >= 18
        FCoreUObjectDelegates::GetPreGarbageCollectDelegate().AddRaw(Singleton, &FUnrealEnginePythonHouseKeeper::OnPreGC);
        FCoreUObjectDelegates::GetPostGarbageCollect().AddRaw(Singleton, &FUnrealEnginePythonHouseKeeper::OnPostGC);
#else
        FCoreUObjectDelegates::PreGarbageCollect.AddRaw(Singleton, &FUnrealEnginePythonHouseKeeper::OnPreGC);
        FCoreUObjectDelegates::PostGarbageCollect.AddRaw(Singleton, &FUnrealEnginePythonHouseKeeper::OnPostGC);
#endif

#if WITH_EDITOR
        // PIE blows up if we don't run GC before shutting down
        //FEditorDelegates::PreBeginPIE.AddRaw(Singleton, &FUnrealEnginePythonHouseKeeper::OnPreBeginPIE);
        FEditorDelegates::PrePIEEnded.AddRaw(Singleton, &FUnrealEnginePythonHouseKeeper::OnPrePIEEnded);
        FEditorDelegates::EndPIE.AddRaw(Singleton, &FUnrealEnginePythonHouseKeeper::OnEndPIE);
#endif
    }
    return Singleton;
}

void FUnrealEnginePythonHouseKeeper::OnPreGC()
{
    LOG("::::");
    FScopePythonGIL gil;
    PruneUnusedPyObjTrackers();
}

int32 FUnrealEnginePythonHouseKeeper::RunGC()
{
    FScopePythonGIL gil;
    PruneUnusedPyObjTrackers();
    return DelegatesGC();
}

void FUnrealEnginePythonHouseKeeper::OnPostGC()
{
    LOG(":::");
    FScopePythonGIL gil;
    DelegatesGC();
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

void FUnrealEnginePythonHouseKeeper::OnPrePIEEnded(bool IsSimulating)
{
    FScopePythonGIL gil;
    PyGC_Collect();
    PruneUnusedPyObjTrackers();
}

void FUnrealEnginePythonHouseKeeper::OnEndPIE(bool IsSimulating)
{
#if WITH_EDITOR
    FScopePythonGIL gil;
    PyGC_Collect();
    PruneUnusedPyObjTrackers();
#endif

#if defined(UEPY_MEMORY_DEBUG)
    // No PIE world objects should remain at this point
    for (auto& entry : UObjectPyMapping)
    {
        UObject *Object = entry.Key;
        FPythonUObjectTracker &Tracker = entry.Value;
        if (!Object)
        {
            LWARN("Tracker for %s has a null object", *Tracker.ObjName);
        }
        else if (!Object->IsValidLowLevel())
        {
            LWARN("Tracker for %s has an invalid object", *Tracker.ObjName);
        }
        else if (Object->GetOutermost()->HasAnyPackageFlags(PKG_PlayInEditor))
        {
            LWARN("Tracker still exists for a PIE object: %s (%p), py obj %p (rc %d)", *Tracker.ObjName, Object, Tracker.PyUObject, Py_REFCNT(Tracker.PyUObject));
        }
    }
#endif
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
    //LOG("XXX Wrapping %s (%p) in pyobj %p", *Object->GetName(), Object, ue_py_object);
#if defined(UEPY_MEMORY_DEBUG)
    UE_LOG(LogPython, Warning, _T("REF CREATE for %s (%p), py %p"), *Object->GetName(), Object, ue_py_object);
#endif
    ue_py_object->ue_object = Object;
    ue_py_object->py_proxy = nullptr;
    ue_py_object->auto_rooted = 0;
    ue_py_object->py_dict = PyDict_New();
    ue_py_object->owned = 0;

    UObjectPyMapping.Add(Object, FPythonUObjectTracker(Object, ue_py_object)); // Note: this steals the ref
    return ue_py_object;
}

// Finds any tracker entries where the python object's refcount is 1 (which means we are the only one still with a ref) and removes
// them.
void FUnrealEnginePythonHouseKeeper::PruneUnusedPyObjTrackers()
{
    TArray<UObject *> objsToRemove;
    for (auto& entry : UObjectPyMapping)
    {
        UObject *Object = entry.Key;
        FPythonUObjectTracker &Tracker = entry.Value;
        int rc = Tracker.PyUObject ? Py_REFCNT(Tracker.PyUObject) : -1;
#if WITH_EDITOR
        if (!Tracker.PyUObject || Py_REFCNT(Tracker.PyUObject) < 1)
        {   // This shouldn't ever happen, so don't try to fix it here.
            LWARN("Tracker exists for %s (%p) but with invalid pyobj (rc %d)", *Tracker.ObjName, Object, rc);
        }
        else
#endif
        if (Py_REFCNT(Tracker.PyUObject) <= 1)
        {   // Nobody else is using this python object wrapping the engine object, so get rid of both
            //LOG("Removing %s", *Tracker.ObjName);
            objsToRemove.Add(Object);
            if (Py_REFCNT(Tracker.PyUObject) > 0)
                Py_DECREF(Tracker.PyUObject);
        }
        else if (!Object || !Object->IsValidLowLevel())
        {   // If we get here, it means somewhere else we didn't ref count properly. Attempting to fix it is likely to fail.
#if WITH_EDITOR
            LWARN("Obj no longer valid %s (%p) for py %p (rc %d)", *Tracker.ObjName, Object, Tracker.PyUObject, rc);
#endif
            if (Py_REFCNT(Tracker.PyUObject) > 0)
                Py_DECREF(Tracker.PyUObject);
            objsToRemove.Add(Object);
		}
    }

    for (UObject *Object : objsToRemove)
        UObjectPyMapping.Remove(Object);
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

