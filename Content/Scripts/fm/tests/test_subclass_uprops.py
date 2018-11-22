'''
Testing UPROPERTYs
C++ testing assumes the C++ class defines 'intProp'
'''

from fm.common import *
import fm
bridge = fm.subclassing.bridge
uproperty = fm.subclassing.uproperty
from unreal_engine import FVector, FRotator, FTransform
from unreal_engine.structs import BoxSphereBounds as BSB
from unreal_engine.classes import Actor, StaticMesh, Pawn
from unreal_engine.enums import EControllerHand

sm = ue.load_object(StaticMesh, '/Game/Meshes/bar_chair')

if 0:
    class PROPTypes(bridge.CChild):
        #i = uproperty(int, 0)
        #b = uproperty(bool, False)
        #f = uproperty(float, 0.0)
        #v = uproperty(FVector, FVector())
        #r = uproperty(FRotator, FRotator())
        #t = uproperty(FTransform, FTransform())
        #e = uproperty(EControllerHand, EControllerHand.Left)
        #bs = uproperty(BSB,BSB())
        c = uproperty(Pawn, Pawn, is_class=True)
        a = uproperty(StaticMesh, sm)
    c = Spawn(PROPTypes)
    o = c.get_py_proxy(c)

# uprop from self
if 0:
    log('-- TEST 0 --')
    class T0Child(bridge.CChild):
        pyntProp = uproperty(int, 11)
    c = Spawn(T0Child)
    o = c.get_py_proxy(c)
    log('pyntProp:', c.pyntProp, o.pyntProp)
    c.pyntProp = 20
    log('pyntProp:', c.pyntProp, o.pyntProp)
    o.pyntProp = 30
    log('pyntProp:', c.pyntProp, o.pyntProp)

# uprop from py parent
if 0:
    log('-- TEST 1 --')
    class T1Parent(bridge.CChild):
        pyntProp = uproperty(int, 11)
    class T1Child(T1Parent):
        pass
    c = Spawn(T1Child)
    o = c.get_py_proxy(c)
    log('pyntProp:', c.pyntProp, o.pyntProp)
    c.pyntProp = 20
    log('pyntProp:', c.pyntProp, o.pyntProp)
    o.pyntProp = 30
    log('pyntProp:', c.pyntProp, o.pyntProp)

# uprop from py parent and self
if 0:
    log('-- TEST 2 --')
    class T2Parent(bridge.CChild):
        pyntProp = uproperty(int, 11)
    class T2Child(T2Parent):
        pyntProp = uproperty(int, 15)
    c = Spawn(T2Child)
    o = c.get_py_proxy(c)
    log('pyntProp:', c.pyntProp, o.pyntProp)
    c.pyntProp = 20
    log('pyntProp:', c.pyntProp, o.pyntProp)
    o.pyntProp = 30
    log('pyntProp:', c.pyntProp, o.pyntProp)

# uprop from C
if 0:
    log('-- TEST 3 --')
    class T3Parent(bridge.CChild):
        pass
    class T3Child(T3Parent):
        pass
    c = Spawn(T3Child)
    o = c.get_py_proxy(c)
    log('intProp:', c.intProp, o.intProp)
    c.intProp = 20
    log('intProp:', c.intProp, o.intProp)
    o.intProp = 30
    log('intProp:', c.intProp, o.intProp)

# uprop from C and parent
if 0:
    log('-- TEST 4 --')
    class T4Parent(bridge.CChild):
        intProp = uproperty(int, 100)
    class T4Child(T4Parent):
        pass
    c = Spawn(T4Child)
    o = c.get_py_proxy(c)
    log('intProp:', c.intProp, o.intProp)
    c.intProp = 20
    log('intProp:', c.intProp, o.intProp)
    o.intProp = 30
    log('intProp:', c.intProp, o.intProp)

# uprop from C and self
if 0:
    log('-- TEST 5 --')
    class T5Parent(bridge.CChild):
        pass
    class T5Child(T5Parent):
        intProp = uproperty(int, 500)
    c = Spawn(T5Child)
    o = c.get_py_proxy(c)
    log('intProp:', c.intProp, o.intProp)
    c.intProp = 20
    log('intProp:', c.intProp, o.intProp)
    o.intProp = 30
    log('intProp:', c.intProp, o.intProp)

# uprop from C and parent and self
if 0:
    log('-- TEST 6 --')
    class T6Parent(bridge.CChild):
        intProp = uproperty(int, 300)
    class T6Child(T6Parent):
        intProp = uproperty(int, 400)
    c = Spawn(T6Child)
    o = c.get_py_proxy(c)
    log('intProp:', c.intProp, o.intProp)
    c.intProp = 20
    log('intProp:', c.intProp, o.intProp)
    o.intProp = 30
    log('intProp:', c.intProp, o.intProp)

# Note: don't forget to wire the above up in BP to test prop r/w

