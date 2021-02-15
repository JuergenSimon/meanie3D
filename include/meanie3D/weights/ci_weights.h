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

#ifndef M3D_OASE_CI_WEIGHTFUNCTION_H
#define M3D_OASE_CI_WEIGHTFUNCTION_H

#include <meanie3D/defines.h>
#include <meanie3D/namespaces.h>

#include <meanie3D/array.h>
#include <meanie3D/clustering/cluster_op.h>
#include <meanie3D/clustering/cluster_list.h>
#include <meanie3D/filters/scalespace_filter.h>
#include <meanie3D/filters.h>
#include <meanie3D/tracking.h>
#include <meanie3D/utils/time_utils.h>

#include <netcdf>
#include <vector>
#include <map>

#include "weight_function.h"

namespace m3D {

    //
    // Constants
    //

    // Variables used for scoring scheme

    static const size_t CI_WEIGHT_NUM_VARS = 6;
    static const char *CI_WEIGHT_VARS[] = {
            "msevi_l15_ir_108",
            "msevi_l15_wv_062",
            "msevi_l15_ir_134",
            "cband_radolan_rx",
            "linet_oase_tl",
            "msevi_l15_hrv"
    };

    // Shorthands used to access variables in order

    static const int msevi_l15_ir_108 = 0;
    static const int msevi_l15_wv_062 = 1;
    static const int msevi_l15_ir_134 = 2;
    static const int cband_radolan_rx = 3;
    static const int linet_oase_tl = 4;
    static const int msevi_l15_hrv = 5;

    // Variables used for protoclusters

    static const size_t PROTOCLUSTER_NUM_VARS = 1;
    static const char *PROTOCLUSTER_VARS[] = {
            "msevi_l15_ir_108"
    };

    /** This class represents a weight function loosely based on the CI score
     * by Walker, MacKenzie Mecicalski, Jewett (2012). Only the static score
     * criteria are used (no time differences).
     *
     * It adds score by looking for radar and lighting signatures in a 5km
     * radius around a given point.
     *
     */
    template<class T>
    class OASECIWeightFunction : public WeightFunction<T>
    {
    private:

        //
        // Members
        //

        detection_params_t <T> m_super_params;
        detection_context_t <T> m_super_context;

        detection_params_t <T> m_params;
        detection_context_t <T> m_ctx;


        NetCDFDataStore <T> *m_data_store;
        NetCDFDataStore <T> *m_ci_comparison_data_store;

        // const std::string *m_ci_comparison_file;
        // const CoordinateSystem<T> *m_coordinate_system;

        MultiArray <T> *m_weight;
        MultiArray<bool> *m_overlap;

        std::vector<std::string> m_variable_names;
        // bool m_satellite_only;
        // bool m_use_walker_mecikalski_limits;

        std::vector<std::string> m_protocluster_variables;

        ClusterList <T> *m_previous_protoclusters;

        // T m_protocluster_scale;
        // int m_protocluster_min_size;

        MultiArray<bool> *m_prev_cluster_area;
        MultiArray<bool> *m_curr_cluster_area;

        // Attributes for calculating brightness
        // temperature from the spectral radiances
        map <size_t, T> m_c1;
        map <size_t, T> m_c2;
        map <size_t, T> m_alpha;
        map <size_t, T> m_beta;
        map <size_t, T> m_wavenumber;

        // Members for range based weight calculations
        PointIndex <T> *m_index; // index for range search
        vector <T> m_bandwidth;  // search radius for numerous operations
        SearchParameters *m_search_params; // search params for search

#if WRITE_CI_SCORE
        MultiArray<T> *m_score_108;
        MultiArray<T> *m_score_108_trend;
        MultiArray<T> *m_score_62_108;
        MultiArray<T> *m_score_134_108;
        MultiArray<T> *m_62_108_trend;
        MultiArray<T> *m_134_108_trend;
#endif

    public:

