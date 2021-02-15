//
//  meanie3D-detect
//  cf-algorithms
//
//  Created by Jürgen Lorenz Simon on 5/3/12.
//  Copyright (c) 2012 Jürgen Lorenz Simon. All rights reserved.
//

#include <boost/tokenizer.hpp>
#include <boost/program_options.hpp>
#include <boost/filesystem.hpp>
#include <boost/lexical_cast.hpp>
#include <boost/algorithm/string.hpp>
#include <boost/exception/info.hpp>
#include <boost/smart_ptr.hpp>

#include <meanie3D/meanie3D.h>

#include <map>
#include <vector>
#include <string>
#include <sstream>
#include <fstream>
#include <exception>
#include <locale>
#include <limits>
#include <stdlib.h>
#include <netcdf>

using namespace std;
using namespace boost;
using namespace netCDF;
using namespace m3D;

/** Feature-space data type */
typedef double FS_TYPE;

static const double NO_SCALE = numeric_limits<double>::min();

void parse_commmandline(program_options::variables_map vm,
                        NcFile **filePtr,
                        string &source_filename,
                        string &root_directory,
                        vector<NcDim> &dimensions,
                        vector<NcVar> &dimension_variables)
{
    // Version

    if (vm.count("version") != 0)
    {
        cout << m3D::VERSION << endl;
        exit(EXIT_FAILURE);
        ;
    }

    if (vm.count("source") == 0)
    {
        cerr << "Missing 'source' argument" << endl;

        exit(1);
    }

    source_filename = vm["source"].as<string>();

    try
    {
        root_directory = vm["root"].as<string>();
    }
    catch (const boost::exception &e)
    {
        cerr << "Missing 'root' argument" << endl;

        exit(EXIT_FAILURE);
        ;
    }

    // Open NetCDF file

    NcFile *file = NULL;

    try
    {
        file = new NcFile(source_filename, NcFile::read);
    }
    catch (const netCDF::exceptions::NcException &e)
    {
        cerr << "ERROR opening file '" << source_filename << "' : " << e.what() << endl;
        exit(EXIT_FAILURE);
        ;
    }

    *filePtr = file;

    // Extract dimensions

    if (vm.count("dimensions") == 0)
    {
        cerr << "Missing parameter --dimensions" << endl;

        exit(1);
    }

    // parse dimension list

    typedef boost::tokenizer<boost::char_separator<char> > tokenizer;

    boost::char_separator<char> sep(",");

    tokenizer dim_tokens(vm["dimensions"].as<string>(), sep);

    for (tokenizer::iterator tok_iter = dim_tokens.begin(); tok_iter != dim_tokens.end(); ++tok_iter)
    {
        const char *name = (*tok_iter).c_str();

        dimensions.push_back(file->getDim(name));

        dimension_variables.push_back(file->getVar(name));
    }
}

