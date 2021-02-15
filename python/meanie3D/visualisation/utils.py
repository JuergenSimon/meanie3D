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
from subprocess import call

from meanie3D.app import external, utils

# make sure external commands are available
external.locateCommands(['convert', 'composite', 'python'])

success, paths_string = external.run('python', '-c "import sys; print sys.path"', return_output=True)
if success:
    home = os.path.expanduser('~')
    searchPaths = ['/usr/lib', '/usr/local/lib', home + os.path.sep + '.pyenv/versions']
    print ("Attempting to locate python module netCDF4 in paths %s" % ",".join(searchPaths))
    # print("Looking in paths:")
    # print(searchPaths)
    result = utils.find_in_paths(searchPaths, "netCDF4", "site-packages")
    if not result:
        result = utils.find_in_paths(searchPaths, "netCDF4", "dist-packages")
    if not result:
        print("Failed to locate python module netCDF4")
        exit(1)
    else:
        print("Found netCDF4 at %s" % result)
        sys.path.append(os.path.split(result)[0])
        # print("Python path after adding system search directories:")
        # print(sys.path)
else:
    print("Failed to obtain system's python path")
    exit(1)

# Now that the path is extended, import the rest
import netCDF4
import visit

# ---------------------------------------------------------
# Global configuration handling
# ---------------------------------------------------------

# Global variable to help making sure the global visit configuration
# is only ran twice to avoid adding multiple color tables etc.
did_global_config_execute = False


def run_global_visit_configuration(configuration):
    """
    Creates any configuration items regarded as global,
    such as creating color tables and named views.
    :param configuration:
    :return:
    """
    # Only run this once
    global did_global_config_execute
    if did_global_config_execute:
        return

    # Color tables
    if getValueForKeyPath(configuration, 'postprocessing.visit.colorTables'):
        createColorTables(configuration, 'postprocessing.visit.colorTables')

    saveWindowAttributes = getValueForKeyPath(configuration, 'postprocessing.visit.saveWindowAttributes')
    if saveWindowAttributes:
        s = visit.GetSaveWindowAttributes()
        updateVisitObjectFromDictionary(s, saveWindowAttributes)
        visit.SetSaveWindowAttributes(s)

    # Level or verbosity
    visit.SuppressMessages(2)
    visit.SuppressQueryOutputOn()

    did_global_config_execute = True
    return


# ---------------------------------------------------------
#
# Plotting functions, helpers
#
# ---------------------------------------------------------

def addPseudocolorPlot(db_file, configuration, time_index=-1):
    """
    Adds a pseudocolor plot
    :param db_file:
    :param configuration:
        {
            "variable":"X",
            "PseudocolorAttributes": {
               see visit.PseudocolorAttributes()
            },
            "ThresholdAttributes" : {
                see visit.ThresholdAttributes()
            }
        }
    :param time_index:
    :return:
    """
    variable = getValueForKeyPath(configuration, 'variable')
    attributes = getValueForKeyPath(configuration, 'PseudocolorAttributes')
    if variable and attributes:

        # Open data file
        visit.OpenDatabase(db_file)

        # Add the plot
        visit.AddPlot("Pseudocolor", variable)

        # Set time slider if necessary
        if time_index >= 0:
            visit.SetTimeSliderState(time_index)

        p = visit.PseudocolorAttributes()
        updateVisitObjectFromDictionary(p, attributes)
        visit.SetPlotOptions(p)

        # Threshold?
        threshold = getValueForKeyPath(configuration, "ThresholdAttributes")
        if threshold:
            visit.AddOperator("Threshold")
            t = visit.ThresholdAttributes()
            updateVisitObjectFromDictionary(t, threshold)
            visit.SetOperatorOptions(t)
    return


