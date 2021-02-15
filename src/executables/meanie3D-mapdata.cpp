//
//  meanie3D-detect
//  cf-algorithms
//
//  Created by Jürgen Lorenz Simon on 5/3/12.
//  Copyright (c) 2012 Jürgen Lorenz Simon. All rights reserved.
//

#if WITH_SHAPELIB

#include <meanie3D/meanie3D.h>

#include <boost/algorithm/string.hpp>
#include <boost/exception/info.hpp>
#include <boost/filesystem.hpp>
#include <boost/lexical_cast.hpp>
#include <boost/math/special_functions/fpclassify.hpp>
#include <boost/program_options.hpp>
#include <boost/tokenizer.hpp>

#include <fstream>
#include <exception>
#include <locale>
#include <limits>
#include <map>
#include <netcdf>
#include <set>
#include <stdlib.h>
#include <string>
#include <sstream>
#include <vector>
#include <shapefil.h>


/* ******************************************************** */
/* Namespace includes                                       */
/* ******************************************************** */

using namespace std;
using namespace boost;
using namespace netCDF;
using namespace m3D;
using namespace m3D::utils;

/* ******************************************************** */
/* Defintiions and constants                                */
/* ******************************************************** */

#define DEBUG 0
#define DEBUG_LINES 0
#define DEBUG_LOCAL_SHAPEFILE 0
#define DEBUG_NATIONAL_SHAPEFILE 0

// Feature-space data type
typedef double T;

// Collection for shapefile lines
typedef vector<CoordinateSystem<double>::GridPoint> gp_vec_t;

// Constants

// # of vertical grid vertices
static const size_t NZ = 60;

// Vertical coordinate bounds
float z_min = 0.0f;
float z_max = 14.75f;

// # of horizontal grid vertices (national)
static const size_t NATIONAL_NX = 900;
static const size_t NATIONAL_NY = 900;

// Horizontal bounds (national)
static const float national_x_min = -523.4622f;
static const float national_x_max = 367.5378f;
static const float national_y_min = -4658.645f;
static const float national_y_max = -3759.645f;

// # of horizontal grid vertices (local)
static const size_t LOCAL_NX = 510;
static const size_t LOCAL_NY = 472;

// Horizontal bounds (local)
static const float local_x_min = -366.462f;
static const float local_x_max = -111.962f;
static const float local_y_min = -4340.645f;
static const float local_y_max = -4105.145f;

// shift in grid points (local within national)
int shift_x = (int) ceil((local_x_min - national_x_min));
int shift_y = (int) ceil((local_y_min - national_y_min));

// Marker values for shapefile
static double const x_marks_the_spot = 1.0;
static const double z_fillValue = -9999.0f;

// make compiler happy with this definition
static const double NO_SCALE = numeric_limits<double>::min();

/* ******************************************************** */
/* Global variables                                         */
/* ******************************************************** */

// This is the main source of information
// Note: this data is in [m].
static float gTopography[NATIONAL_NY][NATIONAL_NX];

/* ******************************************************** */
/* Command line parsing                                     */

/* ******************************************************** */

void parse_commmandline(program_options::variables_map vm,
                        NcFile **filePtr,
                        string &source_filename,
                        string &root_directory,
                        vector<NcDim> &dimensions,
                        vector<NcVar> &dimension_variables) {
    if (vm.count("source") == 0) {
        cerr << "Missing 'source' argument" << endl;
        exit(1);
    }

    source_filename = vm["source"].as<string>();
    try {
        root_directory = vm["root"].as<string>();
    } catch (const boost::exception &e) {
        cerr << "Missing 'root' argument" << endl;

        exit(EXIT_FAILURE);;
    }

    // Open NetCDF file
    NcFile *file = NULL;
    try {
        file = new NcFile(source_filename, NcFile::read);
        *filePtr = file;
    } catch (const netCDF::exceptions::NcException &e) {
        cerr << "error opening file '" << source_filename << "' : " << e.what() << endl;
        exit(EXIT_FAILURE);;
    }

    // Extract dimensions
    if (vm.count("dimensions") == 0) {
        cerr << "Missing parameter --dimensions" << endl;
        exit(1);
    }

    // parse dimension list
    typedef boost::tokenizer<boost::char_separator<char> > tokenizer;
    boost::char_separator<char> sep(",");
    tokenizer dim_tokens(vm["dimensions"].as<string>(), sep);
    for (tokenizer::iterator tok_iter = dim_tokens.begin(); tok_iter != dim_tokens.end(); ++tok_iter) {
        const char *name = (*tok_iter).c_str();
        dimensions.push_back(file->getDim(name));
        dimension_variables.push_back(file->getVar(name));
    }
}

