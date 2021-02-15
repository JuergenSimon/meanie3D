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

import glob
import os
# import pprint

import visit

import utils


def run(conf):
    """
    Plots a list of tracks from .vtk files produced by meanie3D-trackstats --write-center-tracks-as-vtk.
    :param conf: Configuration dictionary
    :return:
    """
    # pp = pprint.PrettyPrinter()
    # pp.pprint(conf)

    # Make sure the global configuration is in place
    utils.run_global_visit_configuration(conf)

    visitConf = utils.getValueForKeyPath(conf, 'postprocessing.tracks.visit')
    if not visitConf:
        print("No configuration for visuals. Nothing to do.")
        return 0

    # Set up background gradient, axis labels etc.
    utils.setAnnotations(conf, 'postprocessing.tracks.visit.annotationAttributes')

    # Set the view straight
    utils.setView(conf, 'postprocessing.tracks.visit.view')

    # Plot the map data
    utils.plotMapdata(conf, 'postprocessing.tracks.visit.map')

    # Plot the tracks
    trackPlotConf = utils.getValueForKeyPath(conf, 'postprocessing.tracks.visit.track')
    # pp.pprint(trackPlotConf)

    currentDirectory = os.path.abspath(os.getcwd())
    os.chdir(conf['tracks_dir'])

    if trackPlotConf:

        # Save value of legend flag
        legendFlag = trackPlotConf['PseudocolorAttributes']['legendFlag']

        # Plot the Tracks
        # track_pattern = conf['tracks_dir'] + "/*-track_*.vtk"

        track_pattern = "*-track_*.vtk"
        _list = sorted(glob.glob(track_pattern))
        print("Looking with pattern " + track_pattern)
        print("Found %d track files." % len(_list))
        count = 0
        for trackFile in _list:

            # add plot
            # trackFile = conf['tracks_dir'] + os.path.sep + fname

            # plot the legend for the first one only
            if (count == 1) and legendFlag:
                trackPlotConf['PseudocolorAttributes']['legendFlag'] = 0
                # pp.pprint(trackPlotConf)

            # Plot the actual track data
            _file = conf['tracks_dir'] + os.path.sep + trackFile
            print("Adding plot for " + _file)
            utils.addPseudocolorPlot(_file, trackPlotConf)

            count = count + 1

            # in case the script is being debugged, exit the script
            # after 10 tracks. This could be configured
            # if getValueForKeyPath(conf,'postprocessing.debugVisitScript') and count > 10

        # Restore flag value
        trackPlotConf['PseudocolorAttributes']['legendFlag'] = legendFlag
        # pp.pprint(trackPlotConf)

    print("Drawing plots")
    visit.DrawPlots()

    print("Saving image to %s" % os.getcwd())
    utils.saveImage("tracks", 0)

    os.chdir(currentDirectory)

    return
