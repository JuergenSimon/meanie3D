//
//  meanie3D-minmax
//
//  Created by Jürgen Lorenz Simon on 5/3/12.
//  Copyright (c) 2012 Jürgen Lorenz Simon. All rights reserved.
//

#include <boost/program_options.hpp>
#include <boost/filesystem.hpp>
#include <boost/progress.hpp>
#include <boost/date_time.hpp>
#include <boost/date_time/local_time/local_time.hpp>
#include <sstream>

#include <meanie3D/meanie3D.h>

#include <map>
#include <vector>
#include <string>
#include <sstream>
#include <iostream>
#include <fstream>
#include <exception>
#include <locale>
#include <limits>
#include <stdlib.h>
#include <netcdf>
#include <map>
#include <time.h>

using namespace std;
using namespace boost;
using namespace netCDF;
using namespace m3D;
using namespace m3D::utils;

#pragma mark -
#pragma mark Definitions

/** Feature-space data type 
 */
typedef double T;
typedef unsigned char Byte;

/** Filename formats 
 */
typedef enum
{
    TimestampFormatRadolan,
    TimestampFormatOASE2D,
    TimestampFormatOASE3D,
    TimestampFormatHErZTB4,
    TimestampFormatAutomatic,

} TimestampFormat;


#pragma mark -
#pragma mark Command line parsing

void parse_commmandline(program_options::variables_map vm,
        string &filename,
        bool &force)
{
    if (vm.count("file") == 0)
    {
        cerr << "Missing 'file' argument" << endl;

        exit(1);
    }

    filename = vm["file"].as<string>();

    force = (vm.count("force") > 0);
}

#pragma mark -
#pragma mark Helper Methods

/** Allocates an array of type T with n objects.
 * 
 * @param n
 * @return 
 */
template <typename T>
T* allocate(size_t n)
{
    T *array = (T*) malloc(sizeof (T) * n);
    if (array == NULL)
    {
        cerr << "FATAL:out of memory" << endl;
        exit(EXIT_FAILURE);
    }
    return array;
}

template <typename T>
void get_limits(NcVar variable, T& min, T& max)
{
    min = std::numeric_limits<T>::max();
    max = std::numeric_limits<T>::min();
    T fill_value = 0.0;
    bool have_fill_value = false;
    try {
        NcVarAtt fillValue = variable.getAtt("_FillValue");
        if (!fillValue.isNull()) {
            fillValue.getValues(&fill_value);
            have_fill_value = true;
        }
    } catch (::netCDF::exceptions::NcException &e) {
      std::cerr << e.what() << std::endl;
    }
    T *values = NULL;
    size_t numElements = 0;
    if (variable.getDimCount() == 1) {
        // 1D
        numElements = variable.getDim(0).getSize();
        values = allocate<T>(numElements);
        variable.getVar(values);
    } else if (variable.getDimCount() == 2) {
        // 2D
        size_t N = variable.getDim(0).getSize();
        size_t M = variable.getDim(1).getSize();
        numElements = N * M;
        values = allocate<T>(numElements);
        variable.getVar(values);
    } else if (variable.getDimCount() == 3) {
        // 3D
        size_t N = variable.getDim(0).getSize();
        size_t M = variable.getDim(1).getSize();
        size_t K = variable.getDim(2).getSize();
        numElements = N * M * K;
        values = allocate<T>(numElements);
        vector<size_t> count(3, 1);
        count[0] = 1;
        count[1] = M;
        count[2] = K;
        vector<size_t> start(3, 0);
        start[0] = 0;
        start[1] = 0;
        start[2] = 0;
        variable.getVar(start, count, values);
    } else {
        cerr << "ERROR:Variables with " << variable.getDimCount()
                << " dimensions are not currently handled" << endl;
        return;
    }

    for (size_t j = 0; j < numElements; j++) {
        T value = values[j];
        if (have_fill_value && value == fill_value) continue;
        if (value < min)
        {
            min = value;
        }
        if (value > max)
        {
            max = value;
        }
    }

    delete values;
}