/* ******************************************************** */
/* Some helper methods                                      */
/* ******************************************************** */

template<typename T, std::size_t N, std::size_t M>
void
initialize_array(T(&data)[N][M], T initial_value) {
    for (size_t iy = 0; iy < N; iy++)
        for (size_t ix = 0; ix < M; ix++)
            data[iy][ix] = initial_value;
}

template<typename T, std::size_t K, std::size_t N, std::size_t M>
void
initialize_array(T(&data)[K][N][M], T initial_value) {
    for (size_t iz = 0; iz < K; iz++)
        for (size_t iy = 0; iy < N; iy++)
            for (size_t ix = 0; ix < M; ix++)
                data[iz][iy][ix] = initial_value;
}

template<typename T, std::size_t K, std::size_t N, std::size_t M>
void
fill_topography_data_at(T(&data)[K][N][M], int ix, int iy, float z_in_meters) {
    double z_km = z_in_meters / 1000.0f; // [m]->[km]

    int index_z = (int) ceil(z_km / 0.25f);

    if (index_z > NZ) {
        index_z = NZ;
    } else if (index_z < 0) {
        index_z = 0;
    }
    for (size_t iz = 0; iz < NZ; iz++) {
        data[iz][iy][ix] = (iz <= index_z) ? z_in_meters : z_fillValue;
    }
}

template<typename T, std::size_t K, std::size_t N, std::size_t M>
void
mark_topography_data_at(T(&data)[K][N][M], int ix, int iy, float z_in_meters) {
    double z_km = z_in_meters / 1000.0f; // [m]->[km]

    int index_z = (int) ceil(z_km / 0.25f);

    if (index_z > NZ) {
        index_z = NZ;
    } else if (index_z < 0) {
        index_z = 0;
    }

    data[index_z][iy][ix] = 1.0;
}


/* ******************************************************** */
/* Topography                                               */

/* ******************************************************** */

bool read_topography(NcFile &topography_file) {
    bool result = false;

    initialize_array(gTopography, 0.0f);

    if (topography_file.isNull()) {
        cerr << "ERROR: could not open topography file " << topography_file.getName() << endl;
    }
    else {
        NcVar topo = topography_file.getVar("topography_srtm");

        if (topo.isNull()) {
            cerr << "ERROR: topography file " << topography_file.getName() << " has no variable 'topography_srtm'" <<
            endl;
        } else {
            topo.getVar(&gTopography[0][0]);
            result = true;
        }
    }

    return result;
}

void add_local_dimensions(NcFile &topography_file, NcFile &mapfile) {
    nc_redef(mapfile.getId());

    // Local dimensions / variables

    NcDim dx = mapfile.addDim("local_x", LOCAL_NX);
    NcDim dy = mapfile.addDim("local_y", LOCAL_NY);
    NcDim dz = mapfile.addDim("local_z", NZ);

    float delta_x = round(100.0 * (local_x_max - local_x_min) / (float) LOCAL_NX) / 100.0f;
    float delta_y = round(100.0 * (local_y_max - local_y_min) / (float) LOCAL_NY) / 100.0f;
    float delta_z = round(100.0 * (z_max - z_min) / (float) NZ) / 100.0f;

    NcVar x = mapfile.addVar("local_x", "float", "local_x");
    x.putAtt("units", "km");
    x.putAtt("long_name", "projection_x_coordinate, local composite");
    x.putAtt("valid_min", ncFloat, local_x_min);
    x.putAtt("valid_max", ncFloat, local_x_max);

    // y

    NcVar y = mapfile.addVar("local_y", "float", "local_y");
    y.putAtt("units", "km");
    y.putAtt("long_name", "projection_y_coordinate, local composite");
    y.putAtt("valid_min", ncFloat, local_y_min);
    y.putAtt("valid_max", ncFloat, local_y_max);

    // z

    NcVar z = mapfile.addVar("local_z", "float", "local_z");
    z.putAtt("units", "km");
    z.putAtt("long_name", "height above sea level, local composite");
    z.putAtt("valid_min", ncFloat, z_min);
    z.putAtt("valid_max", ncFloat, z_max);

    nc_enddef(mapfile.getId());

    // dim-vars

    float *x_data = (float *) malloc(sizeof(float) * LOCAL_NX);
    for (size_t i = 0; i < LOCAL_NX; i++) {
        x_data[i] = local_x_min + i * delta_x;
    }

    float *y_data = (float *) malloc(sizeof(float) * LOCAL_NY);
    for (size_t i = 0; i < LOCAL_NY; i++) {
        y_data[i] = local_y_min + i * delta_y;
    }

    float *z_data = (float *) malloc(sizeof(float) * NZ);
    for (size_t i = 0; i < NZ; i++) {
        z_data[i] = z_min + i * delta_z;
    }

    try {
        x.putVar(&x_data[0]);
        y.putVar(&y_data[0]);
        z.putVar(&z_data[0]);
    } catch (std::exception &e) {
        cerr << "ERROR:exception " << e.what() << endl;
    }

    delete x_data;
    delete y_data;
    delete z_data;
}

