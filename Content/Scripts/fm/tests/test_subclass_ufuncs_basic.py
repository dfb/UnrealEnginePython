'''
There are a gazillion different scenarios wrt UFUNCTIONs that we have to ensure work properly. These tests try
to exercise the most common ones. The tests should run without throwing errors, but you currently have to also
check the output to veriy it's correct.
'''
from fm.common import *
import fm
bridge = fm.subclassing.bridge
ufunction = fm.subclassing.ufunction

# Py call of C UFUNC not overridden in Py subclass but exposed by bridge
# BPviaPy call of C UFUNC not overridden in Py subclass but exposed by bridge
if 1:
    log('-- TEST 0 --')
    class T0Child(bridge.CChild):
        pass
    log('a')
    c = Spawn(T0Child)
    log('b')
    c.CFunctionNoOverride()
    log('c')
    c.get_py_proxy().CFunctionNoOverride()
    log('d')

# Py call of C UFUNC exposed by bridge AND overridden by Py subclass and DOES NOT call super impl
# BPviaPy call of C UFUNC exposed by bridge AND overridden by Py subclass and DOES NOT call super impl
if 1:
    log('-- TEST 1 --')
    class T1Child(bridge.CChild):
        def __init__(self, *args, **kwargs):
            super().__init__(*args, **kwargs)
            self.childProp = 't1child'
        @ufunction
        def CFunctionBPImplementable(self):
            log('T1Child.CFunctionBPImplementable, childprop:', self.childProp)
        @ufunction
        def CFunctionBPNativeEvent(self):
            log('T1Child.CFunctionBPNativeEvent, childprop:', self.childProp)
    log('a')
    c = Spawn(T1Child)
    log('b')
    c.CFunctionBPImplementable()
    log('c')
    c.CFunctionBPNativeEvent()
    log('d')
    c.get_py_proxy().CFunctionBPImplementable()
    log('e')
    c.get_py_proxy().CFunctionBPNativeEvent()
    log('f')

# Py call of C UFUNC exposed by bridge AND overridden by Py subclass and DOES call super impl
# BPviaPy call of C UFUNC exposed by bridge AND overridden by Py subclass and DOES call super impl
if 1:
    log('-- TEST 2 --')
    class T2Child(bridge.CChild):
        def __init__(self, *args, **kwargs):
            super().__init__(*args, **kwargs)
            self.childProp = 't2child'
        @ufunction
        def CFunctionBPImplementable(self):
            log('T2Child.CFunctionBPImplementable, childProp=',self.childProp)
            super().CFunctionBPImplementable() # note that there is nothing here, so no output expected from this
        @ufunction
        def CFunctionBPNativeEvent(self):
            log('T2Child.CFunctionBPNativeEvent, childProp=',self.childProp)
            super().CFunctionBPNativeEvent()
    log('a')
    c = Spawn(T2Child)
    log('b')
    c.get_py_proxy().CFunctionBPImplementable()
    log('c')
    c.get_py_proxy().CFunctionBPNativeEvent()
    log('d')
    c.CFunctionBPImplementable()
    log('e')
    c.CFunctionBPNativeEvent()
    log('f')

# Py call of Py UFUNC exposed in Py subclass
# BPviaPy call of Py UFUNC exposed in Py subclass
if 1:
    log('-- TEST 3 --')
    class T3Child(bridge.CChild):
        def __init__(self, *args, **kwargs):
            super().__init__(*args, **kwargs)
            self.childProp = 't3child'
        @ufunction
        def SomeFunc(self):
            log('T3Child.SomeFunc, childProp=', self.childProp)
    log('a')
    c = Spawn(T3Child)
    log('b')
    c.SomeFunc()
    log('c')
    c.get_py_proxy().SomeFunc()
    log('d')

# Py call of Py UFUNC exposed in Py subclass and NOT overridden by subsubclass
# BPviaPy call of Py UFUNC exposed in Py subclass and NOT overridden by subsubclass
if 1:
    log('-- TEST 4 --')
    class T4Parent(bridge.CChild):
        def __init__(self, *args, **kwargs):
            super().__init__(*args, **kwargs)
            self.parentProp = 't4parent'
        @ufunction
        def SomeFunc(self):
            log('T4Parent.SomeFunc, parentProp=', self.parentProp, 'childProp=', getattr(self, 'childProp',None))
    class T4Child(T4Parent):
        def __init__(self, *args, **kwargs):
            super().__init__(*args, **kwargs)
            self.childProp='t4child'
    log('a')
    c = Spawn(T4Child)
    log('b')
    c.get_py_proxy().SomeFunc()
    log('c')
    c.SomeFunc()
    log('d')

# Py call of Py UFUNC exposed in Py subclass AND overridden by subsubclass and DOES NOT call super impl
# BPviaPy call of Py UFUNC exposed in Py subclass AND overridden by subsubclass and DOES NOT call super impl
if 1:
    log('-- TEST 5 --')
    class T5Parent(bridge.CChild):
        def __init__(self, *args, **kwargs):
            super().__init__(*args, **kwargs)
            self.parentProp = 't5parent'
        @ufunction
        def SomeFunc(self):
            log('T5Parent.SomeFunc', self.parentProp, getattr(self, 'childProp', None))
    class T5Child(T5Parent):
        def __init__(self, *args, **kwargs):
            super().__init__(*args, **kwargs)
            self.childProp = 't5child'
        @ufunction
        def SomeFunc(self):
            log('T5Child.SomeFunc', self.childProp, self.parentProp)

    log('a')
    c = Spawn(T5Child)
    log('b')
    c.SomeFunc()
    log('c')
    c.get_py_proxy().SomeFunc()
    log('d')

# Py call of Py UFUNC exposed in Py subclass AND overridden by subsubclass and DOES call super impl
# BPviaPy call of Py UFUNC exposed in Py subclass AND overridden by subsubclass and DOES call super impl
if 1:
    log('-- TEST 6 --')
    class T6Parent(bridge.CChild):
        def __init__(self, *args, **kwargs):
            super().__init__(*args, **kwargs)
            self.parentProp = 't6parent'
        @ufunction
        def SomeFunc(self):
            log('T6Parent.SomeFunc', self.parentProp, getattr(self, 'childProp', None))
    class T6Child(T6Parent):
        def __init__(self, *args, **kwargs):
            super().__init__(*args, **kwargs)
            self.childProp = 't6child'
        @ufunction
        def SomeFunc(self):
            log('T6Child.SomeFunc', self.childProp, self.parentProp)
            super().SomeFunc()

    log('a')
    c = Spawn(T6Child)
    log('b')
    c.SomeFunc()
    log('c')
    c.get_py_proxy().SomeFunc()
    log('d')

# For now, gotta kinda test these by hand - after the above objs are created, wire them up in the
# level blueprint and see if calling their APIs reacts properly

# BP call of C UFUNC not overridden in Py subclass but exposed by bridge
# BP call of C UFUNC exposed by bridge AND overridden by Py subclass and DOES NOT call super impl
# BP call of C UFUNC exposed by bridge AND overridden by Py subclass and DOES call super impl
# BP call of Py UFUNC exposed in Py subclass
# BP call of Py UFUNC exposed in Py subclass and NOT overridden by subsubclass
# BP call of Py UFUNC exposed in Py subclass AND overridden by subsubclass and DOES NOT call super impl
# BP call of Py UFUNC exposed in Py subclass AND overridden by subsubclass and DOES call super impl

