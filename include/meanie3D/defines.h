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

#ifndef M3D_DEFINES_H
#define M3D_DEFINES_H

// If enabled, this switches on vector size assertion
// in vector utils
#define ASSERT_VECTOR_SIZES 0

/** used to indicate that a NetCDF file has a one-dimensional
 * or non-existing time variable.
 */
#define NO_TIME -1

// ---------------------------------------------------- //
// Configuration
// ---------------------------------------------------- //

// Enable the replacement of zero-shift vectors
// with an average of neighboring points
#define REPLACE_ZEROSHIFT_VECTORS 0

// Figure out the strongest neighbor to the
// zero-shift cluster and add it to the cluster
#define ADD_STRONGEST_NEIGHBOUR 0

// If switched on, the scale-space filter
// excludes all points that were marked as
// 'off limits' in feature-space construction
#define SCALE_SPACE_SKIPS_NON_ORIGINAL_POINTS 0

// Method for rounding vectors to grid resolution

#define GRID_ROUNDING_METHOD_FLOOR 0
#define GRID_ROUNDING_METHOD_ROUND 1
#define GRID_ROUNDING_METHOD_CEIL 0
#define GRID_ROUNDING_METHOD_RINT 0
#define GRID_ROUNDING_METHOD_NONE 0

// ---------------------------------------------------- //
// Debugging Flags (stdout)
// ---------------------------------------------------- //

// #define DEBUG_GRAPH_AGGREGATION 0
// #define WRITE_CI_SCORE 0

// ---------------------------------------------------- //
// Debugging Flags (files)
// ---------------------------------------------------- //

// // Write the feature-space out in .vtk file format
// #define WRITE_FEATURESPACE 0

// // Write out modes found by the iterations or graph approach
// #define WRITE_MODES 0

// // Write out a VTK file that contains 1 and 0 for
// // all points. 1 means, that the point was marked
// // as 'off limits' in the original feature-space
// // (one of the variables outside of valid_range).
// #define WRITE_OFF_LIMITS_MASK 0

// // Write out the result of the initial clustering
// // step into separate files.
// #define WRITE_ZEROSHIFT_CLUSTERS 0

#endif