void add_national_dimensions(NcFile &topography_file, NcFile &mapfile) {
    nc_redef(mapfile.getId());

    NcDim dx = mapfile.addDim("national_x", NATIONAL_NX);
    NcDim dy = mapfile.addDim("national_y", NATIONAL_NY);
    NcDim dz = mapfile.addDim("national_z", NZ);

    // x

    NcVar x = mapfile.addVar("national_x", "float", "national_x");
    x.putAtt("units", "km");
    x.putAtt("long_name", "projection_x_coordinate, national composite");
    x.putAtt("valid_min", ncFloat, national_x_min);
    x.putAtt("valid_max", ncFloat, national_x_max);

    // y

    NcVar y = mapfile.addVar("national_y", "float", "national_y");
    y.putAtt("units", "km");
    y.putAtt("long_name", "projection_y_coordinate, national composite");
    y.putAtt("valid_min", ncFloat, national_y_min);
    y.putAtt("valid_max", ncFloat, national_y_max);

    // z

    NcVar z = mapfile.addVar("national_z", "float", "national_z");
    z.putAtt("units", "km");
    z.putAtt("long_name", "height above sea level, national composite");
    z.putAtt("valid_min", ncFloat, z_min);
    z.putAtt("valid_max", ncFloat, z_max);

    nc_enddef(mapfile.getId());

    // read the x/y - data from topography file

    NcVar topo_y = topography_file.getVar("y");
    float *y_data = (float *) malloc(sizeof(float) * netcdf::num_vals(topo_y));
    topo_y.getVar(y_data);

    NcVar topo_x = topography_file.getVar("x");
    float *x_data = (float *) malloc(sizeof(float) * netcdf::num_vals(topo_x));
    topo_x.getVar(x_data);

    // Topo file has no z data, therefore z must be created
    //
    float delta_z = (z_max - z_min) / boost::numeric_cast<float>(NZ);
    float *z_data = (float *) malloc(sizeof(float) * NZ);
    for (size_t iz = 0; iz < NZ; iz++) {
        z_data[iz] = delta_z * boost::numeric_cast<float>(iz);
    }

    try {
        x.putVar(&x_data[0]);
        y.putVar(&y_data[0]);
        z.putVar(&z_data[0]);
    } catch (std::exception &e) {
        cerr << "ERROR:exception " << e.what() << endl;
    }

    free(y_data);
    free(x_data);
    free(z_data);
}

void add_dimensions(NcFile &topography_file, NcFile &mapfile) {
    add_local_dimensions(topography_file, mapfile);
    add_national_dimensions(topography_file, mapfile);
}

