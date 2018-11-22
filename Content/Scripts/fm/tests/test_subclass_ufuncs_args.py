'''
Testing multi-arg ufunctions and a wide range of allowed arg types in ufunctions
'''
from fm.common import *
import fm
bridge = fm.subclassing.bridge
ufunction = fm.subclassing.ufunction
from unreal_engine import FVector, FRotator, FTransform, FColor, FLinearColor
from unreal_engine.structs import BoxSphereBounds as BSB
from unreal_engine.enums import EControllerHand

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
if 1:
    log('-- TEST 2 --')
    class T2(bridge.CChild):
        @ufunction
        def MyFunc(self, e:EControllerHand):
            log('e:', e)

    log('a')
    c = Spawn(T2)
    log('b')
    c.MyFunc(EControllerHand.Right)
    log('c')
    c.get_py_proxy().MyFunc(EControllerHand.Right)
    log('d')

# builtin UStructs
# custom UStructs
# obj: obj of a certain type, class of a certain type, obj that implements an interface
# array, map, set args

