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
import shutil
import sys
import tempfile
import time

# Own packages
import meanie3D
from . import __have_visit__
from . import external
from . import utils

# ----------------------------------------------------------------------------
if __have_visit__:
    external.locateCommands(['visit'])


# ----------------------------------------------------------------------------

def check_configuration(configuration):
    """
    Checks the consistency of the given configuration and hotfixes
    :param configuration:
    :return:
    """

    # Check the configuration
    dimensions = utils.getValueForKeyPath(configuration, 'data.dimensions')
    if not dimensions:
        print("ERROR:configuration must contain 'dimensions'")
        return -1
    if not len(dimensions) in [2, 3]:
        print("ERROR:Can only process 2D or 3D")
        return -1

    # Check that visualiseTracks has .vtk to work from
    postprocessing = utils.getValueForKeyPath(configuration, 'postprocessing')
    if postprocessing:
        tracks = utils.getValueForKeyPath(postprocessing, 'tracks')
        if tracks:
            if utils.getValueForKeyPath(tracks, 'visualiseTracks') and not \
                    utils.getValueForKeyPath(tracks, 'meanie3D-trackstats.vtk_tracks'):
                print("WARNING: tracks.visualiseTracks = True but tracks.meanie3D-trackstats.vtk_tracks "
                      "= False. Correcting.")
                utils.setValueForKeyPath(configuration, 'postprocessing.tracks.meanie3D-trackstats.vtk_tracks', True)

            # Complement vtk_dimensions to make our life a little easier down the road.
            if __have_visit__:
                vtkDimensions = utils.getValueForKeyPath(configuration, 'data.vtkDimensions')
                if vtkDimensions:
                    vtkDimString = ",".join(vtkDimensions)
                    utils.setValueForKeyPath(configuration,
                                             'postprocessing.tracks.meanie3D-trackstats.vtkDimensions',
                                             vtkDimString)

            # Make sure that plotStats has gnuplot files to work with
            if utils.getValueForKeyPath(tracks, 'plotStats') and not \
                    utils.getValueForKeyPath(tracks, 'meanie3D-trackstats.gnuplot'):
                print("WARNING: track.plot_stats = True but "
                      "tracks.meanie3D-trackstats.gnuplot = False. Correcting.")
                utils.setValueForKeyPath(configuration, 'postprocessing.tracks.meanie3D-trackstats.gnuplot', True)


# ----------------------------------------------------------------------------

def run_trackstats(configuration, directory):
    """
    Runs the meanie3D-trackstats command.
    :param configuration:
    :param directory:
    :return:
    """
    pconf = configuration['postprocessing']
    if 'tracks' not in pconf:
        print("Skipping meanie3D-trackstats because 'tracks' configuration is missing")
        return False

    tracks = pconf['tracks']
    if 'meanie3D-trackstats' not in tracks:
        print("Skipping meanie3D-trackstats because 'tracks.meanie3D-trackstats' configuration is missing")
        return False

    if utils.getValueForKeyPath(configuration, 'skip_trackstats'):
        print("Skipping meanie3D-trackstats because --skip-trackstats is present")
        return False

    print("Running meanie3D-trackstats for %s" % directory)
    conf = tracks['meanie3D-trackstats']

    os.chdir(directory)

    # Assemble command line params
    haveFiles = True
    params = []
    if conf['dictionary']:
        params.append("-t")
        haveFiles = haveFiles and os.path.exists('track-dictionary.json')
    if conf['gnuplot']:
        params.append("-g")
        haveFiles = haveFiles and os.path.exists('plot_stats.gp')
    if conf['length']:
        params.append("--create-length-statistics")
        haveFiles = haveFiles and os.path.exists("lengths.txt")
        haveFiles = haveFiles and os.path.exists("lengths-hist.txt")
    if conf['length_classes']:
        params.append("--length-histogram-classes=%s" % conf['length_classes'])
    if conf['speed']:
        params.append("--create-speed-statistics")
        haveFiles = haveFiles and os.path.exists("speeds.txt")
        haveFiles = haveFiles and os.path.exists("speeds-hist.txt")
    if conf['speed_classes']:
        params.append("--speed-histogram-classes=%s" % conf['speed_classes'])
    if conf['direction']:
        params.append("--create-direction-statistics")
        haveFiles = haveFiles and os.path.exists("directions.txt")
        haveFiles = haveFiles and os.path.exists("directions-hist.txt")
    if conf['direction_classes']:
        params.append("--direction-histogram-classes=%s" % conf['direction_classes'])
    if conf['size']:
        params.append("--create-cluster-statistics")
        haveFiles = haveFiles and os.path.exists("sizes.txt")
        haveFiles = haveFiles and os.path.exists("sizes-hist.txt")
    if conf['size_classes']:
        params.append("--cluster-histogram-classes=%s" % conf['size_classes'])
    if conf['cumulated']:
        params.append("--create-cumulated-size-statistics")
    if conf['cumulated_classes']:
        params.append("--size-histogram-classes=%s" % conf['cumulated_classes'])
    if conf['vtk_tracks']:
        params.append("--write-center-tracks-as-vtk ")
        # Can' be sure in this case, better safe than sorry
        haveFiles = False
    if __have_visit__ and conf['vtkDimensions']:
        params.append("--vtk-dimensions=%s" % conf['vtkDimensions'])
    params.append("-s netcdf")

    if 'resume' in configuration and haveFiles:
        print("Skipping meanie3D-trackstats")
        os.chdir("..")
        return True

    success = external.run("meanie3D-trackstats", " ".join(params))
    if not success:
        sys.exit(0)
    os.chdir("..")
    return success


