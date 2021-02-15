__all__ = ["analysis", "external", "postprocessing", "utils", "tracking"]
__have_visit__ = None
__visitPath__ = None
__visitImportPath__ = None

import os
import sys

from meanie3D.app.utils import findVisitPaths

# Import visit package
__visitPath__, __visitImportPath__ = findVisitPaths()
if __visitImportPath__ and __visitPath__:
    __have_visit__ = True
    sys.path.append(__visitImportPath__)
    sys.path.append(__visitImportPath__ + os.path.sep + 'visit')
else:
    __have_visit__ = False
    sys.stderr.write("\nERROR:could not find visit import path. Visualisation is switched off.\n")