def addContourPlot(db_file, configuration):
    """
    Adds a contour plot.
    :param db_file:
    :param configuration:
        {
            "variable":"X",
            "ContourAttributes": {
               see visit.ContourAttributes()
            },
            "ThresholdAttributes" : {
                see visit.ThresholdAttributes()
            }
        }
    :return:
    """
    variable = getValueForKeyPath(configuration, 'variable')
    attributes = getValueForKeyPath(configuration, 'ContourAttributes')
    if variable and attributes:
        # Open data file
        visit.OpenDatabase(db_file)
        # Add the plot
        visit.AddPlot("Contour", variable)
        p = visit.ContourAttributes()
        updateVisitObjectFromDictionary(p, attributes)
        visit.SetPlotOptions(p)
        # Threshold?
        threshold = getValueForKeyPath(configuration, "ThresholdAttributes")
        if threshold:
            visit.AddOperator("Threshold")
            t = visit.ThresholdAttributes()
            updateVisitObjectFromDictionary(t, threshold)
            visit.SetOperatorOptions(t)
    return


def addVectorPlot(db_file, configuration):
    """
    Adds a vector plot.
    :param db_file:
    :param configuration:
        {
            "variable":"X",
            "VectorAttributes": {
               see visit.VectorAttributes()
            }
        }
    :return:
    """
    variable = getValueForKeyPath(configuration, 'variable')
    attributes = getValueForKeyPath(configuration, 'VectorAttributes')
    if variable and attributes:
        visit.OpenDatabase(db_file)
        visit.AddPlot("Vector", variable)
        p = visit.VectorAttributes()
        updateVisitObjectFromDictionary(p, attributes)
        visit.SetPlotOptions(p)
    return


def addLabelPlot(db_file, configuration):
    """
    Adds a label plot.
    :param db_file:
    :param configuration:
        {
            "variable":"X",
            "LabelAttributes": {
               see visit.LabelAttributes()
            }
        }
    :return:
    """
    variable = getValueForKeyPath(configuration, 'variable')
    attributes = getValueForKeyPath(configuration, 'LabelAttributes')
    if variable and attributes:
        visit.OpenDatabase(db_file)
        visit.AddPlot("Label", variable)
        a = visit.LabelAttributes()
        updateVisitObjectFromDictionary(a, attributes)
        visit.SetActivePlots(visit.GetNumPlots() - 1)
        visit.SetPlotOptions(a)
    return


def addTextAnnotation(x, y, message):
    """
    Add a 2D text annotation of height 2%
    :param x:
    :param y:
    :param message:
    :return:
    """
    try:
        textAnnotation = visit.GetAnnotationObject("Text2D1")
    except visit.VisItException:
        textAnnotation = visit.CreateAnnotationObject("Text2D")

    if textAnnotation:
        textAnnotation.text = message
        textAnnotation.position = (x, y)
        textAnnotation.height = 0.02
    return


def setViewFromDict(view_config):
    """
    Sets the view up with the given perspective object from the configuration.
    :param view_config: you can use all keys found in GetView2D() or GetView3D()
    :return:
    """
    if view_config:
        if 'windowCoords' in view_config or 'viewportCoords' in view_config:
            view = visit.GetView2D()
            updateVisitObjectFromDictionary(view, view_config)
            visit.SetView2D(view)
        else:
            view = visit.GetView3D()
            updateVisitObjectFromDictionary(view, view_config)
            visit.SetView3D(view)
    return


def setView(configuration, key_path):
    """
    Sets the view up with the given perspective object from the configuration.
    :param configuration: you can use all keys found in GetView2D() or GetView3D()
    :param key_path:
    :return:
    """
    viewConfig = utils.getValueForKeyPath(configuration, key_path)
    setViewFromDict(viewConfig)


def addPseudocolorPlots(db_file, configuration, key_path, time_index=-1):
    """
    Plots mapdata according to the configuration given. Note
    that $ variables will be replaced with the value found in
    the environment.
    :param db_file:
    :param configuration:
    :param key_path:
    :param time_index:
    :return:
    """
    plots = getValueForKeyPath(configuration, key_path)
    if plots:
        visit.OpenDatabase(db_file)
        for plot in plots:
            addPseudocolorPlot(db_file, plot, time_index)
    return