        /**
         * 
         * @param params
         * @param ctx
         */
        OASECIWeightFunction(const detection_params_t <T> &params,
                             const detection_context_t <T> &ctx)
                : m_super_params(params), m_super_context(ctx), m_data_store(NULL),
                  m_weight(new MultiArrayBlitz<T>(ctx.coord_system->get_dimension_sizes(), 0.0)) {
            using namespace utils;
            try {
                NcFile file(params.filename.c_str(), NcFile::read);
                for (size_t i = 0; i < CI_WEIGHT_NUM_VARS; i++) {
                    m_variable_names.push_back(std::string(CI_WEIGHT_VARS[i]));
                    NcVar var = file.getVar(CI_WEIGHT_VARS[i]);
                    if (var.isNull()) {
                        cerr << "FATAL: file requires variable " << CI_WEIGHT_VARS[i] << " for CI interest weight" <<
                             endl;
                        exit(EXIT_FAILURE);
                    }

                    // Obtain the constants for transforming radiances
                    // into brightness temperatures fromt he attributes:
                    namespace nu = m3D::utils::netcdf;
                    switch (i) {
                        case msevi_l15_ir_108:
                        case msevi_l15_wv_062:
                        case msevi_l15_ir_134: {
                            m_c1[i] = nu::get_attribute_value<T>(var, "rad_const1");
                            m_c2[i] = nu::get_attribute_value<T>(var, "rad_const2");
                            m_alpha[i] = nu::get_attribute_value<T>(var, "alpha");
                            m_beta[i] = nu::get_attribute_value<T>(var, "beta");
                            m_wavenumber[i] = nu::get_attribute_value<T>(var, "wavenum");
                        }
                    }
                }
            } catch (netCDF::exceptions::NcException &e) {
                cerr << "FATAL: can not read from netcdf file " << params.filename << endl;
                exit(EXIT_FAILURE);
            }

            // Create the data store
            this->m_data_store = new NetCDFDataStore<T>(params.filename,
                                                        m_variable_names,
                                                        params.dimensions,
                                                        params.dimension_variables,
                                                        params.time_index);

            // index for effective range search ops
            m_bandwidth = ctx.fs->spatial_component(ctx.bandwidth);
            m_index = PointIndex<T>::create(&ctx.fs->points, ctx.coord_system->rank());
            m_search_params = new RangeSearchParams<T>(m_bandwidth);

            this->obtain_protoclusters();

            if (params.ci_comparison_file != NULL) {

//#if WITH_OPENCV
                // Section kept for historic interest
                // Attempt (b) : estimate dense motion vector field using opencv
                // and shift values along the field
                //namespace ov = m3D::utils::opencv;
                //m_ci_comparison_data_store = ov::shifted_store_from_flow_of_variable(filename, *ci_comparison_file,
                //                                                                    fs->coordinate_system,
                //                                                                    this->m_variable_names,
                //                                                                    msevi_l15_hrv, 7.0);
//#endif

                // TODO: there is a problem here. The time index should
                // be configurable when the comparison file is the same
                // but has time dimension. 
                m_ci_comparison_data_store = new NetCDFDataStore<T>(params.filename,
                                                                    m_variable_names,
                                                                    params.dimensions,
                                                                    params.dimension_variables,
                                                                    params.time_index);

                if (params.ci_comparison_protocluster_file != NULL) {
                    // Load previous proto-clusters
                    m_previous_protoclusters = ClusterList<T>::read(*params.ci_comparison_protocluster_file);

                    // Shift previous data by tracking protoclusters and use
                    // the resulting tracking vectors / clusters
                    cout << endl << "Shifting comparison data ...";
                    start_timer();
                    this->shift_comparison_data_using_protoclusters(params.ci_comparison_protocluster_file);
                    cout << " done (" << stop_timer() << "s)" << endl;

                    // Reduce the data by calculating the cluster overlap area
                    // (used later in weight function calculation)
                    cout << endl << "Calculating overlap ...";
                    start_timer();
                    this->calculate_overlap();
                    cout << " done (" << stop_timer() << "s)" << endl;

                    // Replace all pixels with the with average of 25% coldest pixels
                    // in their vicinity (10km radius).
                    cout << endl << "Replacing with average of 25% coldest pixels (comparison data) ...";
                    start_timer();
                    // NOTE: using fs here is not entirely correct, because
                    // the featurespace was constructed from the wrong
                    // datastore. This 'should' not be a problem, because
                    // of the overlap calculation
                    this->replace_with_coldest_pixels(m_ci_comparison_data_store, ctx.fs);
                    cout << " done (" << stop_timer() << "s)" << endl;
                }
            }

            cout << endl << "Replacing with average of 25% coldest pixels (current data) ...";
            start_timer();
            this->replace_with_coldest_pixels(m_data_store, ctx.fs);
            cout << " done (" << stop_timer() << "s)" << endl;

            cout << endl << "Calculating final weight score ...";
            start_timer();
            calculate_weight_function(ctx.fs);
            cout << " done (" << stop_timer() << "s)" << endl;
        }

