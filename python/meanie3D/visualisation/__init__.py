import sys
import meanie3D.app.utils

__all__ = ["clusters", "tracks", "utils"]
__have_visit = None

# Import visit package
__visitPath__, __visitImportPath__ = meanie3D.app.utils.findVisitPaths()
if __visitImportPath__ and __visitPath__:
    __have_visit = True
    sys.path.append(__visitImportPath__)
else:
    __have_visit = False
    sys.stderr.write("\nERROR:could not find visit import path. Visualisation is switched off.\n")
