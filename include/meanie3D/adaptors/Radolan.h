/* The MIT License (MIT)
 * 
 * (c) Jürgen Simon 2014 (juergen.simon@uni-bonn.de)
 * 
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 * 
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 * 
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

#ifndef M3D_ADAPTORS_RADOLAN_H
#define M3D_ADAPTORS_RADOLAN_H

#include <meanie3D/defines.h>
#include <meanie3D/namespaces.h>
#include <meanie3D/exceptions.h>

#include <string>
#include <netcdf>
#include <radolan/radolan.h>

using namespace Radolan;

namespace m3D {

    /** Converts the radolan file at path into a CF-Metadata compliant NetCDF-File.
     * 
     * @param radolanPath full path to the radolan file
     * @param netcdfPath full path to the netcdf file to be created
     * @param write_one_bytes_as_byte if <code>true</code> one byte products
     * such as RX are written out as BYTE instead of FLOAT
     * @param threshold minimum treshold for values to make it in the NetCDF file
     * @param mode NcFile::Mode for opening the netcdf file with
     * @param omitOutside @see RDReadScan
     * @return NCFile* NetCDF-Filehandler
     * @throw CFFileConversionException
     */
    netCDF::NcFile *CFConvertRadolanFile(const char *radolanPath,
                                         const char *netcdfPath,
                                         bool write_one_bytes_as_byte = false,
                                         const RDDataType *threshold = NULL,
                                         netCDF::NcFile::FileMode mode = netCDF::NcFile::replace,
                                         bool omitOutside = true) 
    throw(m3D::CFFileConversionException);

    /** Converts the radolan file at path into a CF-Metadata compliant NetCDF-File. 
     * @param radolanPath full path to the radolan file
     * @param netcdfPath full path to the netcdf file to be created
     * @param write_one_bytes_as_byte if <code>true</code> one byte products
     * such as RX are written out as BYTE instead of FLOAT
     * @param threshold minimum treshold for values to make it in the NetCDF file
     * @param mode NcFile::Mode for opening the netcdf file with
     * @return NCFile* NetCDF-Filehandler
     * @throw CFFileConversionException
     */
    netCDF::NcFile *CFConvertRadolanScan(RDScan *scan,
                                         const char *netcdfPath,
                                         bool write_one_bytes_as_byte,
                                         const RDDataType *threshold = NULL,
                                         netCDF::NcFile::FileMode mode = netCDF::NcFile::write)
    throw(m3D::CFFileConversionException);

    /** Simple function to get a visual rep of the file with ascii characters 
     * on terminal.
     * @param NcFile* netcdf Radolan file in CF-Metadata format 
     * @param print values every latVertices points in y
     * @param print values every lonVertices points in x
     */
    void CFPrintConvertedRadolanScan(netCDF::NcFile *file,
                                     int latVertices = 20, 
                                     int lonVertices = 20);

    /** CF-Metadata 'standard_name' for the given scan type 
     * @param scanType
     * @return standard_name
     */
    const char *CFRadolanDataStandardName(RDScanType scanType);

}

#endif