def setAnnotations(configuration, key_path):
    """
    Set annotation attributes from defaults and configuration.
    :param configuration: see visit.GetAnnotationAttributes()
    :param key_path:
    :return:
    """
    ca = utils.getValueForKeyPath(configuration, key_path)
    if ca:
        a = visit.GetAnnotationAttributes()
        updateVisitObjectFromDictionary(a, ca)
        visit.SetAnnotationAttributes(a)
    return


def createColorTables(configuration, key_path):
    """
    Creates a list of colortables from the configuration.
    :param configuration:
    :param key_path:
    :return:
    """
    colorTables = getValueForKeyPath(configuration, key_path)
    if colorTables:
        for colorTable in colorTables:
            colors = colorTable['colors']
            positions = colorTable['positions']
            ccpl = visit.ColorControlPointList()
            ccpl.categoryName = "meanie3D"
            for i in range(0, len(positions)):
                controlPoint = visit.ColorControlPoint()
                controlPoint.colors = tuple(colors[i])
                controlPoint.position = positions[i]
                ccpl.AddControlPoints(controlPoint)
            name = colorTable['name']
            visit.AddColorTable(name, ccpl)


def close_pattern(pattern):
    """
    Closes the databases of all files matching the given pattern.
    :param pattern:
    :return:
    """
    databases = glob.glob(pattern)
    for database in databases:
        visit.CloseDatabase(database)
    return


def plotMapdata(configuration, key_path):
    """
    Plots mapdata according to the configuration given. Note
    that $ variables will be replaced with the value found in
    the environment.
    :param configuration:
    :param key_path:
    :return:
    """
    mapConfig = getValueForKeyPath(configuration, key_path)
    if mapConfig:
        # Find the map data file
        mapFile = os.path.expandvars(mapConfig['mapDataFile'])

        # Check if data file exists
        if os.path.exists(mapFile):
            addPseudocolorPlots(mapFile, configuration, key_path + ".plots")
        else:
            print("ERROR:could not find map file at " + mapFile)
    return


# ---------------------------------------------------------
#
# File utilities
#
# ---------------------------------------------------------

def path(filename):
    """
    Extracts the path part from the given filename.
    :param filename:
    :return:
    """
    return os.path.dirname(filename)


def naked_name(filename):
    """
    Extracts the filename WITHOUT extension from the the given filename.
    :param filename:
    :return:
    """
    stripped = os.path.splitext(filename)[0]
    return stripped


# ---------------------------------------------------------
#
# Handling .png exports / images
#
# ---------------------------------------------------------

def saveImagesForViews(views, basename):
    """
    Iterates over the given view objects, sets each in turn and
    saves an image. If there is only one view, the resulting image
    filenames are 'basename_<number>.png'. If there are more than
    one view, the resulting image filenames look like 'p<viewNum>_basename_<number>.png'
    :param views:
    :param basename:
    :return:
    """
    if views:
        for i in range(0, len(views)):
            view = views[i]
            setViewFromDict(view)
            if len(views) > 1:
                filename = "p%d_%s" % (i, basename)
            else:
                filename = basename
            saveImage(filename, 1)
    else:
        saveImage(basename, 1)
    return


def saveImage(basename, progressive):
    """
    Saves the current window to an image file.
    :param basename:
    :param progressive:
    :return:
    """
    s = visit.GetSaveWindowAttributes()
    s.progressive = progressive
    s.fileName = basename + "_"
    s.outputToCurrentDirectory = 0
    s.outputDirectory = os.getcwd()
    visit.SetSaveWindowAttributes(s)
    visit.SaveWindow()
    return


def createMovieForViews(views, basename, extension):
    """
    Creates movies for the formats given from the images based on
    the basename given. Depending on the views, the filenames used
    start with 'p<viewNum>_basename_' or
    :param views:
    :param basename:
    :param extension:
    :return:
    """
    if views:
        for i in range(0, len(views)):
            if len(views) > 1:
                movie_fn = "p%d_%s" % (i, basename)
            else:
                movie_fn = basename
            image_fn = movie_fn + "_"
            create_movie(image_fn, movie_fn + os.path.extsep + extension)
    else:
        image_fn = basename + "_"
        create_movie(image_fn, basename + os.path.extsep + extension)