        ~OASECIWeightFunction() {

            if (this->m_data_store != NULL) {
                delete this->m_data_store;
                this->m_data_store = NULL;
            }

            if (this->m_index != NULL) {
                delete this->m_index;
                this->m_index = NULL;
            }

            if (this->m_search_params != NULL) {
                delete this->m_search_params;
                this->m_search_params = NULL;
            }

            if (this->m_ci_comparison_data_store != NULL) {
                delete this->m_ci_comparison_data_store;
                this->m_ci_comparison_data_store = NULL;
            }

            if (this->m_previous_protoclusters != NULL) {
                delete this->m_previous_protoclusters;
                this->m_previous_protoclusters = NULL;
            }

            if (this->m_overlap != NULL) {
                delete this->m_overlap;
                this->m_overlap = NULL;
            }

            if (this->m_prev_cluster_area != NULL) {
                delete this->m_prev_cluster_area;
                this->m_prev_cluster_area = NULL;
            }

            if (this->m_curr_cluster_area != NULL) {
                delete this->m_curr_cluster_area;
                this->m_curr_cluster_area = NULL;
            }

            // clean up clustering mess right away to free memory
            Detection<T>::cleanup(m_params, m_ctx);

        }

    private:

        void
        obtain_protoclusters() {

            cout << endl << endl;
            cout << "+ ---------------------------- +" << endl;
            cout << "+ Obtaining protoclusters      +" << endl;
            cout << "+ ---------------------------- +" << endl;

            m_params = Detection<T>::defaultParams();

            // Dimensions stay the same
            m_params.dimensions = m_super_params.dimensions;
            m_params.dimension_variables = m_super_params.dimension_variables;

            // Add variables
            try {
                for (size_t i = 0; i < PROTOCLUSTER_NUM_VARS; i++) {
                    std::string name = std::string(PROTOCLUSTER_VARS[i]);
                    m_params.variables.push_back(name);
                }
            } catch (netCDF::exceptions::NcException &e) {
                cerr << "FATAL: can not read from netcdf file " << m_ci_comparison_data_store->filename() << endl;
                exit(EXIT_FAILURE);
            }

            // cut msevi_l15_ir_108 at max 0 centigrade
            m_params.upper_thresholds[0] = spectral_radiance(msevi_l15_ir_108, 0);

            // Set other parameters
            m_params.filename = m_super_params.filename;
            m_params.scale = m_super_params.ci_protocluster_scale;
            m_params.min_cluster_size = m_super_params.ci_protocluster_min_size;
            m_params.verbosity = m_super_params.verbosity;

            Detection<T>::run(m_params, m_ctx);

            // Write protoclusters out
            boost::filesystem::path path(m_super_params.filename);
            std::string fn = "protoclusters-" + path.stem().generic_string<std::string>() + ".nc";
            m_ctx.clusters->write(fn);
        }

