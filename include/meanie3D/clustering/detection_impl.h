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
#ifndef M3D_DETECTION_IMPL_H
#define    M3D_DETECTION_IMPL_H

#include <exception>
#include <fstream>
#include <limits>
#include <locale>
#include <map>
#include <netcdf>
#include <stdlib.h>
#include <sstream>
#include <string>
#include <vector>

#include <boost/cast.hpp>

#include <meanie3D/utils/verbosity.h>
#include <meanie3D/filters/convection_filter.h>
#include <meanie3D/filters/replacement_filter.h>
#include <meanie3D/filters/scalespace_filter.h>
#include <meanie3D/filters/weight_filter.h>
#include <meanie3D/weights/weight_function_factory.h>

#include "detection.h"
#include "cluster_list.h"

namespace m3D {

    using namespace ::units;
    using namespace utils;
    using namespace utils::vectors;
    using namespace std;
    using namespace boost;
    using namespace netCDF;
    using namespace m3D;

    template<typename T>
    const double
            Detection<T>::NO_SCALE = numeric_limits<double>::min();

#pragma mark -
#pragma mark Defaults

    template<typename T>
    detection_params_t<T>
    Detection<T>::defaultParams() {
        detection_params_t<T> p;
        p.filename = "";
        p.output_filename = "";
        p.parameters = "";
        p.time_index = -1;
        p.min_cluster_size = 1u;
        p.weight_function_name = "default";
        p.wwf_lower_threshold = 0;
        p.wwf_upper_threshold = std::numeric_limits<T>::max();
        p.kernel_name = "uniform";
        p.previous_clusters_filename = NULL;
        p.postprocess_with_previous_output = false;
        p.ci_comparison_file = NULL;
        p.ci_comparison_protocluster_file = NULL;
        p.ci_use_walker_mecikalski = false;
        p.ci_satellite_only = false;
        p.ci_protocluster_scale = 25.0;
        p.ci_protocluster_min_size = 10;
        p.include_weight_in_result = false;
        p.cluster_coverage_threshold = 0.66;
        p.convection_filter_index = -1;
        p.coalesceWithStrongestNeighbour = false;
        p.spatial_range_only = false;
        p.scale = Detection<T>::NO_SCALE;
        p.verbosity = VerbosityNormal;
        p.inline_tracking = false;
        return p;
    }

    template<typename T>
    void
    Detection<T>::initialiseContext(detection_context_t<T> &ctx) {
        ctx.file = NULL;
        ctx.clusters = NULL;
        ctx.previous_clusters = NULL;
        ctx.search_params = NULL;
        ctx.kernel = NULL;
        ctx.kernel_width = 0.0;
        ctx.show_progress = false;
        ctx.data_store = NULL;
        ctx.fs = NULL;
        ctx.decay = 0.01;
        ctx.timestamp = 0;
        ctx.coord_system = NULL;
        ctx.data_store = NULL;
        ctx.index = NULL;
        ctx.initialised = false;
        ctx.sf = NULL;
        ctx.weight_function = NULL;
        ctx.wwf_apply = false;
    }