void add_local_topography(NcFile &mapfile) {
    nc_redef(mapfile.getId());

    // Create 2D topo variable

    vector<NcDim> dims_2D;
    dims_2D.push_back(mapfile.getDim("local_y"));
    dims_2D.push_back(mapfile.getDim("local_x"));

    NcVar topo_2D = mapfile.addVar("local_topo_2D", ncDouble, dims_2D);
    topo_2D.putAtt("_FillValue", ncDouble, z_fillValue);
    topo_2D.putAtt("units", "m");
    topo_2D.putAtt("long_name", "height above sea level");
    topo_2D.putAtt("valid_min", ncDouble, -163.0);
    topo_2D.putAtt("valid_max", ncDouble, 14750.0f);
    topo_2D.putAtt("scale_factor", ncDouble, 1.0);
    topo_2D.putAtt("add_offset", ncDouble, 0.0);

    // Create 3D topo variable

    vector<NcDim> dims_3D;
    dims_3D.push_back(mapfile.getDim("local_z"));
    dims_3D.push_back(mapfile.getDim("local_y"));
    dims_3D.push_back(mapfile.getDim("local_x"));

    NcVar topo_3D = mapfile.addVar("local_topo_3D", ncDouble, dims_3D);
    topo_3D.putAtt("_FillValue", ncDouble, z_fillValue);
    topo_3D.putAtt("units", "m");
    topo_3D.putAtt("long_name", "height above sea level");
    topo_3D.putAtt("valid_min", ncDouble, -163.0f);
    topo_3D.putAtt("valid_max", ncDouble, 14750.0f);
    topo_3D.putAtt("scale_factor", ncDouble, 1.0);
    topo_3D.putAtt("add_offset", ncDouble, 0.0);

    nc_enddef(mapfile.getId());

    // Write variables

    static double topo_data_2D[LOCAL_NY][LOCAL_NX];
    static double topo_data_3D[NZ][LOCAL_NY][LOCAL_NX];

    // figure out the shift between national and local

    for (size_t iy = 0; iy < LOCAL_NY; iy++) {
        for (size_t ix = 0; ix < LOCAL_NX; ix++) {
            // Read the z value from the topography
            // national composite is offset by dy=318 and dx=157
            // and has twice the resolution

            float z_m = 0.0;
            int count = 0;

            // average the 9 pixels from the national composite
            // around the point

            int ny = shift_y + iy / 2;
            int nx = shift_x + ix / 2;

            for (int national_y = (ny - 1); national_y < (ny + 1); national_y++) {
                for (int national_x = (nx - 1); national_x < (nx + 1); national_x++) {
                    if ((national_x >= shift_x && national_x < NATIONAL_NX) &&
                        (national_y >= shift_y && national_y < NATIONAL_NY)) {
                        z_m += gTopography[national_y][national_x];
                        count++;
                    }
                }
            }

            z_m = z_m / boost::numeric_cast<float>(count);

            // 2D data set
            topo_data_2D[iy][ix] = z_m;

            // 3D data set
            fill_topography_data_at(topo_data_3D, ix, iy, z_m);
        }
    }

    // Write data off

    topo_2D.putVar(&topo_data_2D[0][0]);
    topo_3D.putVar(&topo_data_3D[0][0][0]);

}

void add_national_topography(NcFile &mapfile) {
    nc_redef(mapfile.getId());

    // Create 2D topo variable

    vector<NcDim> dims_2D;
    dims_2D.push_back(mapfile.getDim("national_y"));
    dims_2D.push_back(mapfile.getDim("national_x"));

    vector<NcDim> dims_3D;
    dims_3D.push_back(mapfile.getDim("national_z"));
    dims_3D.push_back(mapfile.getDim("national_y"));
    dims_3D.push_back(mapfile.getDim("national_x"));


    NcVar topo_2D = mapfile.addVar("national_topo_2D", ncDouble, dims_2D);
    topo_2D.putAtt("_FillValue", ncDouble, z_fillValue);
    topo_2D.putAtt("units", "m");
    topo_2D.putAtt("long_name", "height above sea level");
    topo_2D.putAtt("valid_min", ncDouble, -163.0);
    topo_2D.putAtt("valid_max", ncDouble, 14750.0f);
    topo_2D.putAtt("scale_factor", ncDouble, 1.0);
    topo_2D.putAtt("add_offset", ncDouble, 0.0);

    // Create 3D topo variable

    NcVar topo_3D = mapfile.addVar("national_topo_3D", ncDouble, dims_3D);
    topo_3D.putAtt("_FillValue", ncDouble, z_fillValue);
    topo_3D.putAtt("units", "m");
    topo_3D.putAtt("long_name", "height above sea level");
    topo_3D.putAtt("valid_min", ncDouble, -163.0f);
    topo_3D.putAtt("valid_max", ncDouble, 14750.0f);
    topo_3D.putAtt("scale_factor", ncDouble, 1.0);
    topo_3D.putAtt("add_offset", ncDouble, 0.0);

    nc_enddef(mapfile.getId());


    // Write variables

    static double topo_data_2D[NATIONAL_NY][NATIONAL_NX];
    static double topo_data_3D[NZ][NATIONAL_NY][NATIONAL_NX];

    for (size_t iy = 0; iy < NATIONAL_NY; iy++) {
        for (size_t ix = 0; ix < NATIONAL_NX; ix++) {
            // Read the z value from the topography

            double z_m = gTopography[iy][ix];

            topo_data_2D[iy][ix] = z_m;

            double z_km = z_m / 1000.0f; // [m]->[km]

            int index_z = (int) ceil(z_km / 0.25f);

            if (index_z > NZ)
                index_z = NZ;
            else if (index_z < 0)
                index_z = 0;

            for (size_t iz = 0; iz < NZ; iz++) {
                topo_data_3D[iz][iy][ix] = (iz <= index_z) ? z_m : z_fillValue;
            }
        }
    }

    // Write data off

    topo_2D.putVar(&topo_data_2D[0][0]);
    topo_3D.putVar(&topo_data_3D[0][0][0]);
}

/* ******************************************************** */
/* Shapefile data                                           */

/* ******************************************************** */


