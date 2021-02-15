#include <meanie3D/adaptors.h>
#include <meanie3D/exceptions.h>

#include <netcdf>
#include <iostream>
#include <radolan/radolan.h>

#define ADD_DIMENSION_Z 0

namespace m3D {

    using namespace Radolan;
    using namespace netCDF;

    /** Converts the radolan file at path into a CF-Metadata compliant NetCDF-File.
     * @param radolanPath full path to the radolan file
     * @param netcdfPath full path to the netcdf file to be created
     * @param mode NcFile::Mode for opening the netcdf file with
     * @param omitOutside @see RDReadScan
     * @return NCFile* NetCDF-Filehandler
     * @throw CFFileConversionException
     */
    netCDF::NcFile * CFConvertRadolanFile(const char* radolanPath,
            const char* netcdfPath,
            bool write_one_bytes_as_byte,
            const RDDataType *threshold,
            netCDF::NcFile::FileMode mode,
            bool omitOutside)
    throw (m3D::CFFileConversionException)
    {
        if (mode == netCDF::NcFile::read)
        {
            throw CFFileConversionException("Mode 'ReadOnly' does not make sense");
        }

        RDScan *scan = RDAllocateScan();

        int res = RDReadScan(radolanPath, scan, omitOutside);

        switch (res)
        {
            case -1:
                throw CFFileConversionException("Insufficient memory for reading radolan scan");
                break;

            case -2:
                throw CFFileConversionException("Radolan file not found");
                break;

            case -3:
                throw CFFileConversionException("I/O eror when reading radolan file");
                break;

            default:
                break;
        }

        // Radolan::RDPrintScan( scan, 10, 10 );

        netCDF::NcFile *file = CFConvertRadolanScan(scan, netcdfPath, write_one_bytes_as_byte, threshold, mode);

        // CFPrintConvertedRadolanScan( file, 10, 10 );

        RDFreeScan(scan);

        return file;
    }

    /** Converts the radolan file at path into a CF-Metadata compliant NetCDF-File. 
     * @param radolanPath full path to the radolan file
     * @param netcdfPath full path to the netcdf file to be created
     * @param mode NcFile::Mode for opening the netcdf file with
     * @return NCFile* NetCDF-Filehandler
     * @throw CFFileConversionException
     */
    NcFile *CFConvertRadolanScan(RDScan *scan,
                                 const char *netcdfPath,
                                 bool write_one_bytes_as_byte,
                                 const RDDataType *threshold,
                                 NcFile::FileMode mode)

