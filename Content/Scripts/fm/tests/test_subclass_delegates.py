'''
Testing the use of delegates
- py exposes an event others can bind to
- py fires event and others receive it
- py binds to an event somebody else exposes
- py properly receives event when fired
- py can unbind from event
'''

from fm.common import *
log('== test_subclass_delegates ==')
import fm
bridge = fm.subclassing.bridge
uproperty = fm.subclassing.uproperty
ufunction = fm.subclassing.ufunction

if 1:
    log('-- TEST 0 --')
    class Foo(bridge.CChild):
        @ufunction
        def MyDispatcher(self, b:bool, i:int, s:str):
            log('Foo.MyDispatcher called for', self, b, i, s)

    foo = Spawn(Foo)
    from unreal_engine.classes import EventFiringPawn_C
    firing = Spawn(EventFiringPawn_C)
    firing.bind_event('MyDispatcher', foo.MyDispatcher)
