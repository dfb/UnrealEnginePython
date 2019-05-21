#pragma once

#include "UnrealEnginePython.h"
#include "UObject/UObjectGlobals.h"
#include "UObject/WeakObjectPtr.h"
#include "Widgets/SWidget.h"
#include "Slate/UEPySlateDelegate.h"
#include "Runtime/CoreUObject/Public/UObject/GCObject.h"
#include "PythonDelegate.h"
#include "PythonSmartDelegate.h"

// Anytime an engine object is passed to Python, it is sent back wrapped in a ue_PyUObject instance. As
// long as that wrapper exists, we need to ensure that the corresponding engine object it wraps stays
// alive, because holding a reference to a wrapper should result in the same behavior as holding a reference
// to the engine object itself. IOW, the engine doesn't know about references to engine objects that are being
// used/stored in Python, so it could erroneously clean up an object because it thinks nobody is using it
// anymore, so there needs to be something that makes sure engine objects being used by Python stay alive.
//
// The housekeeper is a singleton that solves this problem. Anytime an engine object needs to be passed to
// Python, the housekeeper creates a ue_PyUObject wrapper (or reuses an existing one for that engine object)
// and sends it to Python. The housekeeper also maintains an internal list of which engine objects are currently
// in use by Python and then hooks into the UE4 garbage collector to tell it that those objects cannot be
// destroyed just yet.
class FUnrealEnginePythonHouseKeeper : public FGCObject
{
    struct FPythonUObjectTracker
    {
#if defined(UEPY_MEMORY_DEBUG)
        FString ObjName;
#endif
        ue_PyUObject *PyUObject;

        FPythonUObjectTracker(UObject *Object, ue_PyUObject *InPyUObject)
        {
#if defined(UEPY_MEMORY_DEBUG)
            ObjName = Object->GetName();
#endif
            PyUObject = InPyUObject; // we own this ref
        }
    };

    struct FPythonDelegateTracker
    {
        FWeakObjectPtr Owner;
        UPythonDelegate *Delegate;

        FPythonDelegateTracker(UPythonDelegate *DelegateToTrack, UObject *DelegateOwner) : Owner(DelegateOwner), Delegate(DelegateToTrack)
        {
        }

        ~FPythonDelegateTracker()
        {
        }
    };

    struct FPythonSWidgetDelegateTracker
    {
        TWeakPtr<SWidget> Owner;
        TSharedPtr<FPythonSlateDelegate> Delegate;

        FPythonSWidgetDelegateTracker(TSharedRef<FPythonSlateDelegate> DelegateToTrack, TSharedRef<SWidget> DelegateOwner) : Owner(DelegateOwner), Delegate(DelegateToTrack)
        {
        }

        ~FPythonSWidgetDelegateTracker()
        {
        }
    };

public:

	virtual void AddReferencedObjects(FReferenceCollector& InCollector) override;
	static FUnrealEnginePythonHouseKeeper *Get();
    int32 RunGC();
	bool IsValidPyUObject(ue_PyUObject *PyUObject);

    // Given an engine object, returns a borrowed ref to a Python wrapper for it or null if the engine object is invalid
	ue_PyUObject *WrapEngineObject(UObject *Object);

	UPythonDelegate *FindDelegate(UObject *Owner, PyObject *PyCallable);
	UPythonDelegate *NewDelegate(UObject *Owner, PyObject *PyCallable, UFunction *Signature);
	TSharedRef<FPythonSlateDelegate> NewSlateDelegate(TSharedRef<SWidget> Owner, PyObject *PyCallable);
	TSharedRef<FPythonSlateDelegate> NewDeferredSlateDelegate(PyObject *PyCallable);
	TSharedRef<FPythonSmartDelegate> NewPythonSmartDelegate(PyObject *PyCallable);
	void TrackDeferredSlateDelegate(TSharedRef<FPythonSlateDelegate> Delegate, TSharedRef<SWidget> Owner);
	TSharedRef<FPythonSlateDelegate> NewStaticSlateDelegate(PyObject *PyCallable);

private:
    void RunGCDelegate();
    uint32 PyUObjectsGC();
	int32 DelegatesGC();

	TMap<UObject *, FPythonUObjectTracker> UObjectPyMapping;
	TArray<FPythonDelegateTracker> PyDelegatesTracker;

	TArray<FPythonSWidgetDelegateTracker> PySlateDelegatesTracker;
	TArray<TSharedRef<FPythonSlateDelegate>> PyStaticSlateDelegatesTracker;

	TArray<TSharedRef<FPythonSmartDelegate>> PyStaticSmartDelegatesTracker;
};
