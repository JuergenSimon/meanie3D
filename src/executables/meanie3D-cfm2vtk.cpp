//
//  cf-cfm2vtk
//  cf-algorithms
//
//  Created by Jürgen Lorenz Simon on 28/1/13.
//  Copyright (c) 2012 Jürgen Lorenz Simon. All rights reserved.
//

#include <meanie3D/meanie3D.h>

#include <boost/tokenizer.hpp>
#include <boost/program_options.hpp>
#include <boost/filesystem.hpp>
#include <boost/lexical_cast.hpp>
#include <boost/algorithm/string.hpp>
#include <boost/exception/info.hpp>

#include <vector>
#include <string>
#include <sstream>
#include <fstream>
#include <exception>
#include <locale>

#include <netcdf>

using namespace boost;
using namespace m3D;
using namespace m3D::utils;
using namespace m3D::utils::vectors;
using namespace netCDF;
using namespace std;

/** Feature-space data type */
#define FS_TYPE double

/** Verbosity */
typedef enum {
    FileTypeClusters,
    FileTypeComposite,
    FileTypeUnknown
} FileType;

void parse_commmandline(program_options::variables_map vm,
                        string &filename,
                        string &destination,
                        string &variable,
                        vector<size_t> &vtk_dimension_indexes,
                        FileType &type,
                        bool &extract_skin,
                        bool &write_as_xml,
                        bool &write_displacement_vectors,
                        bool &mindTheTime) {
    // Version

    if (vm.count("version") != 0) {
        cout << m3D::VERSION << endl;
        exit(EXIT_SUCCESS);
    }

    // filename

    if (vm.count("file") == 0) {
        cerr << "Missing 'file' argument" << endl;
        exit(EXIT_FAILURE);
    } else {
        filename = vm["file"].as<string>();
    }

    // destination

    if (vm.count("destination") != 0) {
        destination = vm["destination"].as<string>();
    }

    // variable

    if (vm.count("variable") == 0) {
        cerr << "Missing 'variable' argument" << endl;
        exit(1);
    } else {
        variable = vm["variable"].as<string>();
    }

    // figure out file type

    if (vm.count("type") == 0) {
        cerr << "Missing 'type' argument" << endl;
        exit(EXIT_FAILURE);
    }

    string type_str = vm["type"].as<string>();

    boost::algorithm::to_lower(type_str);

    if (type_str == string("cluster")) {
        type = FileTypeClusters;
    } else if (type_str == string("composite")) {
        type = FileTypeComposite;
    } else {
        type = FileTypeUnknown;
    }

    // Delauney filtering?
    extract_skin = vm.count("extract-skin") > 0;

    // xml?
    write_as_xml = vm.count("write-as-xml") > 0;

    // write-displacement-vectors?
    write_displacement_vectors = vm.count("write-displacement-vectors");

    // VTK dimension mapping

    if (vm.count("vtk-dimensions") > 0) {
        // parse dimension list
        typedef boost::tokenizer<boost::char_separator<char> > tokenizer;
        boost::char_separator<char> sep(",");
        string str_value = vm["vtk-dimensions"].as<string>();
        tokenizer dim_tokens(str_value, sep);
        try {
            NcFile *file = new NcFile(filename, NcFile::read);
            vector<NcDim> dimensions = file->getVar(variable).getDims();

            mindTheTime = false;
            for (size_t i = 0; i < dimensions.size() && !mindTheTime; i++) {
                mindTheTime = dimensions[i].getName() == "time";
            }

            for (tokenizer::iterator tok_iter = dim_tokens.begin(); tok_iter != dim_tokens.end(); ++tok_iter) {
                const char *name = (*tok_iter).c_str();
                NcDim dim = file->getDim(name);
                vector<NcDim>::const_iterator fi = find(dimensions.begin(), dimensions.end(), dim);
                if (fi == dimensions.end()) {
                    cerr << "--vtk-dimension parameter " << dim.getName() << " is not part of " << variable <<
                    "'s dimensions" << endl;
                    exit(EXIT_FAILURE);
                }
                size_t index = fi - dimensions.begin();
                vtk_dimension_indexes.push_back(mindTheTime ? index-1 : index);
            }

            if (vtk_dimension_indexes.size() != dimensions.size()) {
                bool numbersDontAddUp = true;
                if (mindTheTime) {
                    numbersDontAddUp = vtk_dimension_indexes.size() != (dimensions.size() - 1);
                }
                if (numbersDontAddUp) {
                    cerr << "The number of vtk-dimensions must be identical to the number of dimensions of " <<
                    variable << endl;
                    exit(EXIT_FAILURE);
                }
            }

            delete file;
        } catch (const netCDF::exceptions::NcException &e) {
            cerr << e.what() << endl;
            exit(EXIT_FAILURE);
        }
    } else {
        cerr << "Missing parameter --vtk-dimensions" << endl;
        exit(EXIT_FAILURE);;
    }
}