        void
        shift_comparison_data_using_protoclusters(const std::string *ci_comparison_protocluster_file) {
            using namespace utils::vectors;
            if (ci_comparison_protocluster_file != NULL) {
                vector<size_t> dims = m_super_context.coord_system->get_dimension_sizes();

                // Perform a tracking run
                tracking_param_t params = Tracking<T>::defaultParams();
                params.tracking_variable = std::string(PROTOCLUSTER_VARS[0]);

                // Time difference calculation can be a second or so
                // off. Allow some slack.
                params.max_deltaT = ::units::values::s(930);

                Tracking<T> proto_tracker(params);
                proto_tracker.track(m_previous_protoclusters, m_ctx.clusters);
                m_ctx.clusters->save();

                // Find object pairs and shift the all data from
                // the comparison scan within that object's area
                // to the 'forecasted' position using the center
                // displacement vector

                // Idea: improve on the result by using OpenCV's
                // affine transformation finder algorithm and
                // morph the pixels into position
                std::vector<std::vector<T> > origins, vectors;

                // initialize storage for shifted data
                typedef std::map<size_t, MultiArray<T> *> data_map_t;
                data_map_t shifted_data;
                for (size_t var_index = 0; var_index < m_ci_comparison_data_store->rank(); var_index++) {
                    T NOT_SET = m_ci_comparison_data_store->fill_value(var_index);
                    shifted_data[var_index] = new MultiArrayBlitz<T>(dims, NOT_SET);
                }

                //                ::m3D::utils::opencv::display_variable(m_ci_comparison_data_store,msevi_l15_ir_108);
                //                ::m3D::utils::opencv::display_array(m_ci_comparison_data_store->get_data(msevi_l15_ir_108),
                //                                                    m_ci_comparison_data_store->min(msevi_l15_ir_108),
                //                                                    m_ci_comparison_data_store->max(msevi_l15_ir_108));

                // iterate over clusters
                for (size_t pi = 0; pi < m_previous_protoclusters->size(); pi++) {

                    typename Cluster<T>::ptr pc = m_previous_protoclusters->clusters.at(pi);

                    // find the matched candidate
                    for (size_t ci = 0; ci < m_ctx.clusters->size(); ci++) {
                        typename Cluster<T>::ptr cc = m_ctx.clusters->clusters.at(ci);

                        if (pc->id == cc->id) {
                            // Calculate average displacement
                            typedef std::vector<T> vec_t;
                            vec_t center_p = pc->geometrical_center();
                            vec_t center_c = cc->geometrical_center();
                            vec_t displacement = center_c - center_p;

                            origins.push_back(center_p);
                            vectors.push_back(displacement);

                            // Move previous data by displacement vector
                            typename Point<T>::list::iterator point_iter;

                            for (point_iter = pc->get_points().begin();
                                 point_iter != pc->get_points().end(); point_iter++) {
                                typename Point<T>::ptr p = *point_iter;
                                vector<T> x = p->coordinate + displacement;
                                vector<int> source_gridpoint = p->gridpoint;
                                vector<int> dest_gridpoint = m_ctx.coord_system->newGridPoint();
                                try {
                                    m_ctx.coord_system->reverse_lookup(x, dest_gridpoint);
                                    for (size_t var_index = 0;
                                         var_index < m_ci_comparison_data_store->rank(); var_index++) {
                                        bool is_in_range = false;
                                        bool is_valid = false;
                                        T value = m_ci_comparison_data_store->get(var_index,
                                                                                  source_gridpoint,
                                                                                  is_in_range,
                                                                                  is_valid);
                                        if (is_valid && is_in_range) {
                                            // we are manipulating raw data in the NetCDFDataStore, which
                                            // keeps the values in 'packed' format internally. When calling
                                            // the get method above, the value is unpacked for convencience.
                                            // This means we need to pack the value again before writing it
                                            // out or the value range will be messed up.
                                            T packed_value = m_ci_comparison_data_store->packed_value(var_index, value);
                                            shifted_data[var_index]->set(dest_gridpoint, packed_value);
                                        }
                                    }
                                } catch (std::out_of_range &e) {
                                }
                            }
                        }
                    }
                }

                //                ::m3D::utils::opencv::display_array(shifted_data[msevi_l15_ir_108],
                //                                                    m_ci_comparison_data_store->min(msevi_l15_ir_108),
                //                                                    m_ci_comparison_data_store->max(msevi_l15_ir_108));

                // replace the original data with the shifted data
                for (size_t var_index = 0; var_index < m_ci_comparison_data_store->rank(); var_index++) {
                    MultiArray<T> *dest = shifted_data[var_index];
                    m_ci_comparison_data_store->set_data(var_index, dest);
                }

                //::m3D::utils::opencv:: display_variable(m_ci_comparison_data_store,msevi_l15_ir_108);
                boost::filesystem::path ppath(m_previous_protoclusters->source_file);
                std::string fn = "shifted-" + ppath.filename().stem().generic_string() + ".nc";
                m_ci_comparison_data_store->save_as(fn);
#if WITH_VTK
                fn = "vectors-" + ppath.filename().stem().generic_string() + ".vtk";
                VisitUtils<T>::write_vectors_vtk(fn, origins, vectors);
#endif
            }
        }

