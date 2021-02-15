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
import os.path
import sys
import time
from subprocess import call

from meanie3D.app import external, utils


def checkConfiguration(conf):
    """
    Checks the configuration for mandatory items etc.
    :param conf: The configuration to check.
    :except: RuntimeError in case something is amiss.
    :return:
    """
    if not conf['data']:
        raise RuntimeError("Missing 'data' section")

    data = conf['data']
    if "variables" not in data:
        raise RuntimeError("Missing 'data.variables'")
    if "dimensions" not in data:
        raise RuntimeError("Missing 'data.dimensions'")

    # TODO: finish this
    return


def run(config, time_index):
    """
    Runs a batch of files through the clustering and tracking.
    :param config: configuration with the following keys:
        description     : a description of the configuration
        netcdf_dir      : directory containing files to process
        output_dir      : directory to write results to
        m3d_home        : ${MEANIE3D_HOME} environment variable
        resume          : if True then pick off where you left off, if False all previous results
                            are deleted and processing starts from scratch
        meanie3D-detect : parameters for meanie3D-detect
        meanie3D-track  : parameters for meanie3D-track
        scale           : scale parameter (optional)
        use_previous    : True: using previous results to enhance tracking. Absent or False:
                            no use of previous results.
        use_ci_score    : Use the CI-Score algorithm
    :param time_index:  Time index to start at.
    :return:
    """
    checkConfiguration(config)
    data = utils.getSafe(config, 'data')
    detection = utils.getSafe(config, 'detection')
    tracking = utils.getSafe(config, 'tracking')

    # Piece together the command line params for detection and tracking
    tracking_params = ""
    detect_params = ""

    if data and detection:

        variables = list()
        lowerThresholds = list()
        upperThresholds = list()
        replacementValues = list()

        detect_params = "%s -d %s" % (detect_params, ','.join(data['dimensions']))

        for variable in data['variables']:
            variables.append(variable['name'])
            if utils.getSafe(variable, 'lowerThreshold'):
                lowerThresholds.append("%s=%s" % (variable['name'], str(variable['lowerThreshold'])))

            if utils.getSafe(variable, 'upperThreshold'):
                upperThresholds.append("%s=%s" % (variable['name'], str(variable['upperThreshold'])))

            if utils.getSafe(variable, 'replacementValue'):
                replacementValues.append("%s=%s" % (variable['name'], str(variable['replacementValue'])))

        detect_params += " -v %s" % ','.join(variables)

        if lowerThresholds:
            detect_params += " --lower-thresholds %s" % ','.join(lowerThresholds)

        if upperThresholds:
            detect_params += " --upper-thresholds %s" % ','.join(upperThresholds)

        if replacementValues:
            detect_params += " --replacement-values %s" % ','.join(lowerThresholds)

        if utils.getSafe(data, 'vtkDimensions'):
            detect_params += " --vtk-dimensions %s" % ','.join(data['vtkDimensions'])

    if detection:
        minClusterSize = utils.getSafe(detection, 'minClusterSize')
        if minClusterSize:
            detect_params = "%s -m %d" % (detect_params, minClusterSize)

        additionalParams = utils.getSafe(detection, 'meanie3D-detect')
        if detect_params:
            detect_params = "%s %s" % (detect_params, additionalParams)

    if tracking:

        if utils.getSafe(tracking, 'meanie3D-track'):
            tracking_params = "%s %s" % (tracking_params, tracking['meanie3D-track'])

        if utils.getSafe(tracking, 'histogramVariable'):
            tracking_params = "%s --tracking-variable %s" % (tracking_params, tracking['histogramVariable'])

        if utils.getSafe(tracking, 'histogramWeight'):
            tracking_params = "%s --wt %s" % (tracking_params, tracking['histogramWeight'])
        else:
            tracking_params = "%s --wt 0" % tracking_params

        if utils.getSafe(tracking, 'positionWeight'):
            tracking_params = "%s --wr %s" % (tracking_params, tracking['positionWeight'])
        else:
            tracking_params = "%s --wr 0" % tracking_params

        if utils.getSafe(tracking, 'sizeWeight'):
            tracking_params = "%s --ws %s" % (tracking_params, tracking['sizeWeight'])
        else:
            tracking_params = "%s --ws 0" % tracking_params

        if utils.getSafe(tracking, 'maxSpeed'):
            tracking_params = "%s --max-speed %f" % (tracking_params, tracking['maxSpeed'])

        if utils.getSafe(tracking, 'maxTime'):
            tracking_params = "%s --max-time %d" % (tracking_params, tracking['maxTime'])

        if utils.getSafe(tracking, 'maxSizeDeviation'):
            tracking_params = "%s --max-size-deviation %d" % (tracking_params, tracking['maxSizeDeviation'])

        if utils.getSafe(tracking, 'useDisplacementVectors'):
            tracking_params = "%s --use-displacement-vectors" % tracking_params

    resume_at_index = 0

    # In case a scale T parameter is given, the output dir
    # is scale<T>. Otherwise it's 'clustering'
    output_dir = config['output_dir']
    scale = utils.getValueForKeyPath(config, 'scale')
    if scale:
        output_dir = output_dir + "/scale" + scale
    else:
        output_dir = output_dir + "/clustering"

    # --range ?
    bandwidth = utils.getValueForKeyPath(config, 'ranges')

    # Resume?
    if not utils.getValueForKeyPath(config, 'resume'):
        # consider time index. Even if not resuming, the
        # output directories should only be created at
        # the first time step
        if time_index <= 0:
            # TODO: consider scenario where user opted not to remove previous results?
            print("Removing results from previous runs")
            utils.create_ouput_directories(output_dir)
    else:
        resume_at_index = utils.number_of_netcdf_files(output_dir + "/netcdf")
        if time_index >= 0 and time_index < resume_at_index:
            return
        print("Resuming at index %s" % str(resume_at_index))

    # Get a list of the files we need to process.
    source = utils.getValueForKeyPath(config, 'source_directory')
    if os.path.isdir(source):
        # Multiple files in a directory
        netcdf_list = sorted(glob.glob(source + os.path.sep + '*.nc'))
    else:
        # Single file
        netcdf_list = [source]

    last_cluster_file = None
    run_count = 0
    if time_index >= 0:
        run_count = time_index + 1

    # Decide if the tracking is run in an individual step
    # or inside of meanie3D-detect
    inline_tracking = utils.getValueForKeyPath(config, 'inline_tracking')

    for netcdf_file in netcdf_list:

        basename = os.path.basename(netcdf_file)
        cluster_file = output_dir + os.path.sep + "netcdf" + os.path.sep + os.path.splitext(basename)[0]
        if time_index >= 0:
            last_cluster_file = cluster_file + "-" + str(time_index - 1) + "-clusters.nc"
            cluster_file = cluster_file + "-" + str(time_index)
        cluster_file += "-clusters.nc"

        print("-------------------------------------------------------------------------------------")
        print("Processing " + netcdf_file)
        print("-------------------------------------------------------------------------------------")

        # ----------------------------------------------
        # Clustering
        # ----------------------------------------------

        if detection:

            # if there is a resume counter, keep skipping until the count is right
            # Note: this only applies to clustering, which is expensive. Tracking
            # will be re-run
            if (resume_at_index > 0) and (run_count <= resume_at_index):
                run_count = run_count + 1
                continue

            print("-- Clustering --")

            # build the clustering command
            params = detect_params

            # amend for time index and make proper logfile name
            if time_index < 0:
                logfile = output_dir + "/log/clustering_" + str(run_count) + ".log"
            else:
                params = params + "  --time-index " + str(time_index)
                logfile = output_dir + "/log/clustering_" + str(time_index) + ".log"

            # scale?
            if scale and scale is not 'None':
                scale_param = " --scale " + scale
                params += scale_param

            # Bandwidth?
            if bandwidth:
                params += (" --ranges " + bandwidth)

            # use previous result to enhance current or incorporate
            # tracking step in detection runs?
            have_previous = ((run_count > 0) or (time_index > 0)) and last_cluster_file
            if (inline_tracking or detection['usePrevious']) and have_previous:
                params += " --previous-output " + os.path.abspath(last_cluster_file)

            if inline_tracking and have_previous:
                params += (" --inline-tracking %s" % tracking_params)

            if utils.getValueForKeyPath(detection, "usePrevious") and have_previous:
                params += " --postprocess-with-previous-output"

            # add ci-comparison-file if applicable
            if run_count >= 3 and utils.getSafe(detection, 'useCIScore'):
                params += " --ci-comparison-file " + os.path.abspath(netcdf_list[run_count - 3])
                proto_file = "protoclusters-" + os.path.splitext(os.path.basename(netcdf_list[run_count - 3]))[
                    0] + ".nc"
                params += " --ci-comparison-protocluster-file " + os.path.abspath(proto_file)

            # Input file
            params += " --file %s --output %s" % (os.path.abspath(netcdf_file), os.path.abspath(cluster_file))

            # complete command with directing output to logfile
            params = "%s > %s" % (params, os.path.abspath(logfile))

            # execute
            start_time = 0
            if config['time_operations']:
                start_time = time.time()

            success = external.run("meanie3D-detect", params, return_output=True)
            if not success:
                print("ERROR: could not perform detection")
                sys.exit(1)

            if config['time_operations']:
                print("Finished. (%.2f seconds)" % (time.time() - start_time))

        # ----------------------------------------------
        # Tracking
        # ----------------------------------------------

        if tracking and not inline_tracking:
            # if we have a previous scan, run the tracking command
            if (time_index <= 0 and (run_count > 0)) or (time_index > 0):

                if time_index < 0:
                    logfile = output_dir + "/log/tracking_" + str(run_count) + ".log"
                else:
                    logfile = output_dir + "/log/tracking_" + str(time_index) + ".log"

                print("-- Tracking --")
                params = tracking_params + " --previous  %s --current %s " \
                         % (os.path.abspath(last_cluster_file), os.path.abspath(cluster_file))
                params = params + " > " + os.path.abspath(logfile)

                # execute
                start_time = 0
                if config['time_operations']:
                    start_time = time.time()

                success = external.run("meanie3D-track", params, return_output=True)
                if not success:
                    print("ERROR: could not perform detection")
                    sys.exit(1)

                if config['time_operations']:
                    print("Finished. (%.2f seconds)" % (time.time() - start_time))

        # keep track
        last_cluster_file = cluster_file

        # don't forget to increment run counter
        run_count = (run_count + 1)

        # Clean out the trash if there is any
        if config['cleanup_vtk']:
            print("Cleaning up *.vt*")
            call("rm -f *.vt*", shell=True)

    print("Done.")
    return
