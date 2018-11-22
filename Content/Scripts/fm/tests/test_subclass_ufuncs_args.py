'''
Testing multi-arg ufunctions and a wide range of allowed arg types in ufunctions
'''
from fm.common import *
import fm
bridge = fm.subclassing.bridge
ufunction = fm.subclassing.ufunction

# basic types: bool, int, float, string
if 1:
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
# obj: obj of a certain type, class of a certain type, obj that implements an interface
# builtin UStructs
# custom UStructs
# enums
# array, map, set args