std::string shape_type(int shapeTypeID) {
    std::string result = "UNKNOWN";

    switch (shapeTypeID) {
        case SHPT_ARC:
            result = "SHPT_ARC";
            break;
        case SHPT_ARCM:
            result = "SHPT_SHPT_ARCM";
            break;
        case SHPT_ARCZ:
            result = "SHPT_ARCZ";
            break;
        case SHPT_MULTIPATCH:
            result = "SHPT_MULTIPATCH";
            break;
        case SHPT_MULTIPOINT:
            result = "SHPT_MULTIPOINT";
            break;
        case SHPT_MULTIPOINTM:
            result = "SHPT_SHPT_MULTIPOINTM";
            break;
        case SHPT_MULTIPOINTZ:
            result = "SHPT_MULTIPOINTZ";
            break;
        case SHPT_POINT:
            result = "SHPT_POINT";
            break;
        case SHPT_POINTM:
            result = "SHPT_POINTM";
            break;
        case SHPT_POINTZ:
            result = "SHPT_POINTZ";
            break;
        case SHPT_POLYGON:
            result = "SHPT_POLYGON";
            break;
        case SHPT_POLYGONM:
            result = "SHPT_POLYGONM";
            break;
        case SHPT_POLYGONZ:
            result = "SHPT_POLYGONZ";
            break;
        case SHPT_NULL:
            result = "SHPT_NULL";
            break;
    }

    return result;
}

template<std::size_t N, std::size_t M>
void
draw_line_in_grid_2D(double (&data)[N][M], gp_vec_t &line_points, NcVar &var) {
    using namespace m3D::utils::vectors;

#if DEBUG_LINES
    cout << "\tdrawing line (" << line_points.size() << " vertices)" << endl;
#endif

    if (line_points.size() < 2) {
#if DEBUG_LINES
        cout << "\tLine consists of only one point. Skipping." << endl;
#endif
        return;
    }

    CoordinateSystem<double>::GridPoint lp;
    bool have_last_point = false;
    gp_vec_t::iterator li;

    size_t j = 0;

    for (li = line_points.begin(); li != line_points.end(); li++) {
        CoordinateSystem<double>::GridPoint p = *li;

        // cout << p << endl;

        if (have_last_point) {
#if DEBUG_LINES
            cout << "\t\tdrawing segment #" << j++ << " from (" << lp[1] << "," << lp[0] << ") "
                    << "to (" << p[1] << "," << p[0] << ")" << endl;
#endif
            // which direction is x going?

            int dx = 0;

            if (lp[1] < p[1])
                dx = 1;
            else if (lp[1] > p[1])
                dx = -1;

            // which direction is y going?

            int dy = 0;

            if (lp[0] < p[0])
                dy = 1;
            else if (lp[0] > p[0])
                dy = -1;

            // on the starting block

            size_t x = lp[1];
            size_t y = lp[0];

#if DEBUG_LINES
            cout << "\t\t\t(" << x << "," << y << ")" << endl;
#endif
            data[y][x] = x_marks_the_spot;

            // go

            while (x != p[1] && y != p[0]) {
                // propose walking x
                vector<int> x_next(2);
                x_next[1] = x + dx;
                x_next[0] = y;

                // propose walking y
                vector<int> y_next(2);
                y_next[1] = x;
                y_next[0] = y + dy;

                // which one gets closer to end?
                double dist_x = vector_norm(p - x_next);
                double dist_y = vector_norm(p - y_next);

                if (dist_x < dist_y) {
                    // walking x gets us closer
                    x = x + dx;
                } else if (dist_x > dist_y) {
                    // walking y gets us closer
                    y = y + dy;
                } else {
                    // same difference, so walk both
                    x = x + dx;
                    y = y + dy;
                }

                // X marks the spot
                data[y][x] = x_marks_the_spot;
#if DEBUG_LINES
                cout << "\t\t\t(" << x << "," << y << ")" << endl;
#endif
            }
        }

        lp = p;
        have_last_point = true;
    }
}

/** Add arcs from a file provided by the following website:
 * http://www.naturalearthdata.com
 *
 * This method expects the dimensions and dimension vars
 * to be in place
 */