        throw(m3D::CFFileConversionException)
    {
        using namespace netCDF;
        using namespace std;

        NcFile* file = NULL;

        try
        {
            file = new netCDF::NcFile(netcdfPath, mode);
        }        catch (const netCDF::exceptions::NcException &e)
        {
            cerr << "ERROR:exception while creating file " << netcdfPath << " : " << e.what() << endl;
            throw CFFileConversionException(e.what());
        }

        // Global attributes

        file->putAtt("Conventions", "CF 1.6");
        file->putAtt("title", "Radolan composite in NetCDF/CF-Metadata form.");
        file->putAtt("institution", "HErZ-TB1 Workgroup");
        file->putAtt("version", "1.0");

        // Dimensions

        vector<NcDim> dims;
        NcDim dimT = file->addDim("time", 1);
        NcDim dimX = file->addDim("x", scan->dimLon);
        NcDim dimY = file->addDim("y", scan->dimLat);

#if ADD_DIMENSION_Z
        NcDim dimZ = file->addDim("z", 1);
#endif

        //dims.push_back(dimT);
#if ADD_DIMENSION_Z
        dims.push_back(dimZ);
#endif
        dims.push_back(dimY);
        dims.push_back(dimX);

        RDCoordinateSystem rcs = RDCoordinateSystem(scan->header.scanType);

        // Coordinates

        netCDF::NcVar x = file->addVar("x", ncDouble, dimX);
        x.putAtt("standard_name", "projection_x_coordinate");
        x.putAtt("units", "km");

        NcVar y = file->addVar("y", ncDouble, dimY);
        y.putAtt("standard_name", "projection_y_coordinate");
        y.putAtt("units", "km");

#if ADD_DIMENSION_Z
        NcVar z = file->addVar("z", ncDouble, dimZ);
        z.putAtt("standard_name", "projection_z_coordinate");
        z.putAtt("units", "km");
#endif

        // Grid Mapping

        RDGridPoint origin = rdGridPoint(0, 0);
        RDGeographicalPoint origin_geo = rcs.geographicalCoordinate(origin);

        NcVar crs = file->addVar("crs", NcType::nc_BYTE, dims); // note: type is of no consequence

        crs.putAtt("grid_mapping_name", "polar_stereographic");

        crs.putAtt("longitude_of_projection_origin", NcType::nc_DOUBLE, origin_geo.longitude);
        crs.putAtt("latitude_of_projection_origin", NcType::nc_DOUBLE, origin_geo.latitude);
        crs.putAtt("false_easting", NcType::nc_DOUBLE, 0.0f);
        crs.putAtt("false_northing", NcType::nc_DOUBLE, 0.0f);
        crs.putAtt("scale_factor_at_projection_origin", NcType::nc_DOUBLE, rcs.polarStereographicScalingFactor(origin_geo.longitude, origin_geo.latitude));
        crs.putAtt("units", "km");

        // Data

        bool is_one_byte = scan->header.scanType == RD_EX || scan->header.scanType == RD_RX;

        NcVar data;

        if (is_one_byte && write_one_bytes_as_byte)
        {
            data = file->addVar(RDScanTypeToString(scan->header.scanType), ncUbyte, dims);

            RDByteType valid_min = RDRVP6ToByteValue(RDMinValue(scan->header.scanType));
            data.putAtt("valid_min", ncInt, valid_min);

            RDByteType valid_max = RDRVP6ToByteValue(RDMaxValue(scan->header.scanType));
            data.putAtt("valid_max", ncInt, valid_max);

            RDByteType fill_value = RDRVP6ToByteValue(RDMissingValue(scan->header.scanType));
            data.putAtt("_FillValue", ncUbyte, fill_value);

            // RVP6 conversion via offset and scale_factor
            data.putAtt("add_offset", ncFloat, -32.5f);
            data.putAtt("scale_factor", ncFloat, 0.5f);
        }
        else
        {
            data = file->addVar(RDScanTypeToString(scan->header.scanType), ncFloat, dims);
            data.putAtt("valid_min", ncFloat, RDMinValue(scan->header.scanType));
            data.putAtt("valid_max", ncFloat, RDMaxValue(scan->header.scanType));
            data.putAtt("_FillValue", ncFloat, RDMissingValue(scan->header.scanType));
        }

        // Enable compression: no shuffle filter, compression rate 1
        // (see http://www.unidata.ucar.edu/software/netcdf/papers/AMS_2008.pdf)
        data.setCompression(false, true, 1);

        data.putAtt("grid_mapping", "polar_stereographic");
        data.putAtt("radolan_product", RDScanTypeToString(scan->header.scanType));
        data.putAtt("standard_name", CFRadolanDataStandardName(scan->header.scanType));

        // TIME

        double timestamp = (double) RDScanTimeInSecondsSinceEpoch(scan);

        NcVar time = file->addVar("time", ncDouble, dimT);
        time.putVar(&timestamp);
        time.putAtt("units", "seconds since 1970-01-01 00:00:00.0");
        time.putAtt("calendar", "gregorian");
        time.putAtt("standard_name", "time");

        // start point and counters for writing
        // the buffer to netcdf

#if ADD_DIMENSION_Z
        // z,y,x
        vector<size_t> startp(3, 0);
        vector<size_t> countp(3, 0);
        countp[0] = 1;
        countp[1] = scan->dimLat;
        countp[2] = scan->dimLon;
#else
        // y,x
        vector<size_t> startp(2, 0);
        vector<size_t> countp(2, 0);
        countp[0] = scan->dimLat;
        countp[1] = scan->dimLon;
#endif

        // Re-package data

        // x and y are switched around in the data (following
        // the cf-metadata convention)

        if (is_one_byte && write_one_bytes_as_byte)
        {
            size_t memSize = scan->dimLon * scan->dimLat * sizeof (RDByteType);
            RDByteType *buffer = (RDByteType *) malloc(memSize);

            for (int iy = 0; iy < scan->dimLon; iy++)
            {
                for (int ix = 0; ix < scan->dimLat; ix++)
                {
                    size_t index = iy * scan->dimLat + ix;
                    RDDataType val = scan->data[index];
                    RDByteType byteValue = RDRVP6ToByteValue(val);

                    if (val == RDMissingValue(scan->header.scanType))
                    {
                        buffer[index] = RX_ERROR_VALUE;
                    } else
                    {
                        // if a threshold is enabled, check the
                        // threshold first. This is checked on
                        // the converted value, not the byte value

                        bool should_write = (threshold == NULL)
                                ? true
                                : (val >= (*threshold));

                        buffer[index] = should_write
                                ? byteValue : 0x00;
                    }
                }
            }

            // write out

            try
            {
                data.putVar(startp, countp, buffer);
            }            catch (const std::exception &e)
            {
                throw CFFileConversionException(e.what());
            }

            free(buffer);
        } else
        {
            size_t memSize = scan->dimLon * scan->dimLat * sizeof (RDDataType);
            RDDataType *converted = (RDDataType *) malloc(memSize);

            // vector<size_t> index( dims.size() );

            for (int iy = 0; iy < scan->dimLon; iy++)
            {
                for (int ix = 0; ix < scan->dimLat; ix++)
                {
                    size_t index = iy * scan->dimLat + ix;
                    RDDataType val = scan->data[index];

                    if (val == RDMissingValue(scan->header.scanType))
                    {
                        // If the value is marked missing, use
                        // it as it is
                        converted[index] = RDMissingValue(scan->header.scanType);
                    } else
                    {
                        // if a threshold is enabled, check the
                        // threshold first

                        bool should_write = (threshold == NULL)
                                ? true
                                : (val >= (*threshold));

                        converted[index] = should_write
                                ? val
                                : RDMinValue(scan->header.scanType);
                    }
                }
            }

            // write out

            try
            {
                data.putVar(startp, countp, converted);
            }            catch (const std::exception &e)
            {
                throw CFFileConversionException(e.what());
            }

            free(converted);
        }

        // write x-axis information

        float *xData = (float *) malloc(sizeof (float) * scan->dimLon);

        for (int i = 0; i < scan->dimLon; i++)
        {
            RDCartesianPoint cp = rcs.cartesianCoordinate(rdGridPoint(i, 0));
            xData[i] = cp.x;
        }

        x.putVar(xData);
        x.putAtt("valid_min", ncFloat, xData[0]);
        x.putAtt("valid_max", ncFloat, xData[ scan->dimLon - 1 ]);
        free(xData);


        // write y-axis information

        float *yData = (float *) malloc(sizeof (float) * scan->dimLat);

        for (int i = 0; i < scan->dimLat; i++)
        {
            RDCartesianPoint cp = rcs.cartesianCoordinate(rdGridPoint(0, i));
            yData[i] = cp.y;
        }

        y.putVar(yData);
        y.putAtt("valid_min", ncFloat, yData[0]);
        y.putAtt("valid_max", ncFloat, yData[ scan->dimLon - 1 ]);
        free(yData);

        // write z-Axis information

#if ADD_DIMENSION_Z
        float *zData = (float *) malloc(sizeof (float) * 1);
        zData[0] = 0.0;
        z.putVar(zData);
        z.putAtt("valid_min", ncFloat, zData[0]);
        z.putAtt("valid_max", ncFloat, zData[0]);
        free(zData);
#endif

        return file;
    }

