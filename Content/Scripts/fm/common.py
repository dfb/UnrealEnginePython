'''
Commonly used stuffs
(1) Try to make this always safe for other modules to do 'from common import *'
(2) Importing this module MUST be safe to do in dev as well as in production
'''

import traceback, time, os, sys, json
import unreal_engine as ue
from unreal_engine import UObject, FVector, FRotator, FTransform

class Bag(dict):
    def __setattr__(self, k, v): self[k] = v

    def __getattr__(self, k):
        try: return self[k]
        except KeyError: raise AttributeError('No such attribute %r' % k)

    def __delattr__(self, k):
        try: del self[k]
        except KeyError: raise AttributeError('No such attribute %r' % k)

    @staticmethod
    def FromJSON(j):
        return json.loads(j, object_pairs_hook=Bag)

    def ToJSON(self, indent=0):
        if indent > 0:
            return json.dumps(self, indent=indent, sort_keys=True)
        return json.dumps(self)

def log(*args):
    print(' '.join(str(x) for x in args))

def logTB():
    for line in traceback.format_exc().split('\n'):
        log(line)

def ModusUserDir():
    '''Returns the full path to the user's Modus VR directory under ~/Documents'''
    return os.path.join(os.path.abspath(os.path.expanduser('~')), 'Documents', 'Modus VR')

# Convenience flags for detecting if we're in dev or in a shipping game
IN_DEV = hasattr(ue, 'get_editor_world') # True when we're in the editor (*including* PIE!!!)
IN_GAME = not IN_DEV # True when we're running in a packaged game

# During packaging/cooking, the build will fail with strange errors as it tries to load these modules, so
# prj.py sets a flag for us to let us know that the build is going on
BUILDING = (not not int(os.environ.get('MODUS_IS_BUILDING', '0')))

try:
    from unreal_engine.enums import EWorldType
except ImportError:
    # guess they haven't implemented it yet
    class EWorldType:
        NONE, Game, Editor, PIE, EditorPreview, GamePreview, Inactive = range(7)

def GetWorld():
    '''Returns the best guess of what the "current" world to use is'''
    worlds = {} # worldType -> *first* world of that type
    for w in ue.all_worlds():
        t = w.get_world_type()
        if worlds.get(t) is None:
            worlds[t] = w

    return worlds.get(EWorldType.Game) or worlds.get(EWorldType.PIE) or worlds.get(EWorldType.Editor)

def Spawn(cls, world=None):
    '''General purpose spawn function - spawns an actor and returns it. If no world is provided, finds one
    using GetWorld. cls can be:
    - the name of the class as a string, in which case it will be imported from unreal_engine.classes
    - a class previously imported from unreal_engine.classes
    - a Python class created via the fm.subclassing module
    '''
    world = world or GetWorld()
    if isinstance(cls, str):
        import unreal_engine.classes as c
        cls = getattr(c, cls)
    else:
        # see if it's one of our subclassing ones
        engineClass = getattr(cls, 'engineClass', None)
        if engineClass is not None:
            cls = engineClass
    return world.actor_spawn(cls)


