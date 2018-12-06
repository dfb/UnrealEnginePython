'''
Testing support for interfaces
'''
from fm.common import *
import fm
import typing
bridge = fm.subclassing.bridge
ufunction = fm.subclassing.ufunction
uproperty = fm.subclassing.uproperty

# class implements an interface
if 1:
    log('-- TEST 0 --')
    from unreal_engine.classes import BlueprintGeneratedClass
    MyInterface_C = ue.load_object(BlueprintGeneratedClass, '/Game/MyInterface.MyInterface_C')

    class T0(bridge.CChild):
        __interfaces__ = [MyInterface_C]
        def __init__(self, *args, **kwargs):
            super().__init__(*args, **kwargs)
            self.c = 10

        @ufunction
        def SomeOtherFunc(self):
            pass

        @ufunction
        def Another(self):
            pass

        @ufunction
        def Happy(self, intParam:int):
            log('T0.Happy, woot!', intParam)

        @ufunction
        def CanHaveCrown(self, *args) -> bool:
            log('T0.CanYaveCrownnnn', args)
            return True

        @ufunction
        def GetCounter(self) -> int:
            self.c += 1
            log('T0.GetCounter', self.c)
            return self.c

        if 0:
            @ufunction
            def MultiRet(self) -> [int, bool]:
                log('T0.MultiRet', self.c)
                return self.c, self.c % 2 == 0

        @ufunction
        def Another2(self):
            pass

    log('a')
    c = Spawn(T0)
    log('b')
    c.Happy(123)
    # Now test in BP - e.g. get all actors that implement this interface and call Happy on them

# func that takes an arg that implements an interface
if 0:
    log('-- TEST 1 --')
    from unreal_engine.classes import Actor, Pawn, BlueprintGeneratedClass
    MyInterface_C = ue.load_object(BlueprintGeneratedClass, '/Game/MyInterface.MyInterface_C')
    AnotherPawn_C = ue.load_object(BlueprintGeneratedClass, '/Game/AnotherPawn.AnotherPawn_C')
    class T1(bridge.CChild):
        @ufunction
        def Go(self, u:MyInterface_C):
            log('u:', u)
            u.Happy(123)

    log('a')
    c = Spawn(T1)
    a = Spawn(AnotherPawn_C)
    log('b')
    c.Go(a)

# uprop of something that implements an interface
if 0:
    log('-- TEST 2 --')
    from unreal_engine.classes import Actor, Pawn, BlueprintGeneratedClass
    MyInterface_C = ue.load_object(BlueprintGeneratedClass, '/Game/MyInterface.MyInterface_C')
    AnotherPawn_C = ue.load_object(BlueprintGeneratedClass, '/Game/AnotherPawn.AnotherPawn_C')
    class T2(bridge.CChild):
        ifProp = uproperty(MyInterface_C, None)

        @ufunction
        def Go(self):
            log('u:', self.ifProp)
            if self.ifProp is not None:
                self.ifProp.Happy(1111)

    log('a')
    c = Spawn(T2)
    a = Spawn(AnotherPawn_C)
    c.ifProp = a
    log('b')
    c.Go(c)

