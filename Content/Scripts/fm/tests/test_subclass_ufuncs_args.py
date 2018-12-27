'''
Testing multi-arg ufunctions and a wide range of allowed arg types in ufunctions
'''
from fm.common import *
import fm
import typing
bridge = fm.subclassing.bridge
ufunction = fm.subclassing.ufunction
from unreal_engine import FVector, FRotator, FTransform, FColor, FLinearColor
from unreal_engine.classes import ScriptStruct
MyStruct = ue.load_object(ScriptStruct, '/Game/MyStruct.MyStruct')
from unreal_engine.structs import BoxSphereBounds as BSB
from unreal_engine.enums import EControllerHand

if 0:
    from unreal_engine.classes import BPActor_C
    m = MyStruct()
    m.af = [101, 102, 103, 104]
    m.b = True
    m.i = 100
    m.s = 'homer'
    m.v = FVector(1000,2000,3000)
    m.ai = [55, 56, 57, 58]
    m.mii = {1:10, 2:20, 3:30, 4:40}
    c = Spawn(BPActor_C)
    c.Hey(m)
    c.Hey(m)

if 0:
    m = MyStruct()
    m.af = [101, 102, 103, 104]
    m.b = True
    m.i = 100
    m.s = 'homer'
    m.v = FVector(1000,2000,3000)
    m.ai = [55, 56, 57, 58]
    m.mii = {1:10, 2:20, 3:30, 4:40}
    def Dump(ms):
        #log('bisv:', ms.b, ms.i, ms.s, ms.v)
        #log('i,s:', ms.i, ms.s)
        #log('af:', ms.af)
        #log('ai:', ms.ai)
        #log('mii:', ms.mii)
        pass

    class T3(bridge.CChild):
        @ufunction
        def Go(self, m:MyStruct):
            Dump(m)
    c = Spawn(T3)
    #c.Go(m)
    #c.Go(m)

# basic types: bool, int, float, string
if 0:
    log('-- TEST 0 --')
    class T0Child(bridge.CChild):
        @ufunction
        def MyFunc(self, b:bool, i:int, f:float, s:str):
            log('T0.Child', repr(b), repr(i), repr(f), repr(s))

    log('a')
    c = Spawn(T0Child)
    log('b')
    c.MyFunc(True, 11, 55.5, 'what is your name?')
    log('c')
    c.get_py_proxy().MyFunc(True, 101, 505.05, 'this is teh thing')
    log('d')

# core builtin structs: FVector, FRotator, FTransform, FColor, FLinearColor
if 0:
    log('-- TEST 1 --')
    class T1Child(bridge.CChild):
        @ufunction
        def MyFunc(self, v:FVector, r:FRotator, t:FTransform, c:FColor, l:FLinearColor):
            log('v:', v)
            log('r:', r)
            log('t:', t)
            log('c:', c)
            log('l:', l)

    log('a')
    c = Spawn(T1Child)
    log('b')
    t = FTransform()
    t.translation = FVector(2.1, 3.2, 4.3)
    t.rotation = FRotator(4.1, 4.2, 4.3)
    t.scale = FVector(5.5, 6.6, 7.7)
    c.MyFunc(FVector(11, 12, 13), FRotator(14, 15, 16), t, FColor(50, 51, 52, 53), FLinearColor(0.1, 0.2, 0.3, 0.4))
    log('c')
    c.get_py_proxy().MyFunc(FVector(11, 12, 13), FRotator(14, 15, 16), t, FColor(50, 51, 52, 53), FLinearColor(0.1, 0.2, 0.3, 0.4))
    log('d')

# enums
if 0:
    log('-- TEST 2 --')
    class T2(bridge.CChild):
        @ufunction
        def MyFunc(self, e:EControllerHand):
            log('e:', e, e == EControllerHand.Right)

    log('a')
    c = Spawn(T2)
    log('b')
    c.MyFunc(EControllerHand.Right)
    log('c')
    c.get_py_proxy().MyFunc(EControllerHand.Right)
    log('d')

# UStructs
if 0:
    log('-- TEST 3 --')
    class T3(bridge.CChild):
        @ufunction
        def Go(self, m:MyStruct):
            log('m.b:', m.b)
            log('m.i:', m.i)
            for x in m.af:
                log('m.af...:', x)

    log('a')
    c = Spawn(T3)
    log('b')
    b = BSB()
    b.boxextent = FVector(100, 101, 102)
    b.origin = FVector(200, 201, 202)
    b.sphereradius = 5551
    m = MyStruct()
    m.b = True
    m.i = 127
    m.af = [5.1, 6.2, 7.3, 8.4, 9.5]
    c.Go(m)
    c.Go(m)
    c.Go(m)
    c.Go(m)
    log('c')
    #c.get_py_proxy().Go(m)
    #c.get_py_proxy().Go(m)
    log('d')

# obj of a certain type
if 0:
    log('-- TEST 4 --')
    from unreal_engine.classes import Actor, Pawn, BlueprintGeneratedClass
    MyPawn_C = ue.load_object(BlueprintGeneratedClass, '/Game/MyPawn.MyPawn_C')
    from unreal_engine.classes import Actor
    class T4(bridge.CChild):
        @ufunction
        def Go(self, o:MyPawn_C):
            log('T4.Go got actor:', o, o.get_actor_bounds()[1])
            log('T4.Go now calling Biff on that actor')
            o.Biff(5)

    log('a')
    mp = Spawn(MyPawn_C)
    c = Spawn(T4)
    log('b')
    c.Go(mp)

# class object
if 0:
    log('-- TEST 5 --')
    from unreal_engine.classes import Actor, Pawn, BlueprintGeneratedClass
    MyPawn_C = ue.load_object(BlueprintGeneratedClass, '/Game/MyPawn.MyPawn_C')
    class T5(bridge.CChild):
        @ufunction
        def Go(self, u:typing.Type[Pawn]):
            log('u:', u)

    log('a')
    c = Spawn(T5)
    log('b')
    c.Go(MyPawn_C)

# obj that implements an interface
if 1:
    log('-- TEST 6 --')
    from unreal_engine.classes import Actor, Pawn, BlueprintGeneratedClass
    MyInterface_C = ue.load_object(BlueprintGeneratedClass, '/Game/MyInterface.MyInterface_C')
    AnotherPawn_C = ue.load_object(BlueprintGeneratedClass, '/Game/AnotherPawn.AnotherPawn_C')
    class T6(bridge.CChild):
        @ufunction
        def Go(self, u:MyInterface_C):
            log('u:', u)
            u.Happy(123)

    log('a')
    c = Spawn(T6)
    a = Spawn(AnotherPawn_C)
    log('b')
    c.Go(a)