# ----------------------------------------------------------------------------

def plot_trackstats(configuration, directory):
    """
    Runs gnuplot to produce .eps files
    :param configuration:
    :param directory:
    :return:
    """
    pconf = configuration['postprocessing']
    if 'tracks' not in pconf:
        return False
    tracks = pconf['tracks']
    if 'meanie3D-trackstats' not in tracks:
        return False

    conf = tracks['meanie3D-trackstats']
    print("Plotting .eps files for %s" % directory)

    os.chdir(directory)

    # create a tempfile for gnuplot
    f = open('plot_stats.gp', 'w')

    # track length
    if conf['length']:
        f.write('set term postscript\n')
        f.write('set output "lengths-hist.eps"\n')
        f.write('set title "Distribution of track length"\n')
        f.write('set xlabel "Track length in [#steps]"\n')
        f.write('set ylabel "Number of tracks"\n')
        f.write('set xtics 5\n')
        f.write('plot "lengths-hist.txt" with boxes\n')

    # sizes
    if conf['size']:
        f.write('# cluster size\n')
        f.write('set output "sizes-hist.eps"\n')
        f.write('set title "Distribution of cluster size"\n')
        f.write('set xlabel "log(cluster size in [#gridpoints])"\n')
        f.write('set ylabel "log(number of clusters)"\n')
        f.write('set logscale y\n')
        f.write('set logscale x\n')
        f.write('set xtics auto\n')
        f.write('plot "sizes-hist.txt" with boxes\n')
        f.write('unset logscale y\n')
        f.write('unset logscale x\n')

    # speed
    if conf['speed']:
        f.write('set output "speeds-hist.eps"\n')
        f.write('set title "Distribution of cluster speeds"\n')
        f.write('set xlabel "Cluster speed in [m/s]"\n')
        f.write('set ylabel "Number of clusters"\n')
        f.write('set xtics 2\n')
        f.write('plot "speeds-hist.txt" with boxes\n')

    # directions
    if conf['direction']:
        f.write('set output "directions-hist.eps"\n')
        f.write('set title "Distribution of tracking direction"\n')
        f.write('set xlabel "Cluster direction in [deg]"\n')
        f.write('set ylabel "Number of clusters"\n')
        f.write('set xtics 15\n')
        f.write('plot "directions-hist.txt" with boxes\n')
    f.close()
    success = external.run("gnuplot", "plot_stats.gp")
    os.chdir("..")
    return success


# ----------------------------------------------------------------------------

