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
import json
import os
import os.path
import shutil
import sys

from . import external


def load_configuration(filename):
    """
    Parses a JSON configuration file
    :param filename:
    :return:
    """
    json_data = open(filename)
    data = json.load(json_data)
    json_data.close()
    return data


def number_of_netcdf_files(source_dir):
    """
    Counts the number of netcdf files in the given directory
    :param source_dir:
    :return:
    """
    netcdf_pattern = source_dir + "/*.nc"
    netcdf_list = sorted(glob.glob(netcdf_pattern))
    return len(netcdf_list)


def numbered_filename(filename, index):
    """
    :param filename:
    :param index:
    :return:
    """
    basename = os.path.basename(filename)
    return os.path.splitext(basename)[0] + "-" + str(index) + ".nc"


def create_ouput_directories(base_path):
    """
    :param base_path:
    :return:
    """
    # base path
    if not os.path.exists(base_path):
        os.makedirs(base_path)

    # logs
    log_dir = base_path + "/log"
    if os.path.exists(log_dir):
        shutil.rmtree(log_dir)
    os.makedirs(log_dir)

    # netcdf results
    netcdf_dir = base_path + "/netcdf"
    if os.path.exists(netcdf_dir):
        shutil.rmtree(netcdf_dir)
    os.makedirs(netcdf_dir)

    return


def askYesNo(prompt):
    """
    Gets an answer to a yes/no kind of question
    :param prompt:
    :return:True if yes, False if no.
    """
    result = None
    msg = "%s [y|n]:" % prompt
    while result not in ('y', 'Y', 'n', 'N'):
        result = raw_input(msg)
    return result in ('y', 'Y')


def removeOutputDirectories(config):
    """
    :param config:
    :return:
    """
    dirsExist = False
    output_dir = config['output_dir']
    if config['scales']:
        for scale in config['scales']:
            _dir = output_dir + "/scale" + str(scale)
            dirsExist = (dirsExist or os.path.exists(_dir))
    else:
        _dir = output_dir + "/clustering"
        dirsExist = os.path.exists(_dir)

    if dirsExist:
        if askYesNo("Results exist from previous runs. They will be removed. Do you wish to proceed?"):
            if config['scales']:
                for scale in config['scales']:
                    _dir = output_dir + "/scale" + str(scale)
                    external.execute_command("rm", "-rf %s" % os.path.abspath(_dir))
            else:
                _dir = output_dir + "/clustering"
                external.execute_command("rm", "-rf %s" % os.path.abspath(_dir))
        else:
            return False
    return True


def getSafe(dictionary, key):
    """
    Tests if the dictionary has the given key. If so, it retrieves the value and returns it. If not, returns None.
    :param dictionary:
    :param key:
    :return:
    """
    if key in dictionary and dictionary[key]:
        return dictionary[key]
    else:
        return None


def capitalize(line):
    """
    Courtesy of
    http://stackoverflow.com/questions/1549641/how-to-capitalize-the-first-letter-of-each-word-in-a-string-python
    :param line:
    :return:
    """
    return ' '.join(s[0].upper() + s[1:] for s in line.split(' '))


def setValueForKeyPath(obj, keypath, value):
    """
    Sets the given object values along a keypath separated by periods. Example: axes2D.yAxis.title.units.
    :param obj:
    :param keypath:
    :param value:
    :return:
    """
    keys = keypath.split(".")
    if len(keys) > 1:
        if type(obj) is dict:
            nextObject = obj[keys[0]]
        else:
            nextObject = getattr(obj, keys[0])
        keys.pop(0)
        remainingPath = ".".join(keys)
        setValueForKeyPath(nextObject, remainingPath, value)
    else:
        # The specific enumerated values in Visit are configured
        # as strings in configuration dictionary, but are int values
        # in visualisation objects. Try to set an enumerated value when hitting
        # this combination
        if type(value) is str and type(getattr(obj, keypath)) is int:
            print("Attempting to resolve constant")
            value = getattr(obj, value)

        if type(obj) is dict:
            obj[keypath] = value
        else:
            try:
                setattr(obj, keypath, value)
            except:
                try:
                    # try a setter
                    setter = getattr(obj, 'Set' + capitalize(keypath))
                    if setter:
                        setter(value)
                except:
                    sys.stderr.write("Can't set value %s for key '%s' on object" % (str(value), keypath))
                    raise

    return


def getValueForKeyPath(obj, key_path):
    """
    Gets the given object values along a keypath separated by periods. Example: axes2D.yAxis.title.units
    :param obj:
    :param key_path:
    :return:
    """
    keys = key_path.split(".")
    if len(keys) > 1:
        if obj is None:
            return None
        elif type(obj) is dict:
            nextObject = getSafe(obj, keys[0])
        else:
            nextObject = getattr(obj, keys[0])
        keys.pop(0)
        remainingPath = ".".join(keys)
        return getValueForKeyPath(nextObject, remainingPath)
    else:
        # The specific enumerated values in Visit are configured
        # as strings in configuration dictionary, but are int values
        # in visualisation objects. Try to set an enumerated value when hitting
        # this combination
        if obj is None:
            return None
        elif type(obj) is dict:
            value = getSafe(obj, key_path)
        else:
            value = getattr(obj, key_path)
        return value


def setValuesFromDictionary(obj, dictionary):
    """
    Sets the values from the dictionary on the given object
    :param obj:
    :param dictionary:
    :return:
    """
    for key, value in dictionary.items():
        setValueForKeyPath(obj, key, value)
    return


def find(path, filename, required_component=None):
    """
    Recursive directory search
    :param path:
    :param filename:
    :param required_component: must have one component in the path to the result that matches this
    :return: fully qualified path to result or None
    """
    for root, dirs, files in os.walk(path, topdown=True):
        all_files = []
        for foo in files:
            all_files.append(foo)
        for bar in dirs:
            all_files.append(bar)
        for _file in all_files:
            if _file == filename:
                complete_path = os.path.abspath(root + os.path.sep + _file)
                if required_component:
                    components = complete_path.split(os.path.sep)
                    if required_component not in components:
                        continue
                    return complete_path
                else:
                    return complete_path
    return None


def find_in_paths(paths, filename, required_component=None):
    """
    Recursive directory search
    :rtype : string
    :param paths:
    :param filename:
    :param required_component:
    :return:
    """
    result = None
    for path in paths:
        result = find(path, filename, required_component)
        if result is not None:
            break
    return result


def findVisitPaths():
    """
    Finds the paths to the visit executable and python package
    :return: visit python package path or None, visit binary path or None
    """
    visitPath = None
    visitImportPath = None
    try:
        cmdMap = external.locateCommands(["visit"], True)
        if cmdMap['visit']:
            visitPath = os.path.abspath(os.path.join(cmdMap['visit'], os.pardir + os.sep + os.pardir))
            if visitPath:
                visitImportPath = find(visitPath, "site-packages")
    except:
        return None, None

    return visitPath, visitImportPath


def strToBool(s):
    """
    Converts a string into a bool using the following sets:
      True: ['true', '1', 't', 'y', 'yes']
      False: ['false', '0', 'f', 'n', 'no']
    Throws ValueError s is neither None nor in either true or false value set.
    :param s:
    :return: True if s is in true value set, False if s is None or in false value set.
    """
    if s:
        test = s.lower()
        if test in ['true', '1', 't', 'y', 'yes']:
            return True
        elif test in ['false', '0', 'f', 'n', 'no']:
            return False
        else:
            raise ValueError
    else:
        return False