    const char * CFRadolanDataStandardName(RDScanType t)
    {
        const char *result = NULL;

        switch (t)
        {
            case RD_RX:
            case RD_EX:
                result = "reflectivity";
                break;
            case RD_RZ:
            case RD_RY:
            case RD_RV:
            case RD_EZ:
            case RD_RH:
            case RD_RB:
            case RD_RW:
            case RD_RL:
            case RD_RU:
            case RD_RS:
            case RD_RQ:
            case RD_SQ:
            case RD_SH:
            case RD_SF:
                result = "rainrate";
                break;
            default:
                result = "unknown";
                break;
        }
        return result;
    }

    void CFPrintConvertedRadolanScan(NcFile *file, int latVertices, int lonVertices)
    {
        using namespace netCDF;
        int dimLon = file->getDim("x").getSize();
        int dimLat = file->getDim("y").getSize();

#if ADD_DIMENSION_Z
        int dimZ = file->getDim("z").getSize();
#endif

        // int dimT = file->get_dim(NcToken("time"))->size();

        NcVar data = file->getVar("reflectivity");
        NcVarAtt product = data.getAtt("radolan_product");
        std::string typeIdentifier;
        product.getValues(typeIdentifier);
        RDScanType scanType = RDScanTypeFromString(typeIdentifier.c_str());

#if ADD_DIMENSION_Z
        float values[dimZ][dimLat][dimLon];
#else
        float values[dimLat][dimLon];
#endif
        data.getVar(values);

#if ADD_DIMENSION_Z
        for (int iz = 0; iz < dimZ; iz++)
        {
#endif
            for (int iy = 0; iy < dimLat; iy++)
            {
                if (iy % latVertices == 0)
                {
                    for (int ix = 0; ix < dimLon; ix++)
                    {
                        if (ix % lonVertices == 0)
                        {
#if ADD_DIMENSION_Z
                            float value = values[iz][iy][ix];
#else
                            float value = values[iy][ix];
#endif
                            std::cout << (RDIsCleanMeasurementAndNotMin(scanType, value) ? "*" : " ");
                        }
                    }
                    std::cout << std::endl;
                }
            }
#if ADD_DIMENSION_Z
            std::cout << std::endl;
        }
#endif
    }
}