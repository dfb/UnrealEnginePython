'''
Code for creating and using subclassses of C++ and Blueprint classes

The general starting point and goals we use for subclassing are:
- you have some existing UClass in your project (either a Blueprint class or a C++ class) that has an arbitrarily deep class
    ancestry, and you want to create a Python subclass of it
- in Python you want to be able to subclass further as needed
- as much as possible, Python subclasses should be first-class citizens in UE4 - they should be usable anywhere that Blueprint
    classes and objects are usable
- at the same time, Python subclasses and objects from them should be as Pythonic as possible
'''
from . common import *
import _fmsubclassing as fms
from unreal_engine import classes as engine_classes

# decorator that is roughly equivalent to UFUNCTION() in C++ - use this on a method in a subclass to cause it to
# be exposed as callable
# TODO: add support for pure=True, etc.
def ufunction(f):
    f.ufunction = True
    return f

# used for declaring UPROPERTYs. Use when creating class vars: myVar = uproperty(FVector, FVector(1,2,3)). By default, implies BlueprintReadWrite.
# TODO: add support for replication, editanywhere, BPReadOnly, repnotify, and other flags. myVar = uproperty(default, *kwFlags)
class uproperty:
    def __init__(self, type, default, is_class=False): # TODO: make default optional, but find a suitable default (e.g. int=0, bool=False, etc. Or, let them give a default and we infer the type in many cases
        self.type = type
        self.default = default
        self.is_class = is_class # i.e. the property holds a UClass not a UObject

class MetaBase(type):
    '''Metaclass used to help in the creation of Python subclasses of engine classes'''
    def __new__(metaclass, name, bases, dct):
        # remove any uproperties before creating the class - they get processed later
        uprops = [] # (name, uprop obj)
        for k,v in list(dct.items()):
            if isinstance(v, uproperty):
                dct.pop(k)
                uprops.append((k,v))

        interfaces = []
        for cls in dct.pop('interfaces', []):
            assert isinstance(cls, UObject), '%r cannot be used as an interface class' % cls
            assert cls.class_get_flags() & ue.CLASS_INTERFACE, '%r is not an interface class' % cls
            interfaces.append(cls)

        newPyClass = super().__new__(metaclass, name, bases, dct)

        # TODO: add some checks to bases to verify that there is at most 1 engine class present in the ancestry
        # TODO: add some checks to make sure that if __uclass__ is present, it exactly matches the ancestor one (ti's unneeded but harmless)
        #       (eh, I think we should raise an error if it's present)
        dct['get_py_proxy'] = lambda self:fms.get_py_proxy(self)

        if name == 'BridgeBase':
            pass # No extra processing for this case
        else:
            assert len(bases) > 0, 'This class must subclass something else'
            assert issubclass(newPyClass, BridgeBase), 'MetaBase is only for use with subclassing BridgeBase'
            isBridge = bases[0] == BridgeBase # is this a bridge class or some further descendent?
            if isBridge:
                engineParentClass = getattr(newPyClass, '__uclass__', None)
                assert engineParentClass is not None, 'Missing __uclass__ property'
            else:
                engineParentClass = bases[0].engineClass

            # create a new corresponding UClass and set up for UPROPERTY handling
            newPyClass.engineClass = fms.create_subclass(name, engineParentClass, newPyClass)
            metaclass.SetupProperties(newPyClass, engineParentClass, uprops)

            # Scan the class and process its methods to wire things up with UE4
            if isBridge:
                metaclass.ProcessBridgeClassMethods(newPyClass, engineParentClass)
            else:
                metaclass.ProcessBridgeDescendentClassMethods(newPyClass)

            # add in any interfaces this class claims to support (TODO: verify that all necessary methods are implemented)
            for cls in interfaces:
                fms.add_interface(newPyClass.engineClass, cls)

        return newPyClass

    @classmethod
    def SetupProperties(metaclass, newPyClass, engineParentClass, upropList):
        '''Called from __new__ to get set up for UPROPERTY handling - saves list of property names and registers with the
        UClass any properties added by this Python class'''
        # Store a list of all known UPROPERTYs - most of property handling is deferred until they are used so at this point
        # we just make a note of their names
        newPyClass.__property_names__ = fms.get_uproperty_names(engineParentClass)

        initialValues = {} # prop name --> default value
        for k,v in upropList:
            if not isinstance(v, uproperty):
                continue
            initialValues[k] = v.default # these will be set during init

            if k not in newPyClass.__property_names__:
                # This is not a known property, so we need to register it with the UCclass
                newPyClass.__property_names__.append(k)
                fms.add_uproperty(newPyClass.engineClass, k, v.type, v.is_class)

        newPyClass.__property_defaults__ = initialValues

    @classmethod
    def ProcessBridgeClassMethods(metaclass, newPyClass, engineParentClass):
        '''Called from __new__ to handle the case where the class being created is a bridge class. For each UFUNCTION in the class ancestry,
        adds a corresponding Python callable to that UFUNCTION that results in a call to the C++ implementation (effectively creating the super()
        implementation for each UFUNCTION).'''
        for funcName in fms.get_ufunction_names(engineParentClass):
            # Make it so that from Python you can use that name to call the UFUNCTION version in UE4
            metaclass.AddMethodCaller(engineParentClass, newPyClass, funcName)

    @classmethod
    def ProcessBridgeDescendentClassMethods(metaclass, newPyClass):
        '''Called from __new__ to handle the case where the class being created is a descendent of a bridge class. For each method that is marked
        as a ufunction, adds that method to the class but under a different name (_orig_<funcName>), adds a UFUNCTION to the new class on the C++
        side to call that method, and adds a Python callable to that UFUNCTION (*not* to the method directly, so that calls like self.Foo will
        go through the UE4 layer for things like replication).'''
        for k,v in list(newPyClass.__dict__.items()):
            if k.startswith('__') or k in ('engineClass',):
                continue
            if not callable(v) or not getattr(v, 'ufunction', None):
                continue
            funcName, func = k,v

            # Add the method to the class but under a different name so it doesn't get stomped by the stuff below
            hiddenFuncName = '_orig_' + funcName
            setattr(newPyClass, hiddenFuncName, func)

            # Expose a UFUNCTION in C++ that calls this method
            fms.add_ufunction(newPyClass.engineClass, funcName, func)

            # Make it so that from Python you can use that name to call the UFUNCTION version in UE4 (so that things
            # like replication work)
            metaclass.AddMethodCaller(newPyClass.engineClass, newPyClass, funcName)

            # TODO: somewhere in here we should warn if the user tries to override something that is BPCallable but not BPNativeEvent
            # and not BPImeplementableEvent - the engine will allow us but the the results won't work quite right

    @classmethod
    def AddMethodCaller(metaclass, classWithFunction, newPyClass, funcName):
        '''Adds to newPyClass a method that calls the UFUNCTION C++ method of the same name. classWithFunction is the engine class
        that has that function (we can't do a dynamic lookup by name later because then super() calls don't work - we have to bind
        to the function on the class now).'''
        uFunc = fms.get_ufunction_object(classWithFunction, funcName)
        def _(self, *args, **kwargs):
            return fms.call_ufunction_object(self.instAddr, self, uFunc, args, kwargs)
        _.__name__ = funcName
        setattr(newPyClass, funcName, _)

