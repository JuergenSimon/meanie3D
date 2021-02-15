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

#ifndef M3D_CONRAD_CLUSTER_H
#define M3D_CONRAD_CLUSTER_H

#include <meanie3D/defines.h>
#include <meanie3D/namespaces.h>

#include <stdlib.h>
#include <time.h>

namespace m3D {

    using namespace Radolan;

    template<typename T>
    class ConradCluster
    {
    public:

        typedef std::vector<ConradCluster<T> > track_t;
        typedef std::map<m3D::id_t, track_t *> trackmap_t;

        int year, month, day, hour, minute;
        m3D::id_t id;
        int cellStatus;
        T centerY, centerX;
        int numCorePixels, numPixels, lifetime, hailWarning, pixelPerHour, directionDeg;
        T yMin, xMin, yMax, xMax;

        // Constructor/Destructor

        ConradCluster() {
        };

        ~ConradCluster() {
        };

        // timestamp

        /** @return seconds since epoch constructed from the
         * cluster's fields
         */
        long secondsSinceEpoch() const {
            // find out if Daylight Saving Time is on
            // by obtaining local time

            time_t now;

            time(&now);

            // initialize the scan time

            struct tm t;

            // gmtime_r does not make timezone adjustment. Since the
            // Radolan timestamps are supposed to be UTC, so will the
            // result of gmtime_r
            gmtime_r((const time_t *) &now, &t);

            // construct the server timestamp @ GMT with DST set up correctly
            t.tm_sec = 0;
            t.tm_min = minute;
            t.tm_hour = hour;
            t.tm_mday = day;
            t.tm_mon = month;
            t.tm_year = (year > 50) ? (1900 + year) : (2000 + year);

            // Now convert to seconds since epoch

            time_t time = timegm(&t);

            return time;
        }

        // Conversion to RADOLAN grid

        /** Calculates the values for center in RADOLAN cartesian
         * coordinates. (x,y)
         */
        vector <T> center() const {
            RDCoordinateSystem rcs(RD_RX);
            RDGeographicalPoint geo = rdGeographicalPoint(centerX, centerY);
            RDCartesianPoint cart = rcs.cartesianCoordinate(geo);

            std::vector<T> c(2, 0);
            c[0] = cart.x;
            c[1] = cart.y;

            return c;
        }

        /** @return lower left corner of the bounding box in 
         * cartesian coordinates
         */
        vector <T> box_min() const {
            RDCoordinateSystem rcs(RD_RX);
            RDGeographicalPoint geo = rdGeographicalPoint(xMin, yMin);
            RDCartesianPoint cart = rcs.cartesianCoordinate(geo);

            std::vector<T> c(2, 0);
            c[0] = cart.x;
            c[1] = cart.y;

            return c;
        }

        /** @return upper right corner of the bounding box in
         * cartesian coordinates
         */
        vector <T> box_max() const {
            RDCoordinateSystem rcs(RD_RX);
            RDGeographicalPoint geo = rdGeographicalPoint(xMax, yMax);
            RDCartesianPoint cart = rcs.cartesianCoordinate(geo);

            std::vector<T> c(2, 0);
            c[0] = cart.x;
            c[1] = cart.y;

            return c;
        }

        // input

        static
        std::vector<ConradCluster<T> >
        read_conrad_short(std::string filename) {
            std::vector<ConradCluster<T> > result;

            std::ifstream file(filename.c_str());

            if (file.is_open()) {
                std::string line;

                while (std::getline(file, line)) {
                    std::stringstream linestream(line);

                    //        01 yy (year)
                    //        02 mm (month)
                    //        03 dd (day)
                    //        04 hh (hour)
                    //        05 mi (minute)
                    //        06 Zell-Nummer
                    //        07 Zell-Status (0=neu, 1=erste..,2= zweite Wiedererkennnung)
                    //        08 Y-Koordinate der Zellmitte
                    //        09 X-Koordinate der Zellmitte
                    //        10 Anzahl Zellkernpixel >46 dBZ
                    //        11 Anzahl der Zellpixel >55 dBZ
                    //        12 Lebensdauer in min (Startguthaben Neuzelle 2 min)
                    //        13 Hagelwarnstufe (1 wenn Anzahl_pixel_55>0, 2 wenn Anzahl_pixel_55>12 oder 1 Kernpixel>60dBZ)
                    //        14 j¸ngste 10min-Zugbahnrichtung Kernmittelpunkt in 360∞
                    //        15 Zuggeschwindigkeit in Pixel/h.
                    //        16 Zellrahmen: Y-Minimum
                    //        17 Zellrahmen: X-Minimum
                    //        18 Zellrahmen: Y-Maximum
                    //        19 Zellrahmen: X-Maximum

                    ConradCluster<T> cluster;

                    // Read the integers using the operator >>
                    linestream >> cluster.year >> cluster.month >> cluster.day >> cluster.hour >> cluster.minute
                               >> cluster.id >> cluster.cellStatus >> cluster.centerY >> cluster.centerX
                               >> cluster.numCorePixels >> cluster.numPixels >> cluster.directionDeg
                               >> cluster.lifetime >> cluster.hailWarning >> cluster.pixelPerHour
                               >> cluster.yMin >> cluster.xMin >> cluster.yMax >> cluster.xMax;

                    result.push_back(cluster);
                }
            } else {
                cerr << "ERROR:Could not open file " << filename << endl;
            }

            return result;
        }

        static void
        print(const std::vector<ConradCluster<T> > &list) {
            cout << "date"
                 << "\t\tid"
                 << "\tcellStatus"
                 << "\tage"
                 << "\tnumPixel"
                 << "\tspeed [pixels/h]"
                 << "\tdirection [deg]"
                 << "\thail_warning"
                 << "\tnumCorePixel"
                 << "\t\tcenter"
                 << "\t\tmin"
                 << "\t\tmax"
                 << endl;

            typename std::vector<ConradCluster<T> >::const_iterator ci;
            for (ci = list.begin(); ci != list.end(); ++ci) {
                ConradCluster<T> c = *ci;

                ostringstream date;
                date << std::setfill('0') << std::setw(2) << std::setprecision(0);
                date << c.day << "." << c.month << "." << c.year << " " << c.hour << ":" << c.minute;

                vector<T> min, max;
                c.box(min, max);

                cout << date.str()
                     << "\t\t" << c.id
                     << "\t" << c.cellStatus
                     << "\t" << c.lifetime
                     << "\t" << c.numPixels
                     << "\t" << c.pixelPerHour
                     << "\t" << c.directionDeg
                     << "\t" << c.hailWarning
                     << "\t" << c.numCorePixels
                     << "\t\t" << c.center()
                     << "\t\t" << c.box_min()
                     << "\t\t" << c.box_max()
                     << endl;
            }
        }
    };
}

#endif