int main(int argc, char **argv)
{
    using namespace m3D;

    // Declare the supported options.

    program_options::options_description desc("Options");
    desc.add_options()("help", "produce this help message")("version", "print version information and exit")("source", program_options::value<string>(), "File to use as a template")("root", program_options::value<string>()->default_value("."), "Root directory to recursively iterate over files and replace the dimensions and vars in all \"*-clusters.nc\" files")("dimensions", program_options::value<string>(), "Comma-separatred list of the dimensions to be used. The program expects dimension variables with identical names.");

    program_options::variables_map vm;

    try
    {
        program_options::store(program_options::parse_command_line(argc, argv, desc), vm);
        program_options::notify(vm);
    }
    catch (std::exception &e)
    {
        cerr << "Error parsing command line: " << e.what() << endl;
        cerr << "Check meanie3D-trackplot --help for command line options" << endl;
        exit(EXIT_FAILURE);
        ;
    }

    if (vm.count("help") == 1 || argc < 2)
    {
        cout << desc << "\n";
        return 1;
    }

    // Select the correct point factory
    PointFactory<FS_TYPE>::set_instance(new PointDefaultFactory<FS_TYPE>());

    // Evaluate user input

    NcFile *source = NULL;
    string source_filename;
    string root_directory;
    vector<NcDim> dimensions;
    vector<NcVar> dimension_variables;

    namespace fs = boost::filesystem;

    try
    {
        parse_commmandline(vm, &source, source_filename, root_directory, dimensions, dimension_variables);
    }
    catch (const std::exception &e)
    {
        cerr << e.what() << endl;
        exit(EXIT_FAILURE);
        ;
    }

    if (fs::is_directory(root_directory))
    {
        fs::directory_iterator dir_iter(root_directory);
        fs::directory_iterator end;

        // Iterate over the "-clusters.nc" - files in sourcepath

        while (dir_iter != end)
        {
            fs::path f = dir_iter->path();

            if (fs::is_regular_file(f) && boost::algorithm::ends_with(f.filename().generic_string(), "-clusters.nc") && f.generic_string() != source_filename)
            {
                try
                {
                    cout << "Processing file " << f.filename() << " ... ";

                    // copy the file to a backup

                    string backup = f.generic_string() + ".backup";

                    fs::path backup_path(backup);

                    // delete backup if it exists

                    if (fs::exists(backup_path))
                    {
                        fs::remove(backup_path);
                    }

                    // move the file to the backup path

                    fs::rename(f, backup_path);

                    NcFile original(backup_path.generic_string(), NcFile::read);

                    // Create file again

                    NcFile copy(f.generic_string(), NcFile::newFile);

                    // Copy attributes

                    multimap<string, NcGroupAtt> attributes = original.getAtts();

                    multimap<string, NcGroupAtt>::iterator at;

                    for (at = attributes.begin(); at != attributes.end(); at++)
                    {
                        NcGroupAtt a = at->second;

                        size_t size = a.getAttLength();

                        void *data = (void *)malloc(size);

                        a.getValues(data);

                        NcGroupAtt copyAtt = copy.putAtt(a.getName(), a.getType(), size, data);

                        free(data);
                    }

                    // Iterate over dimensions

                    multimap<string, NcDim> source_dims = original.getDims();

                    multimap<string, NcDim>::iterator sdi;

                    for (sdi = source_dims.begin(); sdi != source_dims.end(); sdi++)
                    {
                        NcDim source_dim = sdi->second;
                        NcDim copy_dim = copy.addDim(sdi->first, source_dim.getSize());
                    }

                    // Iterate over variables

                    multimap<string, NcVar> source_vars = original.getVars();

                    multimap<string, NcVar>::iterator vit;

                    for (vit = source_vars.begin(); vit != source_vars.end(); vit++)
                    {
                        NcVar original_var = vit->second;

                        // Make sure to copy the variables that were
                        // listed in the --dimensions parameter from
                        // the right file

                        for (size_t k = 0; k < dimensions.size(); k++)
                        {
                            NcDim sd = dimensions[k];

                            if (sd.getName() == original_var.getName())
                            {
                                original_var = dimension_variables[k];

                                break;
                            }
                        }

                        // Copy the variable

                        size_t size = utils::netcdf::num_vals(original_var);

                        // create a copy

                        NcVar copy_var = copy.addVar(original_var.getName(), original_var.getType(), original_var.getDims());

                        // read data

                        FS_TYPE *var_data = (FS_TYPE *)malloc(sizeof(FS_TYPE) * size);

                        original_var.getVar(var_data);

                        // write copy

                        copy_var.putVar(var_data);

                        free(var_data);

                        // copy attributes

                        map<string, NcVarAtt> attributes = original_var.getAtts();

                        map<string, NcVarAtt>::iterator at;

                        for (at = attributes.begin(); at != attributes.end(); at++)
                        {
                            NcVarAtt a = at->second;

                            size_t size = a.getAttLength();

                            void *attr_data = (void *)malloc(size);

                            a.getValues(attr_data);

                            NcVarAtt copy = copy_var.putAtt(a.getName(), a.getType(), size, attr_data);

                            free(attr_data);
                        }
                    }

                    cout << " done." << endl;
                }
                catch (const netCDF::exceptions::NcException &e)
                {
                    cerr << "ERROR opening file '" << f.generic_string() << "' : " << e.what() << endl;
                    exit(EXIT_FAILURE);
                    ;
                }
            }

            dir_iter++;
        }
    }

    return 0;
};