    template<typename T>
    void
    Detection<T>::initialiseContext(const detection_params_t<T> &params,
                                    detection_context_t<T> &ctx) {
        Detection<T>::initialiseContext(ctx);
        try {
            ctx.file = new NcFile(params.filename, NcFile::read);
        } catch (const netCDF::exceptions::NcException &e) {
            cerr << "ERROR: could not open file '" << params.filename
                 << "' for reading: " << e.what() << endl;
            exit(EXIT_FAILURE);
        }

        ctx.data_store = new NetCDFDataStore<T>(params.filename,
                                                params.variables,
                                                params.dimensions,
                                                params.dimension_variables,
                                                params.time_index);

        ctx.show_progress = (params.verbosity > VerbositySilent);

        // Get timestamp
        ctx.timestamp = netcdf::get_time_checked<timestamp_t>(params.filename, params.time_index);

        // Calculate mean-shift bandwidth if necessary
        if (!params.ranges.empty()) {
            // calculate kernel width as average of ranges
            ctx.kernel_width = boost::numeric_cast<T>(0.0);
            for (size_t i = 0; i < params.dimensions.size(); i++) {
                ctx.kernel_width += params.ranges[i];
            }
            ctx.kernel_width /= boost::numeric_cast<T>(params.dimensions.size());
            ctx.bandwidth = params.ranges;
        } else {
            // automatically calculate bandwidths
            T t = (params.scale == Detection<T>::NO_SCALE) ? 1.0 : params.scale;
            ctx.filter_width = ScaleSpaceFilter<T>::scale_to_filter_width(t);
            for (size_t i = 0; i < params.dimensions.size(); i++) {
                ctx.bandwidth.push_back(ctx.filter_width);
            }
            // value range
            for (size_t i = 0; i < ctx.data_store->rank(); i++) {
                T range = ctx.data_store->valid_max(i) - ctx.data_store->valid_min(i);
                ctx.bandwidth.push_back(range);
            }
        }
        ctx.search_params = new RangeSearchParams<T>(ctx.bandwidth);

        // Calculate kernel width if necessary
        if (params.scale != Detection<T>::NO_SCALE) {
            double width = ScaleSpaceFilter<T>::scale_to_filter_width(params.scale);
            if (params.ranges.empty()) {
                ctx.kernel_width = width;
                ctx.filter_width = width;
            }
        }

        // Construct a coordinate system object from the dimensions
        // and dimension variables given
        ctx.coord_system = ctx.data_store->coordinate_system();

        ctx.wwf_apply = (params.wwf_lower_threshold != 0
                         || params.wwf_upper_threshold != std::numeric_limits<T>::max());

        // Construct the kernel
        if (params.kernel_name == "uniform") {
            ctx.kernel = new UniformKernel<T>(ctx.kernel_width);
        } else if (params.kernel_name == "gauss") {
            ctx.kernel = new GaussianNormalKernel<T>(ctx.kernel_width);
        } else if (params.kernel_name == "epanechnikov") {
            ctx.kernel = new EpanechnikovKernel<T>(ctx.kernel_width);
        }

        if (params.inline_tracking || params.postprocess_with_previous_output) {
            if (params.previous_clusters_filename == NULL) {
                cerr << "inline tracking or postprocessing with previous output"
                     << " wanted but previous output is missing" << endl;
                exit(EXIT_FAILURE);
            }
            ctx.previous_clusters = ClusterList<T>::read(*params.previous_clusters_filename);
        }

        ctx.initialised = true;
    };

#define delete_and_clear(X) if (X != NULL) {delete X; X = NULL;}

    template<typename T>
    void
    Detection<T>::cleanup(detection_params_t<T> &params,
                          detection_context_t<T> &ctx) {
        // context
        ctx.clusters->clear();
        ctx.fs->clear();
        delete_and_clear(ctx.search_params)
        delete_and_clear(ctx.data_store);
        delete_and_clear(ctx.fs);
        delete_and_clear(ctx.weight_function);
        delete_and_clear(ctx.sf);
        delete_and_clear(ctx.kernel);
        delete_and_clear(ctx.index);
        delete_and_clear(ctx.clusters);
        delete_and_clear(ctx.previous_clusters);
        delete_and_clear(ctx.file);

        // params
        delete_and_clear(params.previous_clusters_filename);
        delete_and_clear(params.ci_comparison_file);
        delete_and_clear(params.ci_comparison_protocluster_file);
    }