def createMoviesForViews(views, basename, formats):
    """
    :param views:
    :param basename:
    :param formats: ("gif","m4v" etc.)
    :return:
    """
    if formats:
        for k in range(0, len(formats)):
            createMovieForViews(views, basename, formats[k])


def delete_images(views, basename, image_count):
    """
    Checks if the image file(s) with the given number exists and deletes them.
    :param views:
    :param basename:
    :param image_count:
    :return:
    """
    number_postfix = str(image_count).rjust(4, '0') + ".png"
    if len(views) > 1:
        for i in range(0, len(views)):
            fn = "p" + str(i) + "_" + basename + "_" + number_postfix
            if os.path.exists(fn):
                os.remove(fn)
    else:
        fn = basename + "_" + number_postfix
        if os.path.exists(fn):
            os.remove(fn)


def images_exist(views, basename, image_count):
    """
    Checks if the image with the given number exists. Takes perspectives into account.
    :param views:
    :param basename:
    :param image_count:
    :return: "all","none" or "partial"
    """
    number_postfix = str(image_count).rjust(4, '0') + ".png"
    num_found = 0
    for i in range(0, len(views)):
        if len(views) > 1:
            fn = "p" + str(i) + "_" + basename + "_" + number_postfix
        else:
            fn = basename + "_" + number_postfix
        if os.path.exists(fn):
            num_found = num_found + 1
    if num_found == 0:
        return "none"
    elif num_found == len(views):
        return "all"
    else:
        return "partial"


def create_movie(basename, moviename):
    """
    Creates a movie from a .png image series.
    :param basename:
    :param moviename:
    :return:
    """
    print("Creating movie '" + moviename + "' from files '" + basename + "*.png ...")
    args = "-limit memory 4GB -delay 50 -quality 100 -dispose Background %s*.png %s" % (basename, moviename)
    print("convert %s" % args)
    external.run('convert', args, False)
    print("done.")
    return


# ---------------------------------------------------------
#
# Key-value coding, dictionary utils etc.
#
# ---------------------------------------------------------

def getVisitValue(obj, key):
    """
    Visit's objects are a bit of an arse. Standard getattr does
    not seem to work in a lot of times.
    :param obj:
    :param key:
    :return:
    """
    value = getattr(obj, key)
    if value is None:
        getter = getattr(obj, 'Get' + utils.capitalize(key))
        if getter:
            value = getter()
    return value


def setValueForKeyPath(obj, key_path, value):
    """
    Sets the given object values along a keypath separated by periods. Example: axes2D.yAxis.title.units.
    :param obj:
    :param key_path:
    :param value:
    :return:
    """
    keys = key_path.split(".")
    if len(keys) > 1:
        if type(obj) is dict:
            nextObject = obj[keys[0]]
        else:
            nextObject = getattr(obj, keys[0])
        keys.pop(0)
        remainingPath = ".".join(keys)
        setValueForKeyPath(nextObject, remainingPath, value)
    else:
        objectValue = getVisitValue(obj, key_path)

        # The specific enumerated values in Visit are configured
        # as strings in configuration dictionary, but are int values
        # in visualisation objects. Try to set an enumerated value when hitting
        # this combination
        if type(value) != type(objectValue):
            if type(value) == unicode and type(objectValue) == str:
                # unicode -> str autoconversion
                value = str(value)
            elif type(value) == int and type(objectValue) == float:
                # int -> float autoconversion
                value = float(value)
            elif type(value) == float and type(objectValue) == int:
                # float -> int autoconversion
                value = int(value)
            else:
                # Probably a visit enumeration constant. Resolve the value of the
                # enumerated value and use that instead
                value = getVisitValue(obj, str(value))

        if type(obj) is dict:
            obj[key_path] = value
        else:
            try:
                setattr(obj, key_path, value)
            except:
                try:
                    # try a setter
                    setter = getattr(obj, 'Set' + utils.capitalize(key_path))
                    if setter:
                        setter(value)
                except:
                    sys.stderr.write("Can't set value %s for key '%s' on object" % (str(value), key_path))
                    raise
    return