def plot_stats_comparison(configuration):
    """
    Plots comparison charts for each of the parameters to meanie3D-trackstats,
    namely--length, --directions, --speed and --direction
    :param configuration:
    :return:
    """
    scales = configuration['scales']
    if not scales:
        print("No scales found, no comparison is done.")
        return False

    pconf = configuration['postprocessing']
    if not pconf:
        return False

    if not utils.getValueForKeyPath(pconf, 'tracks.runScaleComparison'):
        return False

    conf = utils.getValueForKeyPath(pconf, 'tracks.meanie3D-trackstats')

    print("Plotting .eps files for comparison")

    # create a tempfile for gnuplot
    f = open('plot_stats_comparison.gp', 'w')

    f.write('set terminal png\n')
    f.write('set style fill transparent pattern 4 border 2\n')

    # track length
    if utils.getValueForKeyPath(conf, 'length'):
        f.write('set title "Distribution of track length"\n')
        f.write('set xlabel "Track length in [#steps]"\n')
        f.write('set ylabel "log(#tracks)"\n')
        f.write('set xtics 5\n')
        f.write('set xrange [2:50]\n')
        f.write('set logscale y\n')
        f.write('set output "lengths-hist-comparison.png"\n')
        f.write('plot ')
        for i in range(0, len(scales)):
            scale = scales[i]
            f.write('"scale%s/lengths-hist.txt" with linespoints title "t=%s"' % (scale, scale))
            if i < len(scales) - 1:
                f.write(",")
        f.write('\nunset logscale y\n')
        f.write('set autoscale x\n')

    # cluster size
    if utils.getValueForKeyPath(conf, 'size'):
        f.write('set title "Distribution of cluster size"\n')
        f.write('set xlabel "log(cluster size in [#gridpoints])"\n')
        f.write('set ylabel "number of clusters"\n')
        f.write('set logscale x\n')
        f.write('set xrange [10:10000]\n')
        f.write('set xtics auto\n')
        f.write('set output "sizes-hist-comparison.png"\n')
        f.write('plot ')
        for i in range(0, len(scales)):
            scale = scales[i]
            f.write('"scale%s/sizes-hist.txt" with linespoints title "t=%s"' % (scale, scale))
            if i < len(scales) - 1:
                f.write(",")
        f.write('\nunset logscale x\n')
        f.write('set autoscale x\n')

    # speed
    if utils.getValueForKeyPath(conf, 'speed'):
        f.write('set title "Distribution of cluster speeds"\n')
        f.write('set xlabel "Cluster speed in [m/s]"\n')
        f.write('set ylabel "Number of clusters"\n')
        f.write('set xtics 5\n')
        f.write('set output "speeds-hist-comparison.png"\n')
        f.write('plot ')
        for i in range(0, len(scales)):
            scale = scales[i]
            f.write('"scale%s/speeds-hist.txt" with linespoints title "t=%s"' % (scale, scale))
            if i < len(scales) - 1:
                f.write(",")
        f.write("\n")

    # directions
    if utils.getValueForKeyPath(conf, 'direction'):
        f.write('set title "Distribution of tracking direction"\n')
        f.write('set xlabel "Cluster direction in [deg]"\n')
        f.write('set ylabel "Number of clusters"\n')
        f.write('set xtics 30\n')
        f.write('set xrange [15:360]\n')
        f.write('set output "directions-hist-comparison.png"\n')
        f.write('plot ')
        for i in range(0, len(scales)):
            scale = scales[i]
            f.write('"scale%s/directions-hist.txt" with linespoints title "t=%s"' % (scale, scale))
            if i < len(scales) - 1:
                f.write(",")
        f.write("\n")
    f.close()

    success = external.run("gnuplot", "plot_stats_comparison.gp")
    os.chdir("..")
    return success


def run_comparison(configuration):
    """
    Runs cross-scale analysis.
    :param configuration:
    :return:
    """
    print("Running cross-scale comparison")
    return plot_stats_comparison(configuration)


# ----------------------------------------------------------------------------

def compile_and_run_template(template_path, configuration, replacements, directory):
    """
    Compiles the gievn template into an executable python
    script and runs it through visit.
    :param template_path:
    :param configuration:
    :param replacements:
    :param directory:
    :return:True if the command was successful, False else.
    """
    scriptFilename = tempfile.mktemp() + ".py"
    # scriptFilename = os.path.abspath("generated.py")
    # print("\tWriting python script for visualisation to: " + scriptFilename)
    with open(template_path) as infile, open(scriptFilename, 'w') as outfile:
        for line in infile:
            for src, target in replacements.iteritems():
                line = line.replace(src, target)
            outfile.write(line)
    # print("\tDone.")

    # Compile command line params for visualisation
    params = "-s %s" % scriptFilename
    runHeadless = utils.getValueForKeyPath(configuration, 'postprocessing.runVisitHeadless')
    if runHeadless:
        params = "-cli -nowin %s" % params

    # change into directory
    os.chdir(directory)

    # Run visualisation
    success = external.run('visit', params)

    # Change back out
    os.chdir('..')

    return success


# ----------------------------------------------------------------------------

def visualise_tracks(configuration, directory):
    """
    Visualises the tracks found.
    :param configuration:
    :param directory:
    :return:
    """
    print("Visualising tracks for %s" % directory)
    if not __have_visit__:
        print("Visit not found. Skipping.")
        return
    home = os.path.abspath(os.path.dirname(meanie3D.__file__) + os.path.sep + os.path.pardir)
    templatePath = home + os.path.sep + os.path.sep.join(("meanie3D", "resources", "tracks_visit.py"))
    replacements = {
        'P_TRACKS_DIR': os.path.abspath(directory),
        'P_RESUME': str(configuration['resume']),
        'P_M3D_HOME': home,
        'P_CONFIGURATION_FILE': os.path.abspath(configuration['config_file'])
    }

    return compile_and_run_template(templatePath, configuration, replacements, directory)


# ----------------------------------------------------------------------------