template <typename T>
void add_limits(NcFile &file, NcVar variable, bool have_min, bool have_max, bool force)
{
    T min, max;

    get_limits<T>(variable, min, max);

    nc_redef(file.getId());

    if (!have_min || force)
    {
        cout << "\t\tadding valid_min = " << min << endl;

        if (typeid (min) == typeid (Byte))
        {
            NcVarAtt att = variable.putAtt("valid_min", variable.getType(), (Byte) min);
        }
        if (typeid (min) == typeid (short))
        {
            NcVarAtt att = variable.putAtt("valid_min", variable.getType(), (short) min);
        } else if (typeid (min) == typeid (int))
        {
            NcVarAtt att = variable.putAtt("valid_min", variable.getType(), (int) min);
        } else if (typeid (min) == typeid (long))
        {
            NcVarAtt att = variable.putAtt("valid_min", variable.getType(), (long) min);
        } else if (typeid (min) == typeid (float))
        {
            NcVarAtt att = variable.putAtt("valid_min", variable.getType(), (float) min);
        } else if (typeid (min) == typeid (double))
        {
            NcVarAtt att = variable.putAtt("valid_min", variable.getType(), (double) min);
        } else if (typeid (min) == typeid (unsigned short))
        {
            NcVarAtt att = variable.putAtt("valid_min", variable.getType(), (unsigned short) min);
        } else if (typeid (min) == typeid (unsigned int))
        {
            NcVarAtt att = variable.putAtt("valid_min", variable.getType(), (unsigned int) min);
        } else if (typeid (min) == typeid (unsigned long long))
        {
            NcVarAtt att = variable.putAtt("valid_min", variable.getType(), (unsigned long long) min);
        } else if (typeid (min) == typeid (long long))
        {
            NcVarAtt att = variable.putAtt("valid_min", variable.getType(), (long long) min);
        } else
        {
            cout << "ERROR: type " << typeid (min).name() << " not handled " << endl;
        }
    }

    if (!have_max || force)
    {
        cout << "\t\tadding valid_max = " << max << endl;
        if (typeid (max) == typeid (Byte))
        {
            NcVarAtt att = variable.putAtt("valid_max", variable.getType(), (Byte) max);
        } else if (typeid (max) == typeid (short))
        {
            NcVarAtt att = variable.putAtt("valid_max", variable.getType(), (short) max);
        } else if (typeid (max) == typeid (int))
        {
            NcVarAtt att = variable.putAtt("valid_max", variable.getType(), (int) max);
        } else if (typeid (max) == typeid (long))
        {
            NcVarAtt att = variable.putAtt("valid_max", variable.getType(), (long) max);
        } else if (typeid (max) == typeid (float))
        {
            NcVarAtt att = variable.putAtt("valid_max", variable.getType(), (float) max);
        } else if (typeid (max) == typeid (double))
        {
            NcVarAtt att = variable.putAtt("valid_max", variable.getType(), (double) max);
        } else if (typeid (max) == typeid (unsigned short))
        {
            NcVarAtt att = variable.putAtt("valid_max", variable.getType(), (unsigned short) max);
        } else if (typeid (max) == typeid (unsigned int))
        {
            NcVarAtt att = variable.putAtt("valid_max", variable.getType(), (unsigned int) max);
        } else if (typeid (max) == typeid (unsigned long long))
        {
            NcVarAtt att = variable.putAtt("valid_max", variable.getType(), (unsigned long long) max);
        } else if (typeid (max) == typeid (long long))
        {
            NcVarAtt att = variable.putAtt("valid_max", variable.getType(), (long long) max);
        } else
        {
            cout << "ERROR: type " << typeid (max).name() << " not handled " << endl;
        }
    }

    nc_enddef(file.getId());
}

void define_min_max(NcFile &file, NcVar &variable, bool force)
{
    // check valid_min

    cout << "\tChecking variable " << variable.getName() << " (" << variable.getType().getName() << ")" << endl;

    bool have_valid_min = false;

    try
    {
        NcVarAtt valid_min = variable.getAtt("valid_min");

        have_valid_min = !valid_min.isNull();
    } catch (::netCDF::exceptions::NcException e)
    {
    }

    bool have_valid_max = false;

    try
    {
        NcVarAtt valid_max = variable.getAtt("valid_max");

        have_valid_max = !valid_max.isNull();
    } catch (::netCDF::exceptions::NcException e)
    {
    }

    if (!(have_valid_min || have_valid_max) || force)
    {
        cout << "\t\textracting limits ..." << endl;

        /*!
         The name of this type. For atomic types, the CDL type names are returned. These are as follows:
         - NcByte   String returned is "byte".
         - NcUbyte  String returned is "ubyte".
         - NcChar   String returned is "char".
         - NcShort  String returned is "short".
         - NcUshort String returned is "ushort".
         - NcInt    String returned is "int".
         - NcUint   String returned is "uint".
         - NcInt64  String returned is "int64".
         - NcUint64 String returned is "uint64".
         - NcFloat  String returned is "float".
         - NcDouble String returned is "double".
         - NcString String returned is "string".
         */

        if (strcmp(variable.getType().getName().c_str(), "byte") == 0)
        {
            add_limits<short>(file, variable, have_valid_min, have_valid_max, force);
        } else if (strcmp(variable.getType().getName().c_str(), "ubyte") == 0)
        {
            add_limits<unsigned short>(file, variable, have_valid_min, have_valid_max, force);
        } else if (strcmp(variable.getType().getName().c_str(), "short") == 0)
        {
            add_limits<short>(file, variable, have_valid_min, have_valid_max, force);
        } else if (strcmp(variable.getType().getName().c_str(), "ushort") == 0)
        {
            add_limits<unsigned short>(file, variable, have_valid_min, have_valid_max, force);
        } else if (strcmp(variable.getType().getName().c_str(), "int") == 0)
        {
            add_limits<int>(file, variable, have_valid_min, have_valid_max, force);
        } else if (strcmp(variable.getType().getName().c_str(), "uint") == 0)
        {
            add_limits<unsigned int>(file, variable, have_valid_min, have_valid_max, force);
        } else if (strcmp(variable.getType().getName().c_str(), "int64") == 0)
        {
            add_limits<long int>(file, variable, have_valid_min, have_valid_max, force);
        } else if (strcmp(variable.getType().getName().c_str(), "uint64") == 0)
        {
            add_limits<unsigned long int>(file, variable, have_valid_min, have_valid_max, force);
        } else if (strcmp(variable.getType().getName().c_str(), "float") == 0)
        {
            add_limits<float>(file, variable, have_valid_min, have_valid_max, force);
        } else if (strcmp(variable.getType().getName().c_str(), "double") == 0)
        {
            add_limits<double>(file, variable, have_valid_min, have_valid_max, force);
        } else
        {
            cout << "ERROR: data type " << variable.getType().getName() << " is not handled " << endl;
        }
    } else
    {
        cout << "\t\tvalid_min and valid_max exist" << endl;
    }
}

