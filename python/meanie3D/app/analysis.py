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
import sys

import meanie3D


def loadTrackingDictionary(path):
    """
    Loads a tracking dictionary as output by meanie3D-trackstats -t
    TODO: implement me
    :param path:
    :return: parsed JSON dictionary
    """
    return


def constructTreeJson(trackingDictionary):
    """
    Parses a tracking dictionary and creates data for visualising
    the tracks in d3.js
    TODO: implement me
    :param trackingDictionary:
    :return:
    """


def showTrackTree(tree):
    """
    Displays the given tree JSON in d3.js
    TODO: implement me
    :param tree:
    :return:
    """
    return


def usage():
    """
    Prints help and exits
    :return:
    """
    print("meanie3D-trackgraph -f <tracking dictionary file>")
    print("Analyses a track dictionary and shows a track graph with splits, merges etc.")
    print("-f : tracking dictionary file")
    print("--help, -h  : print this message and exits.")
    print("--version   : prints the version information and exits")


def run():
    """
    Parses the command line and performs the analysis of the track dictionary.
    :return:
    """

    # Parse command line
    try:
        argv = sys.argv[1:]
        opts, args = getopt.getopt(argv, "c:f:s:o:r:h", ["resume", "help", "version", "start=", "end="])
    except getopt.GetoptError as detail:
        print(detail)
        sys.exit(2)

    num_params = 0
    for o, a in opts:
        if o in ['--file', '-f']:
            num_params = num_params + 1
        elif o in ["--help"]:
            usage()
            sys.exit(0)
        elif o in ["--version"]:
            meanie3D.getVersion()
            sys.exit(0)
        else:
            usage()
            sys.exit(1)
    if num_params < 2:
        usage()