        // calculate overlap mask

        void
        calculate_overlap() {
            vector<size_t> dims = m_ctx.coord_system->get_dimension_sizes();
            m_overlap = new MultiArrayBlitz<bool>(dims, false);
            m_prev_cluster_area = new MultiArrayBlitz<bool>(dims, false);
            m_curr_cluster_area = new MultiArrayBlitz<bool>(dims, false);

            // Mark area occupied by all protoclusters from previous set
            for (size_t pi = 0; pi < m_previous_protoclusters->size(); pi++) {
                typename Cluster<T>::ptr c = m_previous_protoclusters->clusters.at(pi);
                typename Point<T>::list::iterator point_iter;
                for (point_iter = c->get_points().begin(); point_iter != c->get_points().end(); point_iter++) {
                    typename Point<T>::ptr p = *point_iter;
                    m_prev_cluster_area->set(p->gridpoint, true);
                }
            }

            // Mark area occupied by all protoclusters from current set

            for (size_t pi = 0; pi < m_ctx.clusters->size(); pi++) {
                typename Cluster<T>::ptr c = m_ctx.clusters->clusters.at(pi);
                typename Point<T>::list::iterator point_iter;
                for (point_iter = c->get_points().begin(); point_iter != c->get_points().end(); point_iter++) {
                    typename Point<T>::ptr p = *point_iter;
                    m_curr_cluster_area->set(p->gridpoint, true);
                }
            }

            // Collate

            class OverlapFunctor : public MultiArray<bool>::ForEachFunctor
            {
            public:

                MultiArray<bool> *m_overlap;
                MultiArray<bool> *m_curr_cluster_area;

                OverlapFunctor(MultiArray<bool> *overlap, MultiArray<bool> *curr_cluster_area)
                        : m_overlap(overlap), m_curr_cluster_area(curr_cluster_area) {
                };

                // for_each callback functor

                void
                operator()(const vector<int> &index, const bool have_previous) {
                    bool have_current = m_curr_cluster_area->get(index);
                    m_overlap->set(index, have_current && have_previous);
                }
            };

            OverlapFunctor f(m_overlap, m_curr_cluster_area);
            m_prev_cluster_area->for_each(&f);

            delete m_prev_cluster_area;
            m_prev_cluster_area = NULL;

            delete m_curr_cluster_area;
            m_prev_cluster_area = NULL;
        }

        // replace each data point in the overlap area
        // with the average of the 25% coldest points
        // within a radius h around it