void adjust_file(std::string filename, bool force)
{
    try
    {
        NcFile file(filename, NcFile::write);

        if (!file.isNull())
        {
            NcDim time_dim;
            NcVar time_var;

            utils::netcdf::get_time_dim_and_var(file, time_dim, time_var);

            std::multimap< std::string, NcVar > vars = file.getVars();

            std::multimap< std::string, NcVar >::iterator vi;

            for (vi = vars.begin(); vi != vars.end(); vi++)
            {
                NcVar var = vi->second;

                if (var.getId() == time_var.getId()) continue;

                define_min_max(file, var, force);
            }

        }
    } catch (::netCDF::exceptions::NcException e)
    {
        cerr << "ERROR:exception " << e.what() << endl;
    }
}

#pragma mark -
#pragma mark MAIN

/* MAIN
 */
int main(int argc, char** argv)
{
    using namespace m3D;

    // Declare the supported options.

    program_options::options_description desc("Checks all non-time variables for valid_min and valid_max and adds them if necessary.");
    desc.add_options()
            ("help", "Produces this help.")
            ("version", "print version information and exit")
            ("file,f", program_options::value<string>(), "A single file or a directory to be processed. Only files ending in .nc will be processed.")
            ("force", program_options::value<bool>()->default_value(false), "Force replacement of attributes.");

    program_options::variables_map vm;

    try
    {
        program_options::store(program_options::parse_command_line(argc, argv, desc), vm);
        program_options::notify(vm);
    } catch (std::exception &e)
    {
        cerr << "FATAL:parsing command line caused exception: " << e.what()
                << ": check meanie3D-minmax --help for command line options"
                << endl;
        exit(EXIT_FAILURE);
    }

    // Version

    if (vm.count("version") != 0)
    {
        cout << m3D::VERSION << endl;
        exit(EXIT_SUCCESS);
    }

    if (vm.count("help") == 1 || argc < 2)
    {
        cout << desc << "\n";
        exit(EXIT_SUCCESS);
    }

    // Evaluate user input

    string source_path;
    bool force = false;

    namespace fs = boost::filesystem;

    try
    {
        parse_commmandline(vm, source_path, force);
    } catch (const std::exception &e)
    {
        cerr << "ERROR:exception " << e.what() << endl;
        exit(EXIT_FAILURE);
        ;
    }

    typedef set<fs::path> fset_t;

    fset_t files;

    if (fs::is_directory(source_path))
    {
        fs::directory_iterator dir_iter(source_path);
        fs::directory_iterator end;

        while (dir_iter != end)
        {
            fs::path f = dir_iter->path();
            std::string fn = f.filename().generic_string();

            if (fs::is_regular_file(f) && boost::algorithm::ends_with(fn, ".nc"))
            {
                //cout << "Adding " << f.generic_string() << endl;
                files.insert(f);
            } else
            {
                cout << "Skipping " << f.generic_string() << endl;
            }

            dir_iter++;
        }
    }
    else
    {
        fs::path f = fs::path(source_path);

        // there is a bug in boost 1.56 that causes this to crash
        // on linux. Replace it with a simpler method based on the
        // string alone

        //        std::string extension = fs::extension(f);
        //        if (fs::is_regular_file(f) && extension == ".nc") {
        //            files.insert(f);
        //        }

        if (fs::is_regular_file(f)
                && boost::algorithm::ends_with(source_path, ".nc"))
        {
            files.insert(f);
        }
    }

    fset_t::iterator it;

    for (it = files.begin(); it != files.end(); ++it)
    {
        fs::path path = *it;

        cout << path.generic_string() << endl;

        std::string fn = path.generic_string();

        // TODO: adjust valid_min/valid_max

        adjust_file(fn, force);

    }

    return 0;
};