def visualise_clusters(configuration, directory):
    """
    Visualises the clusters.
    :param configuration:
    :param directory:
    :return:
    """
    print("Visualising clusters for %s" % directory)
    if not __have_visit__:
        print("Visit not found. Skipping.")
        return
    home = os.path.abspath(os.path.dirname(meanie3D.__file__) + os.path.sep + os.path.pardir)
    templatePath = home + os.path.sep + os.path.sep.join(("meanie3D", "resources", "clusters_visit.py"))
    replacements = {
        'P_CLUSTER_DIR': os.path.abspath(directory + os.path.sep + "netcdf"),
        'P_NETCDF_DIR': configuration['source_directory'],
        'P_RESUME': str(configuration['resume']),
        'P_M3D_HOME': home,
        'P_USES_TIME': str(configuration['uses_time']),
        'P_START_TIME_INDEX': str(configuration['start_time_index']),
        'P_END_TIME_INDEX': str(configuration['end_time_index']),
        'P_CONFIGURATION_FILE': os.path.abspath(configuration['config_file'])
    }
    return compile_and_run_template(templatePath, configuration, replacements, directory)


# ----------------------------------------------------------------------------

def copy_html_files(configuration, directory):
    """
    Copies the files required for html presentation.
    :param configuration:
    :param directory:
    :return:
    """
    home = os.path.abspath(os.path.dirname(meanie3D.__file__) + os.path.sep + os.path.pardir)
    path = home + os.path.sep + os.path.sep.join(("meanie3D", "resources", "index.html"))
    shutil.copy(path, os.path.abspath(directory))
    path = home + os.path.sep + os.path.sep.join(("meanie3D", "resources", "meanie3d.js"))
    shutil.copy(path, os.path.abspath(directory))
    path = home + os.path.sep + os.path.sep.join(("meanie3D", "resources", "ajax-loader.gif"))
    shutil.copy(path, os.path.abspath(directory))
    return


# ----------------------------------------------------------------------------

def cleanup(configuration, directory):
    """
    Removes any files that are not part of the results and were produced
    in any of the previous steps.
    :param configuration:
    :param directory:
    :return:
    """
    print("Cleaning temporary files for %s" % directory)
    os.chdir(directory)
    # if utils.getValueForKeyPath(configuration, 'postprocessing.tracks.meanie3D-trackstats.vtk_tracks'):
    #     for filename in glob.glob('*.vtk'):
    #         os.remove(filename)

    if utils.getValueForKeyPath(configuration, 'postprocessing.tracks.meanie3D-trackstats.gnuplot'):
        for filename in glob.glob('*.gp'):
            os.remove(filename)

    # if (os._exists("visitlog.py")):
    #     os.remove("visitlog.py")

    os.chdir("..")
    return


# ----------------------------------------------------------------------------

def run(configuration):
    """
    Runs postprocessing steps according to the section
    'postprocessing' in the configuration.
    :param configuration:
    :return:
    """

    # Check configuration section 'postprocessing'
    check_configuration(configuration)

    if 'postprocessing' not in configuration:
        return

    # In case scale parameters were given, the output dirs are scaleXYZ.
    # Otherwise it's 'clustering'. To be safe, iterate over both
    scales = configuration['scales']
    if scales:
        base = configuration["output_dir"] + os.path.sep + "scale"
        directories =  [ base+"%s" % scale for scale in scales ]
    else:
        directories = ['clustering']

    print("Processing directories: %s" % str(directories))
    for directory in directories:
        if os.path.isdir(directory):
            print("Processing %s" % directory)
        else:
            print("ERROR:%s is not a directory!" % directory)
            continue

        # run the track statistics
        start_time = 0
        if configuration['time_operations']:
            print("Running trackstats ...")
            start_time = time.time()

        if run_trackstats(configuration, directory):
            if configuration['time_operations']:
                print("Finished. (%.2f seconds)" % (time.time() - start_time))

            # Copy HTML files
            copy_html_files(configuration, directory)

            # run the stats plotting
            if utils.getValueForKeyPath(configuration, 'postprocessing.tracks.plotStats'):
                plot_trackstats(configuration, directory)

            # run the track visualisations
            if __have_visit__ and utils.getValueForKeyPath(configuration, 'postprocessing.tracks.visualiseTracks'):

                if configuration['time_operations']:
                    print("Visualising tracks ...")
                    start_time = time.time()

                visualise_tracks(configuration, directory)

                if configuration['time_operations']:
                    print("Finished. (%.2f seconds)" % (time.time() - start_time))

        if __have_visit__ and utils.getValueForKeyPath(configuration, 'postprocessing.clusters.visualiseClusters'):

            if configuration['time_operations']:
                print("Visualising clusters ...")
                start_time = time.time()

            visualise_clusters(configuration, directory)

            if configuration['time_operations']:
                print("Finished. (%.2f seconds)" % (time.time() - start_time))

        cleanup(configuration, directory)

    if utils.getValueForKeyPath(configuration, 'postprocessing.tracks.runScaleComparison'):
        run_comparison(configuration)

# ----------------------------------------------------------------------------