void convert_clusters(const string &filename,
                      const string &destination,
                      bool extract_skin,
                      bool write_as_xml,
                      bool write_displacement_vectors,
                      bool mind_the_time) {
    boost::filesystem::path path(filename);
    CoordinateSystem<FS_TYPE> *cs = NULL;
    ClusterList<FS_TYPE> *list = ClusterList<FS_TYPE>::read(filename, &cs);

    //VisitUtils<FS_TYPE>::write_clusters_vtr(list, cs, list->source_file, true, false, true);
    // VisitUtils<FS_TYPE>::write_clusters_vtu(list, cs, list->source_file, 5, true, extract_skin, write_as_xml);

    cout << "Writing clusters as vtk/vtu" << endl;
    VisitUtils<FS_TYPE>::write_clusters_vtu(list, cs, list->source_file, 5, true, extract_skin,
                                                          write_as_xml);

    if (write_displacement_vectors) {
        cout << "Writing displacement vectors" << endl;
        vector<vector<FS_TYPE>> origins;
        vector<vector<FS_TYPE> > displacements;
        for (size_t i = 0; i < list->size(); i++) {
            Cluster<FS_TYPE>::ptr c = list->clusters.at(i);
            if (!c->displacement.empty()) {
                origins.push_back(c->geometrical_center());
                displacements.push_back(c->displacement);
            }
        }
        string displacements_path = path.filename().stem().string() + "-displacements.vtk";
        VisitUtils<FS_TYPE>::write_vectors_vtk(displacements_path, origins, displacements,
                                                             "displacement");
    }

    string centers_path = path.filename().stem().string() + "-centers.vtk";
    cout << "Writing cluster centers" << endl;
    VisitUtils<FS_TYPE>::write_geometrical_cluster_centers_vtk(centers_path, list->clusters);

    delete list;
    delete cs;
    cout << "Done." << endl;
}

void convert_composite(const string &filename,
                       const string &variable_name,
                       const string &destination) {
    try {
        // construct the destination path
        std::string filename_noext;
        string filename_only = boost::filesystem::path(filename).filename().string();
        boost::filesystem::path destination_path = boost::filesystem::path(destination);
        destination_path /= filename_only;
        destination_path.replace_extension("vtk");
        string dest_path = destination_path.generic_string();

        NcFile *file = new NcFile(filename, NcFile::read);
        vector<NcDim> dims = file->getVar(variable_name).getDims();

        vector<std::string> dimensions;
        vector<std::string> dimension_variables;
        for (size_t i = 0; i < dims.size(); i++) {
            std::string dimName = dims[i].getName();
            dimensions.push_back(dimName);
            dimension_variables.push_back(dimName);
        }

        vector<std::string> variables;
        variables.push_back(variable_name);

        const map<int, double> lower_thresholds, upper_thresholds, fill_values;

        NetCDFDataStore<FS_TYPE> *dataStore = new NetCDFDataStore<FS_TYPE>(
                filename, variables, dimensions, dimension_variables);

        FeatureSpace<FS_TYPE> *fs = new FeatureSpace<FS_TYPE>(
                dataStore->coordinate_system(),
                dataStore,
                lower_thresholds,
                upper_thresholds,
                fill_values);

        vector<string> feature_variables(dimension_variables);
        feature_variables.push_back(variable_name);

        VisitUtils<FS_TYPE>::write_featurespace_variables_vtk(
                filename, fs, feature_variables, variables);

        delete fs;
        delete file;
        delete dataStore;

    } catch (const netCDF::exceptions::NcException &e) {
        cerr << e.what() << endl;
        exit(EXIT_FAILURE);
    }
}