def getValueForKeyPath(obj, key_path):
    """
    Gets the given object values along a keypath separated
    by periods. Example: axes2D.yAxis.title.units.
    :param obj:
    :param key_path:
    :return:
    """
    return utils.getValueForKeyPath(obj, key_path)


def updateVisitObjectFromDictionary(obj, dictionary):
    """
    Sets the values from the dictionary on the given object.
    :param obj:
    :param dictionary:
    :return:
    """
    for key, value in dictionary.items():
        if type(value) is list:
            setValueForKeyPath(obj, key, tuple(value))
        else:
            setValueForKeyPath(obj, key, value)
    return


def add_datetime(conf, netcdf_file, time_index):
    """
    Reads the timestamp info from NetCDF file. Must comply to the
    cf-metadata convention. The timestamp position and format can be
    controlled through the (optional) configuration entry:
    :param conf:
    :param netcdf_file:
    :param time_index:
    :return:
    """
    ncfile = netCDF4.Dataset(netcdf_file, "r")
    times = ncfile.variables['time']
    ti = time_index
    if time_index < 0:
        ti = 0

    have_valid_time = False
    try:
        date = netCDF4.num2date(times[:], units=times.units, calendar=times.calendar)[ti]
        date = date.replace(microsecond=0)
        have_valid_time = True
    except ValueError as vi:
        print("Error reading time information from file:")
        print(vi.message)
        print("Falling back on time index")
        date = time_index

    x = 0.725
    y = 0.95
    _format = '__default__'
    if getValueForKeyPath(conf, 'timestamp'):
        if getValueForKeyPath(conf, 'timestamp.x'):
            x = getValueForKeyPath(conf, 'timestamp.x')
        if getValueForKeyPath(conf, 'timestamp.y'):
            y = getValueForKeyPath(conf, 'timestamp.y')
        if getValueForKeyPath(conf, 'timestamp.format'):
            _format = getValueForKeyPath(conf, 'timestamp.format')

    if have_valid_time:
        if _format == '__default__':
            addTextAnnotation(x, y, date.isoformat())
        else:
            addTextAnnotation(x, y, date.strftime(_format))
    else:
        addTextAnnotation(x, y, "time index: %d" % time_index)


# ---------------------------------------------------------
#
# Code to be phased out.
#
# ---------------------------------------------------------

def create_dual_panel(basename_left, basename_right, basename_combined):
    """
    Creates a series of images combined from two series of images as one.
    It removes the date stamp on the left side and cuts the right side such,
    that no legend is visible.
    :param basename_left:
    :param basename_right:
    :param basename_combined:
    :return:
    """
    left_files = sorted(glob.glob(basename_left + "*.png"))
    right_files = sorted(glob.glob(basename_right + "*.png"))
    if len(left_files) != len(right_files):
        print("ERROR:the two image series " + basename_left + "*.png and "
              + basename_right + "*.png have different lengths!")
        return

    # create a small image to blank the datestamp with
    call("/usr/local/bin/convert -size 280x50 xc:white dateblind.png", shell=True)

    # Iterate over the series
    for i in range(len(left_files)):
        # create backdrop
        combined = basename_combined + str(i) + ".png"
        external.run("convert", "-size 1852x1024 xc:white " + combined)

        # copy images and blank date
        external.run("composite", "-geometry +826+0 " + right_files[i] + " " + combined + " " + combined)
        external.run("composite", "-geometry +0+0 " + left_files[i] + " " + combined + " " + combined)
        external.run("composite", "-geometry +735+20 dateblind.png " + combined + " " + combined)

    # delete the dateblind
    call("rm dateblind.png", shell=True)
    return