        void
        replace_with_coldest_pixels(NetCDFDataStore <T> *ds, FeatureSpace <T> *fs) {
            float percentage = 0.25;

            // calculate bandwidth in pixels
            vector<int> bandwidth;
            vector<T> resolution = ds->coordinate_system()->resolution();
            for (size_t i = 0; i < m_bandwidth.size(); i++)
                bandwidth.push_back((int) round(m_bandwidth[i] / resolution[i]));

            // use linear mapping to parallelize the operation
            LinearIndexMapping mapping(ds->get_dimension_sizes());
            for (size_t var_index = 0; var_index < ds->rank(); var_index++) {
                // exempt radar and lightning from this
                if (var_index == cband_radolan_rx || var_index == linet_oase_tl) continue;
                MultiArray<T> *data = ds->get_data(var_index);
                MultiArray<T> *result = new MultiArrayBlitz<T>(data->get_dimensions());
                result->copy_from(data);
#if WITH_OPENMP
#pragma omp parallel for schedule(dynamic)
#endif
                for (size_t i = 0; i < mapping.size(); i++) {
                    vector<int> gridpoint = mapping.linear_to_grid(i);
                    vector<T> values;
#if WITH_OPENMP
#pragma omp critical
                    {
#endif
                    data->values_around(gridpoint, bandwidth, values);
#if WITH_OPENMP
                    }
#endif
                    // sort the data in ascending order
                    std::sort(values.begin(), values.end());
                    // calculate the number of values that make up
                    // the required percentage
                    int num_values = round(values.size() * percentage);
                    // obtain the average of the last num_values values
                    T sum = 0.0;
                    for (int i = 0; i < num_values; i++)
                        sum += values[i];
                    T average = sum / ((T) num_values);
                    // replace the value in the result array
                    // with the average
                    result->set(gridpoint, average);
                }

                // Note: this frees the pointer to the old
                // data, we do not have to take care of it
                ds->set_data(var_index, result);
            }

            boost::filesystem::path ppath(ds->filename());
            std::string fn = "25perc-" + ppath.filename().stem().generic_string() + ".nc";
            ds->save_as(fn);
        }

        void
        calculate_weight_function(FeatureSpace <T> *fs) {

#if WRITE_CI_SCORE
            vector<size_t> dims = m_coordinate_system->get_dimension_sizes();
            m_score_108 = new MultiArrayBlitz<T>(dims, 1000);
            m_score_108_trend = new MultiArrayBlitz<T>(dims, 1000);
            m_score_62_108 = new MultiArrayBlitz<T>(dims, 1000);
            m_score_134_108 = new MultiArrayBlitz<T>(dims, 1000);
            m_62_108_trend = new MultiArrayBlitz<T>(dims, 1000);
            m_134_108_trend = new MultiArrayBlitz<T>(dims, 1000);
#endif
            // compute the weights
            for (size_t i = 0; i < fs->points.size(); i++) {
                Point<T> *p = fs->points[i];
                bool have_overlap = (m_overlap == NULL || m_overlap->get(p->gridpoint) == true);
                if (have_overlap) {
                    T saliency = this->compute_weight(p);
                    m_weight->set(p->gridpoint, saliency);
                }
            }

#if WRITE_CI_SCORE
#if WITH_VTK

            boost::filesystem::path ppath(m_data_store->filename());
            std::string basename = ppath.filename().stem().generic_string();

            std::string fn = "ci-score-" + basename + ".vtk";
            VisitUtils<T>::write_multiarray_vtk(fn, "ci-score", m_coordinate_system, m_weight);

            fn = "score_108-" + basename + ".vtk";
            VisitUtils<T>::write_multiarray_vtk(fn, "10.8 ", m_coordinate_system, m_score_108);

            fn = "score_6.2-10.8-" + basename + ".vtk";
            VisitUtils<T>::write_multiarray_vtk(fn, "6.2-10.8", m_coordinate_system, m_score_62_108);

            fn = "score_13.4-10.8-" + basename + ".vtk";
            VisitUtils<T>::write_multiarray_vtk(fn, "13.4-10.8", m_coordinate_system, m_score_134_108);

            fn = "score_10.8-trend-" + basename + ".vtk";
            VisitUtils<T>::write_multiarray_vtk(fn, "10.8-trend", m_coordinate_system, m_score_108_trend);

            fn = "score_6.2-10.8-trend-" + basename + ".vtk";
            VisitUtils<T>::write_multiarray_vtk(fn, "6.2-10.8-trend", m_coordinate_system, m_62_108_trend);

            fn = "score_13.4-10.8-trend-" + basename + ".vtk";
            VisitUtils<T>::write_multiarray_vtk(fn, "13.4-10.8-trend", m_coordinate_system, m_134_108_trend);

            if (m_overlap != NULL) {
                fn = "overlap-" + basename + ".vtk";
                VisitUtils<T>::write_multiarray_vtk(fn, "overlap", m_coordinate_system, m_overlap);
            }
#endif
            delete m_score_108;
            delete m_score_108_trend;
            delete m_score_62_108;
            delete m_score_134_108;
            delete m_62_108_trend;
            delete m_134_108_trend;
#endif
            // dispose of stuff we do not longer need

            delete this->m_data_store;
            this->m_data_store = NULL;

            delete this->m_index;
            this->m_index = NULL;

            delete this->m_search_params;
            this->m_search_params = NULL;
        };