/**
 *
 *
 */
int main(int argc, char **argv) {
    // Declare the supported options.

    program_options::options_description desc("Options");
    desc.add_options()
            ("help,h", "produce help message")
            ("version", "print version information and exit")
            ("file,f", program_options::value<string>(), "CF-Metadata compliant NetCDF-file or a Meanie3D-cluster file")
            ("variable,v", program_options::value<string>(), "Name of the variable to be used")
            ("destination,d", program_options::value<string>()->default_value("."),
             "Name of output directory for the converted files (default '.')")
            ("type,t", program_options::value<string>(), "'cluster' or 'composite'")
#if WITH_VTK
            ("extract-skin,s", "Use delaunay filter to extract skin file")
            ("write-as-xml,x", "Write files in xml instead of ascii")
            ("write-displacement-vectors",
             "Write out an extra file containing the displacement vectors (type cluster only).")
            ("vtk-dimensions", program_options::value<string>(),
             "VTK files are written in the order of dimensions given. This may lead to wrong results if the order of the dimensions is not x,y,z. Add the comma-separated list of dimensions here, in the order you would like them to be written as (x,y,z)")
#endif
            ;

    program_options::variables_map vm;
    try {
        program_options::store(program_options::parse_command_line(argc, argv, desc), vm);
        program_options::notify(vm);
    } catch (const netCDF::exceptions::NcException &e) {
        cerr << "NetCDF exception:" << e.what() << endl;
        exit(EXIT_FAILURE);
    } catch (std::exception &e) {
        cerr << "Error parsing command line: " << e.what() << endl;
        cerr << "Check meanie3D-cfm2vtk --help for command line options" << endl;
        exit(EXIT_FAILURE);
    }

    // Version
    if (vm.count("version") != 0) {
        cout << m3D::VERSION << endl;
        exit(EXIT_FAILURE);
    }

    if (vm.count("help") == 1 || argc < 2) {
        cout << desc << "\n";
        return 1;
    }

    // Evaluate user input
    string filename;
    string destination;
    FileType type;
    string variable;
    vector<size_t> vtk_dimension_indexes;
    bool extract_skin = false;
    bool write_as_xml = false;
    bool write_displacement_vectors = false;
    bool mind_the_time = false;

    // string problemStr = "1.03964e-312";
    // double problemChild = boost::lexical_cast<double>(problemStr);
    // cout << problemStr << " = " << problemChild << endl;

    try {
        parse_commmandline(vm,
                           filename,
                           destination,
                           variable,
                           vtk_dimension_indexes,
                           type,
                           extract_skin,
                           write_as_xml,
                           write_displacement_vectors,
                           mind_the_time);

        // Make the mapping known to the visualization routines
        VisitUtils<FS_TYPE>::VTK_DIMENSION_INDEXES = vtk_dimension_indexes;

        // Select the correct point factory
        PointFactory<FS_TYPE>::set_instance(new PointDefaultFactory<FS_TYPE>());

        switch (type) {
            case FileTypeClusters:
                convert_clusters(filename, destination, extract_skin, write_as_xml, write_displacement_vectors, mind_the_time);
                break;

            case FileTypeComposite:
                convert_composite(filename, variable, destination);
                break;

            default:
                cerr << "Unknown file type" << endl;
                break;
        }

    } catch (const netCDF::exceptions::NcException &e) {
        cerr << "NetCDF exception:" << e.what() << endl;
        exit(EXIT_FAILURE);;
    }
    // } catch (const std::exception &e) {
    //     cerr << "std::exception:" << e.what() << endl;
    //     cerr << e.what() << endl;
    //     exit(EXIT_FAILURE);;
    // }

    return 0;
};
