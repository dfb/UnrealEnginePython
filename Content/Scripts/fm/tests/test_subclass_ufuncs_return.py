'''
Testing simple return values from UFUNCTIONS
'''

from fm.common import *
import fm
bridge = fm.subclassing.bridge
ufunction = fm.subclassing.ufunction

# ufunc in py child only
if 1:
    log('-- TEST 0 --')
    class T0Child(bridge.CChild):
        @ufunction
        def GetInt(self) -> int:
            log('T0Child.GetInt')
            return 55

    log('a')
    c = Spawn(T0Child)
    log('b')
    log(c.GetInt())
    log('c')
    log(c.get_py_proxy().GetInt())
    log('d')

# ufunc in c++ parent only
if 1:
    log('-- TEST 1 --')
    class T1Child(bridge.CChild):
        pass

    log('a')
    c = Spawn(T1Child)
    o = c.get_py_proxy()
    log('b')
    log('IntFuncBPC:', c.IntFuncBPC(), o.IntFuncBPC())
    log('IntFuncBPI:', c.IntFuncBPI(), o.IntFuncBPI())
    log('IntFuncBPN:', c.IntFuncBPN(), o.IntFuncBPN())
    log('e')

# ufunc in py parent and not overridden in child
if 1:
    log('-- TEST 2 --')
    class T2Parent(bridge.CChild):
        @ufunction
        def Gimme(self) -> int:
            log('T2Parent.Gimme')
            return 123
    class T2Child(T2Parent):
        pass
    log('a')
    c = Spawn(T2Child)
    o = c.get_py_proxy()
    log('b')
    log('Gimme', c.Gimme(), o.Gimme())
    log('c')

# ufunc in py parent and overridden in child
if 1:
    log('-- TEST 3 --')
    class T3Parent(bridge.CChild):
        @ufunction
        def Gimme(self) -> int:
            log('T3Parent.Gimme')
            return 123
    class T3Child(T3Parent):
        @ufunction
        def Gimme(self) -> int:
            log('T3Child.Gimme')
            return super().Gimme() * 10
    log('a')
    c = Spawn(T3Child)
    o = c.get_py_proxy()
    log('b')
    log('Gimme', c.Gimme(), o.Gimme())
    log('c')

# ufunc in c++ parent and not overridden in py parent or child
if 1:
    log('-- TEST 4 --')
    class T4Parent(bridge.CChild):
        pass
    class T4Child(T4Parent):
        pass
    log('a')
    c = Spawn(T4Child)
    o = c.get_py_proxy()
    log('b')
    log('IntFuncBPC:', c.IntFuncBPC(), o.IntFuncBPC())
    log('IntFuncBPI:', c.IntFuncBPI(), o.IntFuncBPI())
    log('IntFuncBPN:', c.IntFuncBPN(), o.IntFuncBPN())
    log('e')

# ufunc in c++ parent and overridden in py parent but not child
if 1:
    log('-- TEST 5 --')
    class T5Parent(bridge.CChild):
        @ufunction
        def IntFuncBPC(self) -> int:
            log('T5Parent.IntFuncBPC')
            return super().IntFuncBPC() * 10
        @ufunction
        def IntFuncBPI(self) -> int:
            log('T5Parent.IntFuncBPI')
            return (super().IntFuncBPI() or 1) * 100
        @ufunction
        def IntFuncBPN(self) -> int:
            log('T5Parent.IntFuncBPN')
            return super().IntFuncBPN() * 1000
    class T5Child(T5Parent):
        pass
    log('a')
    c = Spawn(T5Child)
    o = c.get_py_proxy()
    log('b')
    log('IntFuncBPC:', c.IntFuncBPC(), o.IntFuncBPC())
    log('IntFuncBPI:', c.IntFuncBPI(), o.IntFuncBPI())
    log('IntFuncBPN:', c.IntFuncBPN(), o.IntFuncBPN())
    log('e')

# ufunc in c++ parent and overridden in py parent and in child
if 1:
    log('-- TEST 6 --')
    class T6Parent(bridge.CChild):
        @ufunction
        def IntFuncBPC(self) -> int:
            log('T6Parent.IntFuncBPC')
            return super().IntFuncBPC() * 10
        @ufunction
        def IntFuncBPI(self) -> int:
            log('T6Parent.IntFuncBPI')
            return (super().IntFuncBPI() or 1) * 100
        @ufunction
        def IntFuncBPN(self) -> int:
            log('T6Parent.IntFuncBPN')
            return super().IntFuncBPN() * 1000
    class T6Child(T6Parent):
        @ufunction
        def IntFuncBPC(self) -> int:
            log('T6Child.IntFuncBPC')
            return super().IntFuncBPC() +3
        @ufunction
        def IntFuncBPI(self) -> int:
            log('T6Child.IntFuncBPI')
            return super().IntFuncBPI() + 4
        @ufunction
        def IntFuncBPN(self) -> int:
            log('T6Child.IntFuncBPN')
            return super().IntFuncBPN() + 5
    log('a')
    c = Spawn(T6Child)
    o = c.get_py_proxy()
    log('b')
    log('IntFuncBPC:', c.IntFuncBPC(), o.IntFuncBPC())
    log('IntFuncBPI:', c.IntFuncBPI(), o.IntFuncBPI())
    log('IntFuncBPN:', c.IntFuncBPN(), o.IntFuncBPN())
    log('e')

# ufunc in c++ parent and not overridden in py parent and overridden in child
if 1:
    log('-- TEST 7 --')
    class T7Parent(bridge.CChild):
        pass
    class T7Child(T7Parent):
        @ufunction
        def IntFuncBPC(self) -> int:
            log('T7Child.IntFuncBPC')
            return super().IntFuncBPC() +3
        @ufunction
        def IntFuncBPI(self) -> int:
            log('T7Child.IntFuncBPI')
            return super().IntFuncBPI() + 4
        @ufunction
        def IntFuncBPN(self) -> int:
            log('T7Child.IntFuncBPN')
            return super().IntFuncBPN() + 5
    log('a')
    c = Spawn(T7Child)
    o = c.get_py_proxy()
    log('b')
    log('IntFuncBPC:', c.IntFuncBPC(), o.IntFuncBPC())
    log('IntFuncBPI:', c.IntFuncBPI(), o.IntFuncBPI())
    log('IntFuncBPN:', c.IntFuncBPN(), o.IntFuncBPN())
    log('e')

# NOTE: after spawning the above objects, you also need to try out calls to them
# in BP (e.g. in the level blueprint using manual refs to them)
