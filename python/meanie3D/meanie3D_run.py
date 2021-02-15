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

import getopt
import os
import sys

import meanie3D
import meanie3D.app
import meanie3D.app.external
import meanie3D.app.postprocessing
import meanie3D.app.tracking
import meanie3D.app.utils

# Make sure the C++ executables are installed
meanie3D.app.external.locateCommands(["meanie3D-detect", "meanie3D-track", "meanie3D-trackstats", "rm"])


def usage():
    """
    Prints usage information.
    :return:
    """
    print("meanie3D -c=/path/to/json -f=/path/to/netcdf [-o /path/to/output] "
          "[-s=<scale>] [--start=t1 --end=t2]  [--resume,-r] [--help,-h] "
          "[--version] [--time-operations]")
    print("runs a complete set of netcdf files through the clustering/tracking")
    print("-c : json config file specifying variables etc.")
    print("-f : directory containing the files. It is assumed that")
    print("           the files are in the correct order when sorted alphabetically.")
    print("-o : Output directory. If omitted, this is the same as the input directory")
    print("--scales,-s  : comma separated list of scale parameters.")
    print("--ranges,-r  : comma separated list of bandwidths, one for each spatial and value range variable.")
    print("--json-example : prints an example .json configuration and exits")
    print("--start index of time step to start with in files with time dimension")
    print("--end   index of time step to end at in files with time dimension")
    print("--resume     : if this flag is present, the algorithm assumes to resume")
    print("              operations where it last left off. If not present, previous")
    print("              results will be erased before starting")
    print("--help, -h  : print this message and exit.")
    print("--version   : prints the version information and exits")
    print("--time-operations: time operations and print out their elapsed time")
    print("--skip-trackstats: skip meanie3D-trackstats, even if the configuration is present")


def print_configuration_format():
    """
    Prints the configuration file .json file description
    :return:
    """
    print('{')
    print('   "description" : "...",  /* Description of file content */')
    print('   "data" : {...},         /* call --help config.data for help on this section */')
    print('   "detection" : {...},    /* call --help config.detection for help on this section */')
    print('   "tracking" : {...},     /* call --help config.tracking for help on this section */')
    print('   "postprocessing" : {...} /* call --help config.postprocessing for help on this section */')
    print('}')


def print_version():
    """
    Prints version info
    :return:
    """
    print("meanie3D: " + meanie3D.getVersion() + "\n")


# ----------------------------------------------------------------------------

# ----------------------------------------------------------------------------
## Main function
def main():
    # Parse command line
    try:
        argv = sys.argv[1:]
        long_args = ["json-example",
                     "resume",
                     "help",
                     "version",
                     "start=",
                     "end=",
                     "time-operations",
                     "skip-trackstats",
                     "inline-tracking",
                     "ranges=",
                     "scales="]
        opts, args = getopt.getopt(argv, "c:f:s:o:r:h", long_args)
    except getopt.GetoptError as detail:
        print(detail)
        sys.exit(2)

    scales = []
    ranges = []
    resume = False
    time_operations = False
    skip_trackstats = False
    inline_tracking = False
    config_file = ""
    output_dir = "."
    netcdf_dir = ""
    num_params = 0
    start_time_index = -1
    end_time_index = -1

    for o, a in opts:

        if o == "-c":
            config_file = os.path.expandvars(a)
            num_params = num_params + 1

        elif o == "-f":
            netcdf_dir = a
            num_params = num_params + 1

        elif o in ["--scales", "-s"]:
            scales = str(a).split(',')

        elif o in ["--ranges", "-r"]:
            ranges = a

        elif o == "-o":
            output_dir = a

        elif o in ["--json-example"]:
            print_configuration_format()
            sys.exit(0)

        elif o in ["--resume"]:
            resume = True

        elif o in ["--start"]:
            start_time_index = int(float(a))

        elif o in ["--end"]:
            end_time_index = int(float(a))

        elif o in ["--help"]:
            usage()
            sys.exit(0)

        elif o in ["--version"]:
            print_version()
            sys.exit(0)

        elif o in ["--time-operations"]:
            time_operations = True

        elif o in ["--skip-trackstats"]:
            skip_trackstats = True

        elif o in ["--inline-tracking"]:
            inline_tracking = True

        else:
            usage()

    if num_params < 2:
        usage()
        sys.exit(1)

    uses_time = False

    # sanity checks on time range

    if start_time_index != -1 or end_time_index != -1:
        if (start_time_index == -1 and end_time_index != -1) or (start_time_index != -1 and end_time_index == -1):
            print("start-time-index and end-time-index must both be set")
            sys.exit(1)
        if start_time_index > end_time_index:
            print("start-time-index is after end-time-index!")
            sys.exit(1)
        uses_time = True

    # Parse configuration data and expand
    configuration = meanie3D.app.utils.load_configuration(config_file)

    print("-------------------------------------------------------------------------------------")
    print("Configuration: " + configuration['description'])
    print("-------------------------------------------------------------------------------------")

    # Enrich the configuration with env/command line stuff
    configuration["source_directory"] = os.path.abspath(netcdf_dir)
    configuration["output_dir"] = os.path.abspath(output_dir)
    configuration["resume"] = resume
    configuration['config_file'] = os.path.abspath(config_file)
    configuration['time_operations'] = time_operations
    configuration['skip_trackstats'] = skip_trackstats
    configuration['scales'] = scales
    configuration['ranges'] = ranges
    configuration['uses_time'] = uses_time
    configuration['start_time_index'] = start_time_index
    configuration['end_time_index'] = end_time_index
    configuration['inline_tracking'] = inline_tracking

    # Remove previous results
    if resume == False:
        if not meanie3D.app.utils.removeOutputDirectories(configuration):
            return

    # Run the detection and tracking steps
    detection = meanie3D.app.utils.getSafe(configuration, 'detection')
    tracking = meanie3D.app.utils.getSafe(configuration, 'tracking')
    if detection or tracking:

        # run the actual clustering/tracking script
        if not scales:
            if uses_time == False:
                meanie3D.app.tracking.run(configuration, -1)
            else:
                for time_index in range(start_time_index, end_time_index):
                    meanie3D.app.tracking.run(configuration, time_index)

        else:
            for scale in scales:
                configuration["scale"] = scale
                if uses_time == False:
                    meanie3D.app.tracking.run(configuration, -1)
                else:
                    for time_index in range(start_time_index, end_time_index):
                        meanie3D.app.tracking.run(configuration, time_index)

    # Run the postprocessing steps
    if 'postprocessing' in configuration:
        meanie3D.app.postprocessing.run(configuration)

    return


# ----------------------------------------------------------------------------

# ----------------------------------------------------------------------------
# entry point
# ----------------------------------------------------------------------------

if __name__ == "__main__":
    main()