    public:

        /** Calculates the brightness temperature in degree centigrade
         * from the given seviri count
         * @param one of msevi_l15_ir_108, msevi_l15_wv_062, msevi_l15_ir_134
         * @param radiance value for the given channel
         * @return brightness temperature in [C]
         */
        T brightness_temperature(const size_t var_index, const T &radiance) {
            T wavenum = m_wavenumber[var_index];
            T Tbb = m_c2[var_index] * wavenum / log(1 + wavenum * wavenum * wavenum * m_c1[var_index] / radiance);
            T Tb = (Tbb - m_beta[var_index]) / m_alpha[var_index];
            return Tb - 273.15;
        }

        /** Inverse calculation. Spectral radiance from temperature
         * in degree centigrade
         * @param one of msevi_l15_ir_108, msevi_l15_wv_062, msevi_l15_ir_134
         * @param brightness temperature in [C]
         * @return radiance value for the given channel
         */
        T spectral_radiance(const size_t var_index, const T &temperature) {
            T wavenum = m_wavenumber[var_index];
            T Tbb = (temperature + 273.15) * m_alpha[var_index] + m_beta[var_index];
            return wavenum * wavenum * wavenum * m_c1[var_index] / (exp(m_c2[var_index] * wavenum / Tbb) - 1);
        }

    private:

