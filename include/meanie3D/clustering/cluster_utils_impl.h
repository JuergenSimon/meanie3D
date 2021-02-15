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

#ifndef M3D_CLUSTERUTILS_IMPL_H
#define M3D_CLUSTERUTILS_IMPL_H

#include <meanie3D/defines.h>
#include <meanie3D/namespaces.h>
#include <meanie3D/utils.h>

#include <boost/progress.hpp>

namespace m3D
{

    template <typename T>
    ClusterUtils<T>::ClusterUtils(float merge_threshold)
        : m_merge_threshold(merge_threshold)
    {
    }

    template <typename T>
    void
    ClusterUtils<T>::filter_with_previous_clusters(typename ClusterList<T>::ptr previous,
                                                   typename ClusterList<T>::ptr current,
                                                   CoordinateSystem<T> *coord_system,
                                                   WeightFunction<T> *weight_function,
                                                   const Verbosity verbosity)
    {
        using utils::SimpleMatrix;
        using namespace utils::vectors;

        if (verbosity >= VerbosityNormal)
        {
            cout << endl << "Filtering with previous results ..." << endl;
            start_timer();
        }

        // sanity check
        if (current->clusters.size() == 0 || previous->clusters.size() == 0)
        {
            if (verbosity >= VerbosityNormal)
            {
                cout << "Nothing to do." << endl;
                return;
            }
        }

        // Provide index for overlap calculations
        ClusterIndex<T> index_of_previous(previous->clusters, coord_system->get_dimension_sizes());
        size_t new_count = current->clusters.size();
        size_t old_count = previous->clusters.size();
        id_t current_id = current->clusters[current->clusters.size() - 1]->id;

        typename SimpleMatrix<T>::matrix_t coverOldByNew = SimpleMatrix<T>::create_matrix(new_count, old_count);
        typename SimpleMatrix<T>::matrix_t coverNewByOld = SimpleMatrix<T>::create_matrix(new_count, old_count);

        // compute overlap
        boost::progress_display *progress = NULL;
        if (verbosity >= VerbosityNormal)
        {
            progress = new boost::progress_display(2 * old_count);
        }
        size_t n, m;
        for (m = 0; m < old_count; m++)
        {
            typename Cluster<T>::ptr oldCluster = previous->clusters[m];
            if (verbosity >= VerbosityNormal)
            {
                progress->operator++();
            }
            for (n = 0; n < new_count; n++)
            {
                typename Cluster<T>::ptr newCluster = current->clusters[n];
                // Calculate overlap
                T overlap = index_of_previous.occupation_ratio(newCluster, oldCluster);
                if (overlap > 0 && verbosity >= VerbosityDetails)
                {
                    printf("old #%4lu with new #%4lu overlap = %3.2f\n", oldCluster->id, newCluster->id, overlap);
                }
                coverNewByOld[n][m] = overlap;
            }
        }

        // for each new blob check coverage of old blobs
        typedef set<typename Cluster<T>::ptr> cset_t;
        cset_t erased, merged;
        for (m = 0; m < previous->clusters.size(); m++)
        {
            if (verbosity >= VerbosityNormal)
            {
                progress->operator++();
            }

            typename Cluster<T>::ptr old_cluster = previous->clusters[m];
            vector<size_t> candidates;

            // figure out the largest candidate
            // TODO: do we still need this here?
            for (n = 0; n < current->clusters.size(); n++)
            {
                T overlap = coverNewByOld[n][m];
                if (overlap >= 0.33)
                {
                    candidates.push_back(n);
                }
            }

            if (candidates.size() > 1)
            {
                if (verbosity >= VerbosityDetails)
                {
                    printf("Old cluster ID#%4lu seems to have split into new clusters IDs ", old_cluster->id);
                    for (int i = 0; i < candidates.size(); i++)
                    {
                        typename Cluster<T>::ptr c = current->clusters[candidates[i]];
                        printf("#%lu ", c->id);
                    }
                    cout << endl;
                }
                typename Cluster<T>::ptr merged_cluster = new Cluster<T>(old_cluster->mode, old_cluster->spatial_rank());
                merged_cluster->id = ++current_id;
                vector<T> mode(previous->rank(), 0.0);

                // merge with those candidates, that are direct neighbours. Neglect those that have 
                // no direct boundary with any of the other candidates
                size_t num_picked_candidates = 0;
                for (int i = 0; i < candidates.size(); i++)
                {
                    bool have_boundary = true;
                    if (have_boundary)
                    {
                        typename Cluster<T>::ptr c = current->clusters[candidates[i]];
                        merged_cluster->add_points(c->get_points());
                        mode += c->mode;
                        erased.insert(c);
                        num_picked_candidates++;
                    }
                }
                if (num_picked_candidates > 1)
                {
                    merged_cluster->mode = mode / ((T)num_picked_candidates);
                    merged.insert(merged_cluster);
                }
                else
                {
                    delete merged_cluster;
                }
            }
        }

        // remove the erased ones and add the merged ones
        typename cset_t::iterator ci;
        for (ci = erased.begin(); ci != erased.end(); ++ci)
        {
            typename Cluster<T>::ptr c = *ci;
            typename Cluster<T>::list::iterator di = find(current->clusters.begin(), current->clusters.end(), c);
            current->clusters.erase(di);
            delete c;
        }
        for (ci = merged.begin(); ci != merged.end(); ci++)
        {
            typename Cluster<T>::ptr c = *ci;
            current->clusters.push_back(c);
        }
        if (verbosity >= VerbosityNormal)
        {
            cout << " done. (Found " << current->clusters.size() << " clusters in " << stop_timer() << " seconds)" << endl;
            delete progress;
        }
    }