void add_shapefile_data(NcFile &mapfile, const char *shapefile, const char *variable_name) {
    SHPHandle file = SHPOpen(shapefile, "rb");

    if (!file) {
        cerr << "ERROR:can't open " << shapefile << endl;
        return;
    }

    // Obtain NetCDF data and create river variable

    vector<NcDim> dimensions_2D;
    dimensions_2D.push_back(mapfile.getDim("national_y"));
    dimensions_2D.push_back(mapfile.getDim("national_x"));

    vector<NcDim> dimensions_3D;
    dimensions_3D.push_back(mapfile.getDim("national_z"));
    dimensions_3D.push_back(mapfile.getDim("national_y"));
    dimensions_3D.push_back(mapfile.getDim("national_x"));

    vector<NcDim> local_dimensions_2D;
    local_dimensions_2D.push_back(mapfile.getDim("local_y"));
    local_dimensions_2D.push_back(mapfile.getDim("local_x"));

    vector<NcDim> local_dimensions_3D;
    local_dimensions_3D.push_back(mapfile.getDim("local_z"));
    local_dimensions_3D.push_back(mapfile.getDim("local_y"));
    local_dimensions_3D.push_back(mapfile.getDim("local_x"));

    vector<NcVar> variables_2D;
    variables_2D.push_back(mapfile.getVar("national_y"));
    variables_2D.push_back(mapfile.getVar("national_x"));
    CoordinateSystem<double> *cs_2D = new CoordinateSystem<double>(dimensions_2D, variables_2D);

    // Need to go into definition mode because of a
    // bug in the c++ NetCDF interface

    nc_redef(mapfile.getId());

    // Create river variable

    std::string varname_2D = "national_" + std::string(variable_name) + "_2D";
    std::string varname_3D = "national_" + std::string(variable_name) + "_3D";

    NcVar shapevar_2D = mapfile.addVar(varname_2D, ncFloat, dimensions_2D);
    shapevar_2D.putAtt("_FillValue", ncFloat, z_fillValue);
    shapevar_2D.putAtt("units", "km");
    shapevar_2D.putAtt("long_name", varname_2D);
    shapevar_2D.putAtt("valid_min", ncFloat, 0.0);
    shapevar_2D.putAtt("valid_max", ncFloat, x_marks_the_spot);
    shapevar_2D.putAtt("scale_factor", ncFloat, 1.0);
    shapevar_2D.putAtt("add_offset", ncFloat, 0.0);

    NcVar shapevar_3D = mapfile.addVar(varname_3D, ncFloat, dimensions_3D);
    shapevar_3D.putAtt("_FillValue", ncFloat, z_fillValue);
    shapevar_3D.putAtt("units", "km");
    shapevar_3D.putAtt("long_name", varname_3D);
    shapevar_3D.putAtt("valid_min", ncFloat, 0.0);
    shapevar_3D.putAtt("valid_max", ncFloat, x_marks_the_spot);
    shapevar_3D.putAtt("scale_factor", ncFloat, 1.0);
    shapevar_3D.putAtt("add_offset", ncFloat, 0.0);

    std::string local_varname_2D = "local_" + std::string(variable_name) + "_2D";
    std::string local_varname_3D = "local_" + std::string(variable_name) + "_3D";

    NcVar local_shapevar_2D = mapfile.addVar(local_varname_2D, ncFloat, local_dimensions_2D);
    local_shapevar_2D.putAtt("_FillValue", ncFloat, z_fillValue);
    local_shapevar_2D.putAtt("units", "km");
    local_shapevar_2D.putAtt("long_name", local_varname_2D);
    local_shapevar_2D.putAtt("valid_min", ncFloat, 0.0);
    local_shapevar_2D.putAtt("valid_max", ncFloat, x_marks_the_spot);
    local_shapevar_2D.putAtt("scale_factor", ncFloat, 1.0);
    local_shapevar_2D.putAtt("add_offset", ncFloat, 0.0);

    NcVar local_shapevar_3D = mapfile.addVar(local_varname_3D, ncFloat, local_dimensions_3D);
    local_shapevar_3D.putAtt("_FillValue", ncFloat, z_fillValue);
    local_shapevar_3D.putAtt("units", "km");
    local_shapevar_3D.putAtt("long_name", local_varname_3D);
    local_shapevar_3D.putAtt("valid_min", ncFloat, 0.0);
    local_shapevar_3D.putAtt("valid_max", ncFloat, x_marks_the_spot);
    local_shapevar_3D.putAtt("scale_factor", ncFloat, 1.0);
    local_shapevar_3D.putAtt("add_offset", ncFloat, 0.0);

    nc_enddef(mapfile.getId());

    vector<NcVar> local_variables_2D;
    local_variables_2D.push_back(mapfile.getVar("local_y"));
    local_variables_2D.push_back(mapfile.getVar("local_x"));
    CoordinateSystem<double> *local_cs_2D = new CoordinateSystem<double>(local_dimensions_2D, local_variables_2D);

    // allocate data and initialize

    static double data_2D[NATIONAL_NY][NATIONAL_NX];
    initialize_array(data_2D, z_fillValue);

    static double local_data_2D[NATIONAL_NY][NATIONAL_NX];
    initialize_array(local_data_2D, z_fillValue);

    // Iterate over the content of the shapefile

    // Radolan coordinate system
    RDCoordinateSystem rcs(RD_RX);

    for (int i = 0; i < file->nRecords; i++) {
        SHPObject *obj = SHPReadObject(file, i);

#if DEBUG
        cout << "Object #" << i << endl;
        cout << "\tnumber of parts: " << obj->nParts << endl;
        cout << "\tpart indexes: ";
        cfa::utils::array::print_array(obj->panPartStart, obj->nParts);
        cout << endl;
        cout << "\tnumber of vertices: " << obj->nVertices << endl;
        cout << "\tshape type:" << shape_type(obj->nSHPType) << endl;
#endif

        CoordinateSystem<double>::Coordinate coord = cs_2D->newCoordinate();

        CoordinateSystem<double>::GridPoint gp = cs_2D->newGridPoint();
        CoordinateSystem<double>::GridPoint local_gp = local_cs_2D->newGridPoint();

        for (int pi = 0; pi < obj->nParts; pi++) {
            gp_vec_t line_points;
            gp_vec_t local_line_points;

            int start_vertex = obj->panPartStart[pi];
            int end_vertex = (pi < (obj->nParts - 1)) ? (obj->panPartStart[pi + 1] - 1) : (obj->nVertices - 1);

#if DEBUG
            cout << "\tpart #" << pi << "vertices [" << start_vertex << " ... " << end_vertex << "]" << endl;
#endif

            for (int vi = start_vertex; vi <= end_vertex; vi++) {
                // Assume at this point that the original
                // projection/coordinate system is WGS84

                RDGeographicalPoint geographical_coordinate = rdGeographicalPoint(obj->padfX[vi], obj->padfY[vi]);

                // project to cartesian system
                RDCartesianPoint cartesian = rcs.cartesianCoordinate(geographical_coordinate);

                coord[0] = cartesian.y;
                coord[1] = cartesian.x;

                // national coordinate system

                try {
                    // look the grid point up from the file's coordinate system
                    cs_2D->reverse_lookup(coord, gp);


#if DEBUG_NATIONAL_SHAPEFILE
                    cout << "\t\tvertice #" << vi << " "
                            << " (lon,lat): (" << obj->padfX[vi] << "," << obj->padfY[vi] << ")"
                            << " (x,y): (" << cartesian.x << "," << cartesian.y << ")"
                            << " (i,j): (" << gp[1] << "," << gp[0] << ")"
                            << endl;
#endif

                    // if it's within bounds, add it
                    // line_points.insert(gp);
                    line_points.push_back(gp);
                } catch (std::out_of_range &e) {
                    // not within bounds.

                    if (!line_points.empty()) {
                        // dropped out of the grid in the middle of a segment, eh?
                        // Draw what we got and clear the line
                        draw_line_in_grid_2D(data_2D, line_points, shapevar_2D);

                        line_points.clear();
#if DEBUG
                        shapevar_2D.putVar(&data_2D[0][0]);
#endif
                    }
                }

                // local coordinate system

                try {
                    // look the grid point up from the file's coordinate system

                    local_cs_2D->reverse_lookup(coord, local_gp);

#if DEBUG_LOCAL_SHAPEFILE
                    cout << "\t\tvertice #" << vi << " "
                            << " (lon,lat): (" << obj->padfX[vi] << "," << obj->padfY[vi] << ")"
                            << " (x,y): (" << cartesian.x << "," << cartesian.y << ")"
                            << " (i,j): (" << local_gp[1] << "," << local_gp[0] << ")"
                            << endl;
#endif

                    // if it's within bounds, add it
                    // line_points.insert(gp);
                    local_line_points.push_back(local_gp);
                } catch (std::out_of_range &e) {
                    // not within bounds.

                    if (!local_line_points.empty()) {
                        // dropped out of the grid in the middle of a segment, eh?
                        // Draw what we got and clear the line
                        draw_line_in_grid_2D(local_data_2D, local_line_points, local_shapevar_2D);

                        local_line_points.clear();
#if DEBUG
                        local_shapevar_2D.putVar(&local_data_2D[0][0]);
#endif
                    }
                }
            }

            if (!line_points.empty()) {
                draw_line_in_grid_2D(data_2D, line_points, shapevar_2D);
#if DEBUG
                shapevar_2D.putVar(&data_2D[0][0]);
#endif
            }

            // Draw the line ...

            if (!local_line_points.empty()) {
                draw_line_in_grid_2D(local_data_2D, local_line_points, local_shapevar_2D);
#if DEBUG
                local_shapevar_2D.putVar(&local_data_2D[0][0]);
#endif
            }
        }

        SHPDestroyObject(obj);
    }

    // The 3D version is basically the same as the 2D version, only
    // the value is only set at the evelation of the topography file

    static double data_3D[NZ][NATIONAL_NY][NATIONAL_NX];
    initialize_array(data_3D, z_fillValue);

    for (size_t iy = 0; iy < NATIONAL_NY; iy++) {
        for (size_t ix = 0; ix < NATIONAL_NY; ix++) {
            if (data_2D[iy][ix] != z_fillValue) {
                // 3D data set
                //fill_topography_data_at(data_3D, ix, iy, data_2D[iy][ix]);
                mark_topography_data_at(data_3D, ix, iy, data_2D[iy][ix]);
            }
        }
    }

    // local

    static double local_data_3D[NZ][LOCAL_NY][LOCAL_NX];
    initialize_array(local_data_3D, z_fillValue);

    for (size_t iy = 0; iy < LOCAL_NY; iy++) {
        for (size_t ix = 0; ix < LOCAL_NY; ix++) {
            if (local_data_2D[iy][ix] != z_fillValue) {
                // 3D data set
                //fill_topography_data_at(local_data_3D, ix, iy, local_data_2D[iy][ix]);
                mark_topography_data_at(local_data_3D, ix, iy, local_data_2D[iy][ix] + 500.0);
            }
        }
    }

    // Write data off

    shapevar_2D.putVar(&data_2D[0][0]);
    shapevar_3D.putVar(&data_3D[0][0][0]);

    local_shapevar_2D.putVar(&local_data_2D[0][0]);
    local_shapevar_3D.putVar(&local_data_3D[0][0][0]);
}

