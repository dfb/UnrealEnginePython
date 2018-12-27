'''
Testing arguments and properties that are lists
- py class has a ufunc with a list arg
- py calling py list ufunc
- py calling bp list ufunc
- bp calling py list ufunc
- py class has a ufunc that returns a list
- py calling py list ret ufunc
- py calling bp list ret ufunc
- bp calling py list ret ufunc
- py class has a uprop that is a list
'''

from fm.common import *
import fm
bridge = fm.subclassing.bridge
uproperty = fm.subclassing.uproperty

