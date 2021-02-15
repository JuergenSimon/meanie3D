#!/usr/bin/python

"""
The MIT License (MIT)

(c) Juergen Simon 2014 (juergen.simon@uni-bonn.de)

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.
"""

__author__ = "Juergen Simon"
__email__ = "juergen.simon@uni-bonn.de"
__version__ = "1.5.4"

# This is a template for a python script to be created with
# template variables replaced and then executed inside of
# Visit. The purpose is to visualise tracks from .vtk files
# produced by meanie3D-trackstats --write-center-tracks-as-vtk

# Parameters
_TRACKS_DIR = "P_TRACKS_DIR"
_M3D_HOME = "P_M3D_HOME"
_CONFIG_FILE = "P_CONFIGURATION_FILE"
_RESUME = "P_RESUME"

# Import modules
import sys
sys.path.append(_M3D_HOME)
import meanie3D.visualisation.tracks

# Parse configuration data
configuration = meanie3D.app.utils.load_configuration(_CONFIG_FILE)

# Add parsed parameters
configuration["meanie3d_home"] = _M3D_HOME
configuration["tracks_dir"] = _TRACKS_DIR
configuration['resume'] = meanie3D.app.utils.strToBool(_RESUME)

# run it
# pdb.set_trace()
meanie3D.visualisation.tracks.run(configuration)
quit()