    template <typename T>
    void
    ClusterUtils<T>::replace_points_from_datastore(typename ClusterList<T>::ptr list,
                                                   typename DataStore<T>::ptr dataStore)
    {
        for (size_t ci = 0; ci < list->size(); ci++)
        {
            typename Cluster<T>::ptr c = list->clusters.at(ci);
            typename Point<T>::list::iterator pi;
            for (pi = c->get_points().begin(); pi != c->get_points().end(); ++pi)
            {
                typename Point<T>::ptr p = *pi;
                for (int vi = 0; vi < dataStore->rank(); vi++)
                {
                    bool isInRange = false;
                    bool isValid = false;
                    T value = dataStore->get(vi, p->gridpoint, isInRange, isValid);
                    p->values[c->spatial_rank() + vi] = value;
                }
            }
        }
    }

    template <typename T>
    void
    ClusterUtils<T>::obtain_margin_flag(typename ClusterList<T>::ptr list,
                                        typename FeatureSpace<T>::ptr fs)
    {
        vector<size_t> dims = fs->coordinate_system->get_dimension_sizes();
        ArrayIndex<T> index(dims, fs->points, false);
        typename Cluster<T>::list::iterator ci;
        for (ci = list->clusters.begin(); ci != list->clusters.end(); ++ci)
        {
            typename Cluster<T>::ptr c = *ci;
            typename Point<T>::list::iterator pi;
            for (pi = c->get_points().begin(); pi != c->get_points().end() && !c->has_margin_points(); ++pi)
            {
                typename Point<T>::ptr p = *pi;
                if (!p->isOriginalPoint)
                    continue;
                typename Point<T>::list neighbors;
                neighbors = index.find_neighbours(p->gridpoint, 1);
                typename Point<T>::list::iterator ni;
                for (ni = neighbors.begin(); ni != neighbors.end(); ++ni)
                {
                    typename Point<T>::ptr n = *ni;
                    if (fs->off_limits()->get(n->gridpoint))
                    {
                        c->set_has_margin_points(true);
                        break;
                    }
                }
            }
        }
    }

    template <typename T>
    void
    ClusterUtils<T>::provideUuids(typename ClusterList<T>::ptr list, uuid_t &uuid)
    {
        typename Cluster<T>::list::iterator ci;
        for (ci = list->clusters.begin(); ci != list->clusters.end(); ++ci)
        {
            typename Cluster<T>::ptr c = *ci;
            uuid = m3D::nextUuid(uuid);
            c->uuid = uuid;
        }
    }

    template <typename T>
    void
    ClusterUtils<T>::provideIds(typename ClusterList<T>::ptr list, id_t &id)
    {
        typename Cluster<T>::list::iterator ci;
        for (ci = list->clusters.begin(); ci != list->clusters.end(); ++ci)
        {
            typename Cluster<T>::ptr c = *ci;
            id = m3D::nextId(id);
            c->id = id;
        }
    }
} // namespace m3D

#endif
