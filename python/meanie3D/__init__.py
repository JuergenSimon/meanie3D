__author__ = 'Juergen Simon'
__email__ = 'tachyonimpulse@gmail.com'
__version__ = '1.6.1'
__url__ = 'http://git.meteo.uni-bonn.de/projects/meanie3d'
__all__ = ['app', 'visualisation', 'resources']

import os.path
import sys


def getVersion():
    """
    :return:meanie3D package version
    """
    from . import __version__
    return __version__


def getHome():
    """
    meanie3D package location
    :return:
    """
    return os.path.abspath(os.path.dirname(__file__))


def appendSystemPythonPath():
    """
    Returns the system's python path to import outside modules.
    """
    system_python_path = ":/Library/Frameworks/SQLite3.framework/Versions/C/Python/2.7:/Library/Frameworks/GEOS.framework/Versions/3/Python/2.7:/Library/Python/2.7/site-packages/numpy-override:/Library/Python/2.7/site-packages/matplotlib-override:/Library/Frameworks/GDAL.framework/Versions/1.11/Python/2.7/site-packages:/Library/Python/2.7/site-packages/pip-1.5.2-py2.7.egg:/System/Library/Frameworks/Python.framework/Versions/2.7/lib/python27.zip:/System/Library/Frameworks/Python.framework/Versions/2.7/lib/python2.7:/System/Library/Frameworks/Python.framework/Versions/2.7/lib/python2.7/plat-darwin:/System/Library/Frameworks/Python.framework/Versions/2.7/lib/python2.7/plat-mac:/System/Library/Frameworks/Python.framework/Versions/2.7/lib/python2.7/plat-mac/lib-scriptpackages:/System/Library/Frameworks/Python.framework/Versions/2.7/lib/python2.7/lib-tk:/System/Library/Frameworks/Python.framework/Versions/2.7/lib/python2.7/lib-old:/System/Library/Frameworks/Python.framework/Versions/2.7/lib/python2.7/lib-dynload:/Library/Python/2.7/site-packages:/System/Library/Frameworks/Python.framework/Versions/2.7/Extras/lib/python:/System/Library/Frameworks/Python.framework/Versions/2.7/Extras/lib/python/PyObjC".strip()
    paths = system_python_path.split(':')
    for path in paths:
        if path:
            sys.path.append(path)