    template<typename T>
    void
    Detection<T>::run(const detection_params_t<T> &params,
                      detection_context_t<T> &ctx) {
        if (!ctx.initialised) {
            Detection<T>::initialiseContext(params, ctx);
        }

        // used in writing out debug data
        boost::filesystem::path path(params.filename);

        // Construct Featurespace from data
        ctx.fs = new FeatureSpace<T>(
                ctx.coord_system,
                ctx.data_store,
                params.lower_thresholds,
                params.upper_thresholds,
                params.replacement_values,
                ctx.show_progress);

        // Run replacement filters
        if (!params.replacementFilterVariableIndex.empty()) {
            size_t rfvi_length = params.replacementFilterVariableIndex.size();
            for (size_t i = 0; i < rfvi_length; i++) {
                int rvi = params.replacementFilterVariableIndex[i];
                typename ReplacementFilter<T>::ReplacementMode mode = params.replacementFilterModes.at(rvi);
                float percent = params.replacementFilterPercentages.at(rvi);
                if (params.verbosity >= VerbosityNormal) {
                    start_timer("Applying replacement filter for " + params.variables[rvi]);
                }
                ReplacementFilter<T> rf(mode, rvi, ctx.bandwidth, percent);
                rf.apply(ctx.fs);
                if (params.verbosity >= VerbosityNormal) {
                    stop_timer("done.");
                }
            }
        }

#if WRITE_FEATURESPACE
        static size_t fs_index = 0;
        std::string fn = path.stem().string() + "_featurespace_" + boost::lexical_cast<string>(fs_index++) + ".vtk";
        VisitUtils<T>::write_featurespace_vtk(fn, ctx.fs);
#endif

#if WRITE_OFF_LIMITS_MASK
        std::string ol_fname = path.filename().stem().string() + "-off_limits.vtk";
        VisitUtils<T>::write_multiarray_vtk(ol_fname, "off_limits", ctx.coord_system, ctx.fs->off_limits());
#endif

        // Convection Filter?
        if (params.convection_filter_index >= 0) {
            if (params.verbosity >= VerbosityNormal) {
                start_timer("Applying convection filter");
            }
            ConvectionFilter<T> convection_filter(ctx.bandwidth,
                                                  params.convection_filter_index, ctx.show_progress);
            convection_filter.apply(ctx.fs);
            if (params.verbosity >= VerbosityNormal) {
                stop_timer("done.");
            }
        }

        // Scale-Space smoothing
        if (params.scale != NO_SCALE) {

            vector<T> resolution = ctx.fs->coordinate_system->resolution();
            ctx.sf = new ScaleSpaceFilter<T>(params.scale,
                                             resolution,
                                             params.exclude_from_scale_space_filtering,
                                             ctx.decay,
                                             ctx.show_progress);
            ctx.sf->apply(ctx.fs);

#if WRITE_FEATURESPACE
            std::string fn = path.stem().string() + "_scale_" + boost::lexical_cast<string>(params.scale) + ".vtk";
            VisitUtils<T>::write_featurespace_vtk(fn, ctx.fs);
#endif
        }

        // Construct the weight function
        if (params.verbosity > VerbositySilent) {
            start_timer("Constructing weight function: " + params.weight_function_name);
        }
        ctx.weight_function = WeightFunctionFactory<T>::create(params, ctx);
        if (params.verbosity > VerbositySilent) {
            stop_timer("done");
        }

        // Apply weight function filtering
        if (ctx.wwf_apply) {
            if (params.verbosity > VerbositySilent) {
                start_timer("Applying weight function filter ...");
            }

            // Apply weight function filter
            WeightThresholdFilter<T> wtf(ctx.weight_function,
                                         params.wwf_lower_threshold,
                                         params.wwf_upper_threshold,
                                         ctx.show_progress);
            wtf.apply(ctx.fs);

            if (params.verbosity > VerbositySilent) {
                stop_timer("done");
                cout << "Filtered featurespace contains "
                     << ctx.fs->count_original_points() << " original points "
                     << endl;
            }
        }

#if WITH_VTK
        if (params.write_weight_function) {
            std::string wfname = "weights-" + path.filename().stem().string();
            start_timer("Writing weight function");
            VisitUtils<T>::write_weight_function_response(wfname, ctx.fs, ctx.weight_function);
            stop_timer("done");
        }

        if (!params.vtk_variables.empty()) {

            string filename_only = path.filename().string();
            boost::filesystem::path destination_path = boost::filesystem::path(".");
            destination_path /= filename_only;
            destination_path.replace_extension();
            string dest_path = destination_path.generic_string();

            if (params.verbosity > VerbositySilent) {
                cout << "Writing featurespace-variables ...";
            }

            VisitUtils<T>::write_featurespace_variables_vtk(dest_path,
                                                            ctx.fs,
                                                            ctx.data_store->variables(),
                                                            params.vtk_variables,
                                                            false);

            if (params.verbosity > VerbositySilent) {
                cout << " done." << endl;
            }
        }
#endif

        // Create the quick lookup index to speed up mean-shift
        // clustering. By default this is FLANN K/D-tree implementation.
        ctx.index = PointIndex<T>::create(ctx.fs->get_points(), ctx.fs->rank());

        // Perform the actual clustering
        ClusterOperation<T> cop(params, ctx);
        ctx.clusters = cop.cluster();

#if WITH_VTK
        if (params.write_meanshift_vectors) {
            std::string ms_path = "meanshift-vectors-" + path.filename().stem().string() + ".vtk";
            VisitUtils<T>::write_shift_vectors(ms_path, ctx.fs, true);
        }
#endif

        // Axe weenies
        ctx.clusters->apply_size_threshold(params.min_cluster_size);

        // The survivors are now eligible for an actual id
        // TODO: we need to move away from doing this as part
        // of the detection process. The id is something only
        // to be assigned/changed in tracking!
        m3D::id_t id = m3D::MIN_ID;
        ClusterUtils<T>::provideIds(ctx.clusters, id);
        ctx.clusters->highest_id = id;

        // Give (provisional) universal identifiers
        // TODO: store the uuid in a place that can be accessed 
        // between the runs, so that the uuid becomes a true
        // uuid from the point of detection. At this time, only
        // the tracking can provide this by getting the highest_uuid
        // from the previous file.
        m3D::uuid_t uuid = m3D::MIN_UUID;
        ClusterUtils<T>::provideUuids(ctx.clusters, uuid);
        ctx.clusters->highest_uuid = uuid;

        // Collate with previous clusters, if provided
        if (params.postprocess_with_previous_output && params.previous_clusters_filename != NULL) {
            cout << endl << "Collating with previous results:" << endl;
            if (params.verbosity >= VerbosityDetails)
                ctx.clusters->print();

            try {
                if (params.verbosity >= VerbosityNormal)
                    cout << "Comparing " << ctx.clusters->size()
                         << " new clusters to " << ctx.previous_clusters->size()
                         << " old clusters" << endl;

                if (params.verbosity >= VerbosityDetails) {
                    cout << "List of new clusters:" << endl;
                    ctx.clusters->print();
                    cout << endl << "List of previous clusters:" << endl;
                    ctx.previous_clusters->print();
                }

                ClusterUtils<T> cluster_filter(params.cluster_coverage_threshold);
                cluster_filter.filter_with_previous_clusters(
                        ctx.previous_clusters,
                        ctx.clusters,
                        ctx.coord_system,
                        ctx.weight_function,
                        params.verbosity);

            } catch (const std::exception &e) {
                cerr << "FATAL:exception reading previous cluster file: " << e.what() << endl;
                exit(EXIT_FAILURE);
            }
            cout << endl << "Done. Have " << ctx.clusters->size() << " clusters:" << endl;

            if (params.verbosity >= VerbosityDetails)
                ctx.clusters->print();

        } else {
            // No previous file? Provide UUIDs from scratch
        }

        // Announce final results
        if (params.verbosity > VerbositySilent)
            cout << endl << "Final result: found " << ctx.clusters->size() << " objects: " << endl;

        if (params.verbosity >= VerbosityDetails)
            ctx.clusters->print();

        // Write out the cluster list

#if WITH_VTK
        if (params.write_vtk && ctx.clusters->size() > 0) {
            ::m3D::utils::VisitUtils<T>::write_clusters_vtu(ctx.clusters,
                                                            ctx.coord_system, path.filename().string());
        }
        if (params.write_cluster_modes) {
            string modes_path = "clusters_modes-" + path.filename().stem().string() + ".vtk";
            ::m3D::utils::VisitUtils<T>::write_cluster_modes_vtk(modes_path,
                                                                 ctx.clusters->clusters, true);
        }
        if (params.write_cluster_centers) {
            string centers_path = "clusters_centers-" + path.filename().stem().string() + ".vtk";
            ::m3D::utils::VisitUtils<T>::write_geometrical_cluster_centers_vtk(centers_path,
                                                                               ctx.clusters->clusters);
        }
        if (params.write_weight_response && ctx.clusters->size() > 0) {
            string wr_path = "clusters_weight-" + path.filename().stem().string();
            ::m3D::utils::VisitUtils<T>::write_cluster_weight_response_vtk(wr_path,
                                                                           ctx.clusters->clusters, ctx.weight_function,
                                                                           false);
        }
#endif

        // Set the timestamp!!
        ctx.clusters->timestamp = ctx.timestamp;

        if (!params.inline_tracking) {

            if (params.verbosity > VerbositySilent) {
                std::string msg = "Writing clusters to NetCDF file " + params.output_filename + " ...";
                start_timer(msg);
            }

            if (!params.output_filename.empty()) {
                ctx.clusters->write(params.output_filename);
            }

            if (params.include_weight_in_result) {
                cout << "NOT IMPLEMENTED" << endl;
                // cout << "Writing weight function to result file ... ";
                // cout << "done." << endl;
            }
            if (params.verbosity > VerbositySilent) {
                stop_timer("done");
            }
        }
    }
}

#endif	/* M3D_DETECTION_IMPL_H */