        /** Calculates the weight at the given point using the
         * the scoring scheme.
         */
        T compute_weight(Point <T> *p) {
            // Silke's suggestion: when radar is present, use max score to
            // make sure objects are tracked.
            T max_score = (m_super_params.ci_comparison_file != NULL) ? 8 : 6;

            // If only satellite data is used, subtract lightning
            // and radar from max score
            if (m_super_params.ci_satellite_only)
                max_score -= 2;

            vector<int> g = p->gridpoint;
            bool isInRange = false;
            bool isValid = false;

            // TODO: validity checks
            T ir_108_radiance = this->m_data_store->get(msevi_l15_ir_108, g, isInRange, isValid);
            T ir_108_temp = brightness_temperature(msevi_l15_ir_108, ir_108_radiance);

#if WRITE_CI_SCORE
            m_score_108->set(g, ir_108_temp);
#endif

            T wv_062_rad = this->m_data_store->get(msevi_l15_wv_062, g, isInRange, isValid);
            T wv_062_temp = brightness_temperature(msevi_l15_wv_062, wv_062_rad);

            T ir_134_rad = this->m_data_store->get(msevi_l15_ir_134, g, isInRange, isValid);
            T ir_134_temp = brightness_temperature(msevi_l15_ir_134, ir_134_rad);

            // Calculate score
            int score = 0;

            // IR 10.7 critical value
            if (ir_108_temp <= 0.0) {
                score++;
            }

            // IR 0.65 - IR 10.7
            T delta_wv_062_ir_108 = wv_062_temp - ir_108_temp;

#if WRITE_CI_SCORE
            m_score_62_108->set(g, delta_wv_062_ir_108);
#endif

            if (m_super_params.ci_use_walker_mecikalski) {
                if (delta_wv_062_ir_108 >= -35.0 && delta_wv_062_ir_108 <= -10.0)
                    score++;
            } else {
                if (delta_wv_062_ir_108 <= 2.0)
                    score++;
            }

            // IR 13.3 - IR 10.7
            T delta_ir_134_ir_108 = ir_134_temp - ir_108_temp;

#if WRITE_CI_SCORE
            m_score_134_108->set(g, delta_ir_134_ir_108);
#endif

            if (m_super_params.ci_use_walker_mecikalski) {
                if (delta_ir_134_ir_108 >= -25.0 && delta_ir_134_ir_108 <= -5.0)
                    score++;
            } else {
                if (-delta_ir_134_ir_108 <= 2.0)
                    score++;
            }

            if (m_super_params.ci_comparison_file != NULL) {
                // WARNING: this assumes the dime difference is 15 mins!!
                // TODO: adapt the calculation for different intervals?

                T ir_108_radiance_prev = this->m_ci_comparison_data_store->get(msevi_l15_ir_108, g, isInRange, isValid);
                T ir_108_temp_prev = brightness_temperature(msevi_l15_ir_108, ir_108_radiance_prev);

                T wv_062_rad_prev = this->m_ci_comparison_data_store->get(msevi_l15_wv_062, g, isInRange, isValid);
                T wv_062_temp_prev = brightness_temperature(msevi_l15_wv_062, wv_062_rad_prev);

                T ir_134_rad_prev = this->m_ci_comparison_data_store->get(msevi_l15_ir_134, g, isInRange, isValid);
                T ir_134_temp_prev = brightness_temperature(msevi_l15_ir_134, ir_134_rad_prev);

                T dT1 = ir_108_temp - ir_108_temp_prev;

                if (m_super_params.ci_use_walker_mecikalski) {
                    if (dT1 <= -4.0) score++;
                } else {
                    if (dT1 <= -2.0) score++;
                }

                T dT2 = (wv_062_temp - ir_108_temp) - (wv_062_temp_prev - ir_108_temp_prev);

                if (m_super_params.ci_use_walker_mecikalski) {
                    if (dT2 >= 3.0) score++;
                } else {
                    if (dT2 >= 1.0) score++;
                }

                T dT3 = (ir_134_temp - ir_108_temp) - (ir_134_temp_prev - ir_108_temp_prev);

                if (m_super_params.ci_use_walker_mecikalski) {
                    if (dT3 > 3.0) score++;
                } else {
                    if (dT3 >= 1.0) score++;
                }

#if WRITE_CI_SCORE
                m_score_108_trend->set(g, dT1);
                m_62_108_trend->set(g, dT2);
                m_134_108_trend->set(g, dT3);
#endif
            }

            if (!m_super_params.ci_satellite_only) {
                // Is radar signature > 25dBZ and/or lightning present in 5km radius?

                bool has_lightning = false;
                bool has_radar = false;

                typename Point<T>::list *neighbors = m_index->search(p->coordinate, m_search_params);

                T cband_rx, linet_count;

                for (size_t pi = 0; pi < neighbors->size() && !(has_lightning || has_radar); pi++) {
                    typename Point<T>::ptr n = neighbors->at(pi);

                    bool neighbour_is_in_range = false;
                    bool neighbour_is_valid = false;

                    if (!has_radar) {
                        cband_rx = this->m_data_store->get(cband_radolan_rx,
                                                           n->gridpoint,
                                                           neighbour_is_in_range,
                                                           neighbour_is_valid);

                        // Start using radar at light rain (>= 25dBZ)
                        has_radar = (neighbour_is_valid && cband_rx >= 25.0 && cband_rx <= 65);
                    }

                    if (!has_radar) {
                        neighbour_is_valid = false;
                        neighbour_is_in_range = false;

                        linet_count = this->m_data_store->get(linet_oase_tl,
                                                              n->gridpoint,
                                                              neighbour_is_in_range,
                                                              neighbour_is_valid);

                        has_lightning = (neighbour_is_valid && linet_count > 0.0);
                    }
                }

                delete neighbors;

                // If any lightning is present in 5km radius:
                // increase score

                if (has_lightning) {
                    score++;
                }

                // If light rain or more is present in 5km radius:
                // increase score to max

                if (has_radar) {
                    score = max_score;
                }
            }

            return score;
        }

        T operator()(const typename Point<T>::ptr p) const {
            return m_weight->get(p->gridpoint);
        }

    };
}

#endif
