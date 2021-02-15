//
//  meanie3D-detect
//  cf-algorithms
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
#include <radolan/radolan.h>

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
#include <time.h>
#include <algorithm>

using namespace std;
using namespace boost;
using namespace netCDF;
using namespace m3D;

namespace fs = boost::filesystem;

typedef enum
{
    ShiftedPropertiesSatellite = 0,
    ShiftedPropertiesOthers = 1,
} ShiftedProperties;


#pragma mark -
#pragma mark Definitions

#define WRITE_PARALLAX_VECTORS 1

/** Feature-space data type
 */
typedef double T;

#pragma mark -
#pragma mark Command line parsing

void parse_commmandline(program_options::variables_map vm,
        string &filename,
        ShiftedProperties &shifted)
{
    if (vm.count("file") == 0)
    {
        cerr << "Missing 'file' argument" << endl;

        exit(1);
    }

    filename = vm["file"].as<string>();

    std::string shifted_name = vm["shifted"].as<string>();

    if (!(shifted_name == "satellite" || shifted_name == "other"))
    {
        cerr << "Illegal value for argument 'shifted'. Only 'satellite' or 'others' are accepted." << endl;
        exit(1);
    }

    if (shifted_name == "satellite")
    {
        shifted = ShiftedPropertiesSatellite;
    } else
    {
        shifted = ShiftedPropertiesOthers;
    }
};

#pragma mark -
#pragma mark Worker Methods

template <typename T>
T SQR(const T &x)
{
    return x * x;
}

/** This is a c++ translation of the method sent to me by Marianne Koenig (in F90):
 *
 * <cite>
 * Subroutine does a parallax correction for something seen at some
 * height in a position lat/lon by the satellite given by satheight,
 * satlat, satlon. The new coordinates are returned in latcorr and loncorr
 * </cite>
 *
 * @param satheight (REAL): height of the satellite in km
 * @param satlat (REAL): subsatellite latitude (deg, N is positive)
 * @param satlon (REAL): subsatellite longitude (deg, E is positive)
 * @param height (REAL): height of the cloud (km)
 * @param lat (REAL): latitude of the satellite pixel (N is positive)
 * @param lon (REAL): longitude of the satellite pixel (E is positive)
 */
template <typename T>
void parallax(T satheight, T satlat, T satlon, T height, T lat, T lon, T& latcorr, T& loncorr)
{
    T dpi;
    T radius_eq;
    T radius_pole;
    T radius_ratio;
    T mean_radius;
    T dheight;
    T alat, alon;
    T asatlat, asatlon;
    T satlat_geod, satlon_geod;
    T xsat, ysat, zsat;
    T xsurf, ysurf, zsurf;
    T alat_geod;
    T radius_surf;
    T radius_ratio_local;
    T xdiff, ydiff, zdiff;
    T xfact, zen;
    T e1, e2, e3;
    T corr;
    T xcorr, ycorr, zcorr;

    dpi = 3.14159265;

    // varius earth radius information

    radius_eq = 6378.077;
    dheight = satheight;
    radius_pole = 6356.577;
    radius_ratio = radius_eq / radius_pole;
    mean_radius = 0.5 * (radius_eq + radius_pole);
    zdiff = 0.0;

    //     angle conversion to radians

    asatlat = satlat * dpi / 180.0;
    asatlon = satlon * dpi / 180.0;
    alat = lat * dpi / 180.0;
    alon = lon * dpi / 180.0;

    //     cartesian coordinates for the satellite
    //     satlat_geod is the geodetic satellite latitude

    satlat_geod = atan(tan(asatlat) * SQR(radius_ratio));
    xsat = dheight * cos(satlat_geod) * sin(asatlon);
    ysat = dheight * sin(satlat_geod);
    zsat = dheight * cos(satlat_geod) * cos(asatlon);

    //     cartesian coordinates of the surface point

    alat_geod = atan(tan(alat) * SQR(radius_ratio));
    radius_surf = radius_eq / sqrt(SQR(cos(alat_geod)) + SQR(radius_ratio) * SQR(sin(alat_geod)));
    xsurf = radius_surf * cos(alat_geod) * sin(alon);
    ysurf = radius_surf * sin(alat_geod);
    zsurf = radius_surf * cos(alat_geod) * cos(alon);

    //     compute new radius ratio depending on height

    radius_ratio_local = SQR((radius_eq + height) / (radius_pole + height));

    //     Satellite minus surface location

    xdiff = xsat - xsurf;
    ydiff = ysat - ysurf;
    zdiff = zsat - zdiff;

    //     compute local zenith angle

    xfact = sqrt(SQR(xdiff) + SQR(ydiff) + SQR(zdiff));
    zen = (xdiff * xsurf + ydiff * ysurf + zdiff * zsurf) / (mean_radius * xfact);
    zen = acos(zen);
    zen = zen * 180.0 / dpi;

    //     equation to solve for the line of sight at height Z

    e1 = SQR(xdiff) + radius_ratio_local * SQR(ydiff) + SQR(zdiff);
    e2 = 2.0 * (xsurf * xdiff + radius_ratio_local * ysurf * ydiff + zsurf * zdiff);
    e3 = SQR(xsurf) + SQR(zsurf) + radius_ratio_local * SQR(ysurf) - SQR(radius_eq + height);

    corr = (sqrt(e2 * e2 - 4.0 * e1 * e3) - e2) / 2.0 / e1;

    //     corrected surface coordinates

    xcorr = xsurf + corr*xdiff;
    ycorr = ysurf + corr*ydiff;
    zcorr = zsurf + corr*zdiff;

    //     convert back to latitude and longitude

    latcorr = atan(ycorr / sqrt(SQR(xcorr) + SQR(zcorr)));
    latcorr = atan(tan(latcorr) / SQR(radius_ratio)) * 180.0 / dpi;

    loncorr = atan2(xcorr, zcorr) * 180.0 / dpi;
}

