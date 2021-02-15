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

#ifndef M3D_OPERATION_CLUSTER_IMPL_H
#define M3D_OPERATION_CLUSTER_IMPL_H

#include <meanie3D/defines.h>
#include <meanie3D/namespaces.h>

#include <meanie3D/parallel.h>
#include <meanie3D/utils.h>
#include "detection.h"

#include <vector>

namespace m3D
{

#if WRITE_MODES
    static size_t s_pass_counter = 0;

    template <typename T> size_t ClusterOperation<T>::pass_counter() {
        return s_pass_counter;
    };

    template <typename T> void ClusterOperation<T>::reset_pass_counter() {
        s_pass_counter = 0;
    };

    template <typename T> void ClusterOperation<T>::increment_pass_counter() {
        s_pass_counter++;
    }
#endif

#pragma mark -
#pragma mark Clustering Code

    template <typename T> ClusterList<T>* ClusterOperation<T>::cluster() {
        using namespace m3D::utils::vectors;
        const CoordinateSystem<T> *cs = m_context.coord_system;
        vector<T> resolution;
        if (m_context.search_params->search_type() == SearchTypeRange) {
            RangeSearchParams<T> *p = (RangeSearchParams<T> *)m_context.search_params;
            // Physical grid resolution in the
            // spatial range
            resolution = cs->resolution();
            resolution = ((T)4.0) * resolution;
            // Supplement with bandwidth values for
            // the value range
            for (size_t i = resolution.size(); i < p->bandwidth.size(); i++) {
                resolution.push_back(p->bandwidth[i]);
            }
        } else {
            KNNSearchParams<T> *p = (KNNSearchParams<T> *)m_context.search_params;
            resolution = p->resolution;
        }

        if (m_context.show_progress) {
            cout << endl << "Creating meanshift vector graph ...";
            start_timer();
            m_progress_bar = new boost::progress_display(this->feature_space->size());
        }

        // Create an empty cluster list
        ClusterList<T> *cluster_list = new ClusterList<T>(
            m_params.filename,
            m_params.variables,
            m_params.dimensions,
            m_params.dimension_variables,
            m_params.time_index);

        // Guard against empty feature-space
        if (this->feature_space->points.size() == 0) {
            cout << "Feature space is empty" << endl;
            return cluster_list;
        }

        MeanshiftOperation<T> meanshiftOperator(this->feature_space, this->point_index);
        meanshiftOperator.prime_index(m_context.search_params);

#if WITH_OPENMP
#pragma omp parallel for schedule(dynamic)
#endif
        for (size_t index = 0; index < this->feature_space->size(); index++) {
            if (m_context.show_progress) {
#if WITH_OPENMP
#pragma omp critical
#endif
                m_progress_bar->operator++();
            }

            // Get the meanshift vector for this point
            typename Point<T>::ptr x = this->feature_space->points[index];

            // If the weight function indicates no significant response, skip
            // this point
            // TODO: what happened to this code?


            // Calculate the mean shift vector
            x->shift = meanshiftOperator.meanshift(x->values,
                                                   m_context.search_params,
                                                   m_context.kernel,
                                                   m_context.weight_function);

            // Extract the spatial component and obtain the grid
            vector<T> spatial_shift = this->feature_space->spatial_component(x->shift);
            x->gridded_shift = this->feature_space->coordinate_system->to_gridpoints(spatial_shift);
        }

        if (m_context.show_progress) {
            cout << "done. (" << stop_timer() << "s)" << endl;
            delete m_progress_bar;
            m_progress_bar = NULL;
        }

        // Analyse the graph and create clusters
        cluster_list->aggregate_cluster_graph(
            this->feature_space,
            m_context.weight_function,
            m_params.coalesceWithStrongestNeighbour,
            m_context.show_progress);

        // Provide fresh ids right away
        m3D::uuid_t uuid = 0;
        ClusterUtils<T>::provideUuids(cluster_list, uuid);
        m3D::id_t id = 0;
        ClusterUtils<T>::provideIds(cluster_list, id);

        //        cout << "Cluster list after aggregation:" << endl;
        //        cluster_list.print();

        // Replace points with original data ()
        ClusterUtils<T>::replace_points_from_datastore(cluster_list, m_context.data_store);
        //        cout << "Cluster list after filtering points:" << endl;
        //        cluster_list.print();

        // Find margin points (#325)
        ClusterUtils<T>::obtain_margin_flag(cluster_list, this->feature_space);

#if WRITE_BOUNDARIES
        cluster_list.write_boundaries(weight_function, this->feature_space, this->point_index, resolution);
#endif

#if WRITE_MODES
#if WITH_VTK
        size_t min_size = std::numeric_limits<size_t>::max();
        size_t max_size = std::numeric_limits<size_t>::min();
        for (size_t i = 0; i < cluster_list.size(); i++)
        {
            if (cluster_list[i]->size() < min_size)
            {
                min_size = cluster_list[i]->size();
            }
            if (cluster_list[i]->size() > max_size)
            {
                max_size = cluster_list[i]->size();
            }
        }

        NetCDFDataStore<T> *ds = (NetCDFDataStore<T> *)this->feature_space->data_store();
        std::string fn = ds->filename() + "-modes-" + boost::lexical_cast<string>(pass_counter()) + ".vtk";
        VisitUtils<T>::write_cluster_modes_vtk(fn, cluster_list.clusters);

        fn = ds->filename() + "-raw-modes-" + boost::lexical_cast<string>(pass_counter()) + ".vtk";
        VisitUtils<T>::write_modes_vtk(fn, cluster_list.trajectory_endpoints(), cluster_list.trajectory_lengths());
        VisitUtils<T>::write_cluster_modes_vtk(fn, cluster_list.clusters);
#endif
#endif
        return cluster_list;
    }
} // namespace m3D

#endif
