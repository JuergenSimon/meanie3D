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
import subprocess
import sys
import time
import pprint

import visit

# Own packages
import meanie3D.visualisation
import utils
from meanie3D.app import external

__have_vtk__ = external.hasCommand('meanie3D-cfm2vtk')


def add_clusters(cluster_vtk_file, configuration):
    """
    Adds clusters with names "*_infix_*.vtk" to the current visualisation window.
    The clusters are colored according to the cluster_colors colortable,
    which is created at the beginning of the visualisation code.

    :param cluster_vtk_file:
    :param configuration: Options to overwrite the options the clusters are
    plotted with. Clusters are added as pseudocolor plots. For information
    on the available options, check PseudocolorAttributes()
    :return:
    """
    # Get the configuration
    clusterOptions = utils.getValueForKeyPath(configuration, 'postprocessing.clusters.visit.cluster')
    if not clusterOptions:
        sys.stderr.write("ERROR:no value found for postprocessing.clusters.visit.cluster")
        return
    # plot the clusters
    print("Adding clusters from file %s" % cluster_vtk_file)
    utils.addPseudocolorPlot(cluster_vtk_file, clusterOptions)
    return


def run(conf, print_conf=False):
    """
    Visualises clusters of a given run.
    :param conf:
    :return:
    """
    if print_conf:
        pp = pprint.PrettyPrinter()
        pp.pprint(conf)

    # Make sure the global configuration is in place
    utils.run_global_visit_configuration(conf)
    # pdb.set_trace()

    clusterConf = utils.getValueForKeyPath(conf, 'postprocessing.clusters')
    if not clusterConf:
        print("No configuration for cluster postprocessing. Nothing to do.")
        return 0

    visitConf = utils.getValueForKeyPath(clusterConf, 'visit')
    if not visitConf:
        print("No configuration for visuals. Nothing to do.")
        return 0

    views = utils.getValueForKeyPath(visitConf, 'views')

    # Set up background gradient, axis labels etc.
    utils.setAnnotations(visitConf, 'annotationAttributes')

    # Set view and annotation attributes
    utils.setAnnotations(conf, 'postprocessing.clusters.visit.annotations')

    if not utils.getValueForKeyPath(conf, 'resume'):
        print("Removing results from previous runs")
        subprocess.call("rm -rf images movies *.vtk *.vtr *tracking*.png *source*.png", shell=True)
    else:
        print("Removing intermediary files from previous runs")
        subprocess.call("rm -f *.vtk *.vtr *.vtu", shell=True)

    # Figure out the ending for the cluster vtk files
    conversion_config = utils.getValueForKeyPath(conf, 'postprocessing.clusters.meanie3D-cfm2vtk')
    if "--write-as-xml" in conversion_config:
        cluster_vtk_extension = ".vtu"
    else:
        cluster_vtk_extension = ".vtk"

    # Glob the netcdf directory or find the single file
    uses_time = utils.getValueForKeyPath(conf, 'uses_time')

    print("Current work directory: " + os.path.abspath(os.getcwd()))
    if uses_time:
        print("Processing file " + conf['source_directory'])
        netcdf_file = conf['source_directory']
    else:
        print("Processing files in directory " + conf['source_directory'])
        netcdf_files = sorted(glob.glob(conf['source_directory'] + "/*.nc"))

    # Keep track of number of images to allow
    # forced re-set in time to circumvent the
    # Visit memory leak
    image_count = 0

    if uses_time:
        t1 = int(utils.getValueForKeyPath(conf, 'start_time_index'))
        t2 = int(utils.getValueForKeyPath(conf, 'end_time_index'))
        index_range = range(t1, t2 + 1)
    else:
        index_range = range(len(netcdf_files))
        time_index = -1

    for index in index_range:

        # construct the cluster filename and find it
        # in the cluster directory
        if not uses_time:
            netcdf_file = netcdf_files[index]
        else:
            time_index = index

        netcdf_path, filename = os.path.split(netcdf_file)
        basename = os.path.splitext(filename)[0]
        if uses_time:
            basename = basename + "-" + str(time_index)
        cluster_file = conf['cluster_directory'] + os.path.sep + basename + "-clusters.nc"

        # cluster file gets it's basename from the input file rather than the cluster file
        cluster_vtk_file = os.path.splitext(filename)[0] + "-clusters" + cluster_vtk_extension

        # label and displacement files are based on the cluster file name
        label_vtk_file = basename + "-clusters-centers.vtk"
        displacement_vtk_file = basename + "-clusters-displacements.vtk"

        print("netcdf_file  = " + netcdf_file)
        print("cluster_file = " + cluster_file)

        # check if the files both exist
        if not os.path.exists(cluster_file):
            print("Cluster file does not exist. Skipping.")
            continue

        # predict the filenames for checking on resume
        number_postfix = str(image_count).rjust(4, '0')

        source_open = False
        skip_source = False

        if conf['resume']:
            exists = utils.images_exist(visitConf['views'], "source", image_count)
            if exists == "all":
                print("Source visualization " + number_postfix + " exists. Skipping.")
                skip_source = True
            elif exists == "partial":
                print("Deleting partial visualization " + number_postfix)
                utils.delete_images(conf, "source", image_count)

        if not skip_source and utils.getValueForKeyPath(clusterConf, 'createSourceMovie'):

            # Add ancillary background data
            utils.plotMapdata(visitConf, 'map')

            # Add timestamp
            if utils.getValueForKeyPath(clusterConf, 'showDateTime'):
                utils.add_datetime(clusterConf, netcdf_file, time_index)

            # Add source data and threshold it
            print("Plotting source data ...")
            start_time = time.time()

            utils.addPseudocolorPlots(netcdf_file, visitConf, 'source.plots', time_index)
            source_open = True
            visit.DrawPlots()

            utils.saveImagesForViews(views, "source")

            visit.DeleteAllPlots()
            visit.ClearWindow()

            print("    done. (%.2f seconds)" % (time.time() - start_time))

        if utils.getValueForKeyPath(clusterConf, 'visualiseClusters') and __have_vtk__:
            skip = False
            if conf['resume']:

                exists = utils.images_exist(visitConf['views'], "tracking", image_count)
                if exists == "all":
                    print("Cluster visualization " + number_postfix + " exists. Skipping.")
                    skip = True
                elif exists == "partial":
                    print("Deleting partial cluster visualization " + number_postfix)
                    utils.delete_images(conf, "tracking", image_count)

            if not skip:

                # Run the conversion
                print("-- Converting clusters to .vtr --")
                start_time = time.time()
                params = "-f %s %s" \
                         % (cluster_file, utils.getValueForKeyPath(conf, 'postprocessing.clusters.meanie3D-cfm2vtk'))
                if utils.getValueForKeyPath(conf, 'postprocessing.clusters.showDisplacementVectors'):
                    params += " --write-displacement-vectors"

                if utils.getValueForKeyPath(conf, 'data.vtkDimensions'):
                    vtkDimString = ",".join(utils.getValueForKeyPath(conf, 'data.vtkDimensions'))
                    params += " --vtk-dimensions=%s" % vtkDimString

                # pdb.set_trace();
                success = external.run('meanie3D-cfm2vtk', params)
                if not success:
                    sys.exit(1)
                print("    done. (%.2f seconds)" % (time.time() - start_time))

                # Move cluster output file to individual file
                if uses_time:
                    cluster_vtk_file_dst = basename + "-clusters.vtk"
                    os.rename(cluster_vtk_file, cluster_vtk_file_dst)
                    cluster_vtk_file = cluster_vtk_file_dst

                print("-- Rendering cluster scene --")
                start_time = time.time()

                # Add ancillary background data
                utils.plotMapdata(visitConf, 'map')

                # Add timestamp
                if utils.getValueForKeyPath(clusterConf, 'showDateTime'):
                    utils.add_datetime(clusterConf, netcdf_file, time_index)

                # Add background source data
                if utils.getValueForKeyPath(clusterConf, 'showSourceBackground'):
                    utils.addPseudocolorPlots(netcdf_file, visitConf, 'sourceBackground.plots', time_index)
                    source_open = True

                # Add the clusters
                add_clusters(cluster_vtk_file, conf)

                # Add modes as labels
                labelConf = utils.getValueForKeyPath(visitConf, 'label')
                if labelConf:
                    utils.addLabelPlot(label_vtk_file, labelConf)

                # Add displacement vectors
                if utils.getValueForKeyPath(clusterConf, 'showDisplacementVectors'):
                    vectorConf = utils.getValueForKeyPath(visitConf, 'displacementVectors')
                    if vectorConf:
                        utils.addVectorPlot(displacement_vtk_file, vectorConf)

                visit.DrawPlots()
                utils.saveImagesForViews(views, "tracking")

                print("    done. (%.2f seconds)" % (time.time() - start_time))

        # clean up

        visit.DeleteAllPlots()
        visit.ClearWindow()
        if source_open:
            visit.CloseDatabase(netcdf_file)
            visit.CloseDatabase(label_vtk_file)
        utils.close_pattern(basename + "*.vtr")
        utils.close_pattern(basename + "*.vtk")
        utils.close_pattern(basename + "*.vtu")

        if utils.getValueForKeyPath(conf, 'cleanup_vtk'):
            subprocess.call("rm -f *.vt*", shell=True)

        # periodically kill computing engine to
        # work around the memory leak fix
        image_count = image_count + 1

        # TODO: check if this is still necessary and re-implement if it is.
        # if image_count % 100 == 0:
        #    visit.CloseComputeEngine()

    # close mapstuff
    # TODO: might need to keep track of open database files and
    # close them.

    # Use imagemagick to use image sequences to make movies

    movieFormats = utils.getValueForKeyPath(clusterConf, 'movieFormats')
    if utils.getValueForKeyPath(clusterConf, 'createSourceMovie'):
        utils.createMoviesForViews(views, "source", movieFormats)

    if utils.getValueForKeyPath(clusterConf, 'createClusterMovie'):
        utils.createMoviesForViews(views, "tracking", movieFormats)

    # clean up
    print("Cleaning up ...")
    subprocess.call("mkdir images", shell=True)
    subprocess.call("mv *tracking_*.png images", shell=True)
    subprocess.call("mv *source_*.png images", shell=True)
    subprocess.call("mkdir movies", shell=True)
    subprocess.call("mv *source*.gif *tracking*.gif *.m4v movies", shell=True)
    if utils.getValueForKeyPath(conf, 'cleanup_vtk'):
        subprocess.call("rm -f *.vt* visitlog.py", shell=True)
    return