void do_it() {
    // home
    const char *topo_file = "/Users/simon/Projects/Meteo/Ertel/data/maps/mapstuff/oase-georef-1km-germany-2d-v01b.nc";
    const char *river_shapefile = "/Users/simon/Projects/Meteo/Ertel/data/maps/www.naturalearthdata.com/ne_10m_rivers_lake_centerlines/ne_10m_rivers_lake_centerlines.shp";
    const char *boundaries_shapefile = "/Users/simon/Projects/Meteo/Ertel/data/maps/www.naturalearthdata.com/ne_10m_admin_0_countries/ne_10m_admin_0_countries.shp";

    // institute
    //    const char *topo_file =             "/home/Projects/Ertel/data/maps/mapstuff/oase-georef-1km-germany-2d-v01b.nc";
    //    const char *river_shapefile =       "/home/Projects/Ertel/data/maps/www.naturalearthdata.com/ne_10m_rivers_lake_centerlines/ne_10m_rivers_lake_centerlines.shp";
    //    const char *boundaries_shapefile =  "/home/Projects/Ertel/data/maps/www.naturalearthdata.com/ne_10m_admin_0_countries/ne_10m_admin_0_countries.shp";

    cout << "Reading topography data ... ";
    NcFile topography_file(topo_file, NcFile::read);
    if (topography_file.isNull()) {
        cerr << "ERROR: could not read topography file " << topo_file << endl;
        return;
    }
    read_topography(topography_file);
    cout << "done." << endl;

    cout << "Creating mapfile ...";
    NcFile mapfile("/Users/simon/Projects/Meteo/Ertel/data/maps/mapstuff/oase-mapdata.nc", NcFile::replace,
                   NcFile::classic);
    mapfile.putAtt("conventions", "CF-1.6");
    mapfile.putAtt("authors", "Jürgen Simon, Malte Diederich");
    mapfile.putAtt("created", "Oct 5th 2013 18:25:00 CET");
    mapfile.putAtt("version", "1.0");
    cout << "done." << endl;

    cout << "Adding dimensions and dimension variables to mapfile ... ";
    add_dimensions(topography_file, mapfile);
    cout << "done." << endl;

    cout << "Adding local topography data ... ";
    add_local_topography(mapfile);
    cout << "done." << endl;

    cout << "Adding national topography data ... ";
    add_national_topography(mapfile);
    cout << "done." << endl;

    cout << "Adding national shapefile data ... ";
    cout << " (rivers";
    add_shapefile_data(mapfile, river_shapefile, "rivers");
    cout << ",boundaries";
    add_shapefile_data(mapfile, boundaries_shapefile, "boundaries");
    cout << ") done." << endl;
}

int main(int argc, char **argv) {
    try {
        do_it();
    } catch (std::exception &e) {
        cerr << "ERROR:" << e.what() << endl;
    }

    return 0;
};

#else

#include <iostream>
int main(int argc, char **argv) {
    std::cerr << "Component disabled due to missing shapelib" << std::endl;
    return 0;
};

#endif