class BridgeBase: #(metaclass=MetaBase): - let the metaclass be specified dynamically
    '''Base class of all bridge classes we generate'''
    def __new__(cls, instAddr, *args, **kwargs): # same convention as with __init__
        inst = super(BridgeBase, cls).__new__(cls)

        # Set any default property values
        for propName, default in cls.__property_defaults__.items():
            try:
                fms.set_uproperty_value(instAddr, propName, default, False)
            except:
                logTB()
        return inst

    def __init__(self, instAddr):
        self.instAddr = instAddr # by convention, the UObject addr is passed to the instance

    @property
    def uobject(self):
        '''gets the UObject that owns self, wrapped as a python object so it can be passed to other APIs'''
        return fms.get_ue_inst(self.instAddr)

    def __setattr__(self, k, v):
        if k in self.__class__.__property_names__:
            fms.set_uproperty_value(self.instAddr, k, v, True)
        else:
            super().__setattr__(k, v)

    def __getattr__(self, k):
        if k in self.__class__.__property_names__:
            return fms.get_uproperty_value(self.instAddr, k)
        else:
            return super().__getattr__(self, k)

class BridgeClassGenerator:
    '''Dynamically creates the bridge class for any UE4 class'''
    def __init__(self, metaclass):
        self.cache = {} # class name --> class instance
        self.newClassMetaclass = metaclass

    def __getattr__(self, className):
        try:
            # return cached copy if we've already generated it previously
            return self.cache[className]
        except KeyError:
            # generate a new class and cache it
            engineClass = getattr(engine_classes, className) # let this raise an error if the class doesn't exist

            #from unreal_engine.classes import 
            dct = dict(
                __uclass__ = engineClass
            )
            meta = self.newClassMetaclass
            cls = meta.__new__(meta, className+'_Bridge', (BridgeBase,), dct)
            self.cache[className] = cls
            return cls

# TODO: make this implicit - i.e. let subclasses pass in an unreal_engine.classes class obj and have metaclass wrap them automatically
bridge = BridgeClassGenerator(MetaBase)