template <typename T>
T** allocate_array(size_t dim_y, size_t dim_x)
{
    T **array = new T*[dim_y];
    for (int i = 0; i < dim_y; ++i)
        array[i] = new T[dim_x];
    return array;
}

#define deallocate_array(array,dim) for (int i=0; i<dim; i++) delete[] array[i]; delete[] array;

/** Corrects the parallax on all seviri satellite variables
 * in the national 2D OASE composite
 * @param in_path path to the netcdf file to be corrected. The data
 * in the file is overwritten.
 */
void correct_parallax(boost::filesystem::path in_path, const ShiftedProperties shifted)
{
    // Some constants
    const double SAT_LON = 9.5; // longitude of METEOSAT-9
    const double SAT_LAT = 0.0; // latitute of METEOSAT-9
    const double SAT_HEIGHT = 35785.83; // height of METEOSAT-9 [km]

#define dim_x 900
#define dim_y 900

    try
    {
        NcFile file(in_path.generic_string(), NcFile::write);

        try
        {
            std::string type;
            file.getAtt("parallax_corrected").getValues(type);
            cout << "Parallax is already corrected (parallax_corrected=" << type << ")" << endl;
            cout << "Skipping file" << endl;
            return;
        } catch (const netCDF::exceptions::NcBadId &e)
        {
        }

        typedef std::multimap<std::string, NcVar> vmap_t;

        vmap_t variables = file.getVars();

        // Cloud-Top-Height is needed as input

        static float cloud_top_height[dim_y][dim_x];

        float cth_min = std::numeric_limits<float>::max();
        float cth_max = std::numeric_limits<float>::min();

        vmap_t::iterator fi = variables.find("msevi_l2_nwcsaf_cth");
        if (fi == variables.end())
        {
            cerr << "ERROR: could not find cloud top height (msevi_l2_nwcsaf_cth) variable" << endl;
            return;
        }

        fi->second.getVar(&cloud_top_height[0][0]);

        float cth_scale_factor = 1.0;
        float cth_offset = 0.0;
        float cth_valid_min, cth_valid_max;
        float cth_fill_value = std::numeric_limits<float>::min();

        fi->second.getAtt("scale_factor").getValues(&cth_scale_factor);
        fi->second.getAtt("add_offset").getValues(&cth_offset);
        fi->second.getAtt("_FillValue").getValues(&cth_fill_value);
        fi->second.getAtt("valid_min").getValues(&cth_valid_min);
        fi->second.getAtt("valid_max").getValues(&cth_valid_max);

        // create a variable for input and one for output

        static int corrected_iy[dim_y][dim_x];
        static int corrected_ix[dim_y][dim_x];

        // initialize output data with flag to find pixels
        // later that have not been set

        for (size_t iy = 0; iy < dim_y; iy++)
        {
            for (size_t ix = 0; ix < dim_x; ix++)
            {
                corrected_ix[iy][ix] = 0;
                corrected_iy[iy][ix] = 0;

                if (cloud_top_height[iy][ix] < cth_valid_min
                        || cloud_top_height[iy][ix] > cth_valid_max
                        || cloud_top_height[iy][ix] == cth_fill_value)
                {
                    cloud_top_height[iy][ix] = 0.0;
                } else
                {
                    cloud_top_height[iy][ix] = cth_scale_factor * cloud_top_height[iy][ix] + cth_offset;
                }

                if (cloud_top_height[iy][ix] > cth_max)
                    cth_max = cloud_top_height[iy][ix];

                if (cloud_top_height[iy][ix] < cth_min)
                    cth_min = cloud_top_height[iy][ix];
            }
        }

        // cout << endl << "cth_min=" << cth_min << " cth_max=" << cth_max << endl;

        // Coordinate system for lat/lon transformation

        RDCoordinateSystem rcs(RD_RX);

        typedef std::vector< std::vector<T> > vec_list_t;

#if WRITE_PARALLAX_VECTORS
        vec_list_t origins;
        vec_list_t correction_vectors;
#endif
        // correct the parallax

        for (size_t iy = 0; iy < dim_y; iy++)
        {
            for (size_t ix = 0; ix < dim_x; ix++)
            {
                RDGridPoint gp = rdGridPoint(ix, iy);

                // get lat/lon for this pixel
                RDGeographicalPoint coord = rcs.geographicalCoordinate(gp);

                // Get Marianne Koenig's correction values

                T cth = boost::numeric_cast<float> (cloud_top_height[iy][ix]) / 1000.0f;

                T lat_corrected = 0;
                T lon_corrected = 0;

                parallax<double> (SAT_HEIGHT, SAT_LAT, SAT_LON, cth, coord.latitude, coord.longitude, lat_corrected, lon_corrected);

                //                if (cth > 0)
                //                {
                //                    T lat_corr_0, lon_corr_0;
                //                    parallax<double> ( SAT_HEIGHT, SAT_LAT, SAT_LON, 0, coord.latitude, coord.longitude, lat_corr_0, lon_corr_0 );
                //                    cout << "(lat="<<coord.latitude << "N,lon="<<coord.longitude<<"E,cth="<<cth<<"km)"
                //                        << " (lat_corr="<<lat_corrected<<",lon_corr="<<lon_corrected<<")"
                //                        << " @cth=0.0:"
                //                        << " (lat_corr="<<lat_corr_0<<",lon_corr="<<lon_corr_0<<")"
                //                        << endl;
                //                }

                RDGeographicalPoint coord_corrected;

                // Figure out the grid point again and set
                // data at corrected position

                if (shifted == ShiftedPropertiesSatellite)
                {
                    // The correction shifts the satellite data to the
                    // corrected position. The parallax of the satellite
                    // is now corrected, but the other data stays in place

                    coord_corrected.latitude = lat_corrected;
                    coord_corrected.longitude = lon_corrected;
                } else
                {
                    // The correction shifts the other data to the
                    // corrected position. The parallax of the satellite
                    // is not corrected, but the other data is shifted
                    // to be congruent

                    // Experimental

                    T dLat = (lat_corrected - coord.latitude);
                    T dLon = (lon_corrected - coord.longitude);

                    coord_corrected.latitude = coord.latitude - dLat;
                    coord_corrected.longitude = coord.longitude - dLon;
                }

                bool is_inside = false;
                RDGridPoint gp_corrected = rcs.gridPoint(coord_corrected, is_inside);

                // Figure out the parallax vector
                RDCartesianPoint cartesian = rcs.cartesianCoordinate(gp);
                RDCartesianPoint cartesian_corr = rcs.cartesianCoordinate(gp_corrected);

                // TODO: what if two pixels are moved to the same place?
                // The way things are now is 'last write wins'

                if (is_inside)
                {
#if WRITE_PARALLAX_VECTORS
                    vector<T> origin(2);
                    origin[0] = cartesian.x;
                    origin[1] = cartesian.y;
                    origins.push_back(origin);

                    vector<T> correction(2);
                    correction[0] = cartesian_corr.x - cartesian.x;
                    correction[1] = cartesian_corr.y - cartesian.y;
                    correction_vectors.push_back(correction);
#endif
                    corrected_ix[iy][ix] = gp_corrected.ix;
                    corrected_iy[iy][ix] = gp_corrected.iy;
                }
            }
        }
#if WITH_VTK
#if WRITE_PARALLAX_VECTORS
        string vector_path = in_path.filename().stem().string() + "-parallax.vtk";
        VisitUtils<T>::write_vectors_vtk(vector_path, origins, correction_vectors, "parallax");
#endif
#endif

        static int input_data[dim_y][dim_x];
        static int output_data[dim_y][dim_x];

        for (vmap_t::iterator vi = variables.begin(); vi != variables.end(); vi++)
        {
            NcVar variable = vi->second;

            int fill_value = 0;
            try
            {
                // Get the official _FillValue value if the
                // variable has one
                NcVarAtt fillValue = variable.getAtt("_FillValue");
                fillValue.getValues(&fill_value);
            } catch (netCDF::exceptions::NcException e)
            {
                // if not, put the value just outside the valid range
                int valid_min = std::numeric_limits<int>::min();
                fill_value = valid_min - 1;
            }

            if ((shifted == ShiftedPropertiesSatellite && boost::starts_with(variable.getName(), "msevi_"))
                    || (shifted == ShiftedPropertiesOthers && !boost::starts_with(variable.getName(), "msevi_")))
            {
                // Initialize arrays

                cout << "Correcting " << variable.getName() << " ... ";

                for (size_t iy = 0; iy < dim_y; iy++)
                {
                    for (size_t ix = 0; ix < dim_x; ix++)
                    {
                        input_data[iy][ix] = 0;
                        output_data[iy][ix] = fill_value;
                    }
                }

                // Read the satellite variable

                variable.getVar(&input_data[0][0]);

                // apply the correction derived from cloud top height

                for (size_t iy = 0; iy < dim_y; iy++)
                {
                    for (size_t ix = 0; ix < dim_x; ix++)
                    {
                        // get corrected indicees

                        size_t iy_corr = corrected_iy[iy][ix];
                        size_t ix_corr = corrected_ix[iy][ix];

                        // copy data over

                        output_data[iy_corr][ix_corr] = input_data[iy][ix];
                    }
                }

                // TODO: post-processing of points that got no values

                for (size_t iy = 0; iy < dim_y; iy++)
                {
                    for (size_t ix = 0; ix < dim_x; ix++)
                    {
                        if (output_data[iy][ix] == fill_value)
                        {
                            if (variable.getName() == "msevi_l2_nwcsaf_ct" || variable.getName() == "msevi_l2_nwcsaf_cma")
                            {
                                // use the most prevalent value in 25 neighborhood
                                map<int, int> value_count;

                                int num_values = 0;
                                int interpolation_width = 2;
                                int min_neighbours = 8;

                                for (int iiy = iy - interpolation_width; iiy < iy + interpolation_width; iiy++)
                                {
                                    for (int iix = ix - interpolation_width; iix < ix + interpolation_width; iix++)
                                    {
                                        if (iix == ix && iiy == iy) continue;

                                        if (iiy >= 0 && iiy < dim_y && iix >= 0 && iix < dim_x)
                                        {
                                            if (output_data[iiy][iix] != fill_value)
                                            {
                                                int val = boost::numeric_cast<int>(output_data[iiy][iix]);

                                                map<int, int>::iterator mi = value_count.find(val);

                                                if (mi == value_count.end())
                                                {
                                                    value_count[val] = 1;
                                                } else
                                                {
                                                    mi->second = mi->second + 1;
                                                }

                                                num_values++;
                                            }
                                        }
                                    }
                                }

                                // only replace if you have at least 4 valid neighbours

                                if (num_values >= min_neighbours)
                                {
                                    int most_used = value_count.begin()->first;

                                    for (map<int, int>::iterator mi = value_count.begin(); mi != value_count.end(); ++mi)
                                    {
                                        if (mi->second > value_count[most_used])
                                        {
                                            most_used = mi->first;
                                        }
                                    }

                                    output_data[iy][ix] = most_used;
                                }
                            } else
                            {
                                // replace with geometric average of 25 neighborhood

                                T sum = 0.0;
                                int num_values = 0;

                                int interpolation_width = 2;
                                int min_neighbours = 8;

                                for (int iiy = iy - interpolation_width; iiy < iy + interpolation_width; iiy++)
                                {
                                    for (int iix = ix - interpolation_width; iix < ix + interpolation_width; iix++)
                                    {
                                        if (iix == ix && iiy == iy) continue;

                                        if (iiy >= 0 && iiy < dim_y && iix >= 0 && iix < dim_x)
                                        {
                                            if (output_data[iiy][iix] != fill_value)
                                            {
                                                sum += output_data[iiy][iix];
                                                num_values++;
                                            }
                                        }
                                    }
                                }

                                // only replace if you have at least 4 valid neighbours

                                if (num_values >= min_neighbours)
                                {
                                    output_data[iy][ix] = (sum / num_values);
                                }
                            }
                        }
                    }
                }

                // Write data back

                variable.putVar(&output_data[0][0]);

                cout << "done." << endl;
            }
        }

        nc_redef(file.getId());
        file.putAtt("parallax_corrected", (shifted == ShiftedPropertiesSatellite ? "satellite" : "others"));
        nc_enddef(file.getId());

    } catch (netCDF::exceptions::NcException &e)
    {
        cerr << "ERROR:exception " << e.what() << endl;
        return;
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

    program_options::options_description desc("Applies parallax correction to mseviri satellite data in OASE composite files.");
    desc.add_options()
            ("help", "Produces this help.")
            ("version", "print version information and exit")
            ("file,f", program_options::value<string>(), "A single file or a directory to be processed. Only files ending in .nc will be processed.")
            ("shifted,s", program_options::value<string>()->default_value("satellite"), "Which values are to be shifted? [satellite|other] (default:satellite)");

    program_options::variables_map vm;

    try
    {
        program_options::store(program_options::parse_command_line(argc, argv, desc), vm);
        program_options::notify(vm);
    } catch (std::exception &e)
    {
        cerr << "ERROR:parsing command line caused exception: " << e.what()
                << ":check meanie3D-trackplot --help for command line options" << endl;
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
    ShiftedProperties shifted = ShiftedPropertiesSatellite;

    try
    {
        parse_commmandline(vm, source_path, shifted);
    } catch (const std::exception &e)
    {
        cerr << "FATAL:" << e.what() << endl;
        exit(EXIT_FAILURE);
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

            if (fs::is_regular_file(f) && fs::extension(f) == ".nc")
            {
                //cout << "Adding " << f.generic_string() << endl;
                files.insert(f);
            } else
            {
                cout << "Skipping " << f.generic_string() << endl;
            }

            dir_iter++;
        }
    } else
    {
        fs::path f = fs::path(source_path);

        std::string extension = fs::extension(f);

        if (fs::is_regular_file(f) && extension == ".nc")
        {
            files.insert(f);
        }
    }

    fset_t::iterator it;

    //	boost::progress_display *progress = NULL;

    //	if ( files.size() > 1 ) {
    //		progress = new progress_display ( files.size() );
    //	}

    for (it = files.begin(); it != files.end(); ++it)
    {
        //		if ( progress != NULL ) {
        //			progress->operator++();
        //		}

        boost::filesystem::path path = *it;

        // Correct

        try
        {
            cout << "Correcting " << path << "...";
            correct_parallax(path, shifted);
            cout << "done." << endl;
        } catch (std::exception &e)
        {
            cerr << "ERORO:Exception processing " << path.filename().generic_string()
                    << ":" << e.what() << endl;
        }
    }

    //	if ( progress != NULL ) {
    //		delete progress;
    //	}

    return 0;
};
