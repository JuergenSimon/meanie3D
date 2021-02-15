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


#ifndef M3D_CLUSTERLIST_IMPL_H
#define M3D_CLUSTERLIST_IMPL_H

#include <meanie3D/defines.h>
#include <meanie3D/namespaces.h>
#include <meanie3D/clustering/cluster.h>
#include <meanie3D/utils/set_utils.h>

#include <algorithm>
#include <sstream>
#include <stdlib.h>
#include <netcdf>
#include <vector>
#include <set>
#include <stdexcept>

#include <boost/tokenizer.hpp>
#include <boost/algorithm/string/predicate.hpp>

#include "cluster_list.h"

namespace m3D {

    using namespace std;
    using namespace netCDF;
    using namespace utils;
#if WITH_VTK
    using utils::VisitUtils;
#endif

#pragma mark -
#pragma mark Macros

#define sup(v1, v2) (v1 > v2 ? v1:v2)
#define inf(v1, v2) (v1 < v2 ? v1:v2)

#pragma mark -
#pragma mark Constructors/Destructors et. Al.

    template<typename T>
    ClusterList<T>::ClusterList()
            : file(NULL), tracking_performed(false), highest_id(0), highest_uuid(0) {
    };

    template<typename T>
    ClusterList<T>::ClusterList(const string &source,
                                const vector<string> &variables,
                                const vector<string> &dimensions,
                                const vector<string> &dimension_variables,
                                long timestamp,
                                int ti,
                                bool orig_pts)
            : file(NULL), tracking_performed(false), highest_id(0), highest_uuid(0), variables(variables),
              dimensions(dimensions), dimension_variables(dimension_variables), source_file(source), time_index(ti),
              timestamp(timestamp), m_use_original_points_only(orig_pts) {};

    template<typename T>
    ClusterList<T>::ClusterList(
            const typename Cluster<T>::list &list,
            const string &source,
            const vector<string> &vars,
            const vector<string> &dims,
            const vector<string> &dim_vars,
            long timestamp,
            int ti,
            bool orig_pts)
            : file(NULL), tracking_performed(false), highest_id(0), highest_uuid(0), variables(vars), dimensions(dims),
              dimension_variables(dim_vars), source_file(source), time_index(ti), timestamp(timestamp),
              m_use_original_points_only(orig_pts), clusters(list) {};

    template<typename T>
    ClusterList<T>::ClusterList(const ClusterList &o)
            : file(o.file), filename(o.filename), variables(o.variables), dimensions(dimensions),
              dimension_variables(o.dimension_variables), source_file(o.source_file), clusters(o.clusters),
              tracking_performed(o.tracking_performed), tracking_time_difference(o.tracking_time_difference),
              tracked_ids(o.tracked_ids), dropped_ids(o.dropped_ids), new_ids(o.new_ids), splits(o.splits),
              merges(o.merges), highest_id(o.highest_id), highest_uuid(o.highest_uuid), timestamp(o.timestamp),
              time_index(o.time_index), m_use_original_points_only(o.m_use_original_points_only) {};


#pragma mark -
#pragma mark Accessing the list

    template<typename T>
    size_t
    ClusterList<T>::size() const {
        return clusters.size();
    }

    template<typename T>
    typename Cluster<T>::ptr
    ClusterList<T>::operator[](size_t index) {
        return clusters.at(index);
    }

    template<typename T>
    void
    ClusterList<T>::clear(bool deletion_flag) {
        typename Cluster<T>::list::const_iterator ci;
        for (ci = clusters.begin(); ci != clusters.end(); ++ci) {
            typename Cluster<T>::ptr c = *ci;
            c->clear(deletion_flag);
            delete c;
        }
        clusters.clear();
    }

#pragma mark -
#pragma mark Adding / Removing points

    template<typename T>
    void
    ClusterList<T>::apply_size_threshold(unsigned int min_cluster_size, const bool &show_progress) {
        boost::progress_display *progress = NULL;
        if (show_progress) {
            cout << endl << "Applying size threshold of " << min_cluster_size << " ... ";
            start_timer();
            progress = new boost::progress_display(clusters.size());
        }
        size_t axe_count = 0;
        if (min_cluster_size > 1) {
            typename Cluster<T>::list::iterator it;
            for (it = clusters.begin(); it != clusters.end();) {
                progress->operator++();

                typename Cluster<T>::ptr sc = *it;
                if (sc->size() < min_cluster_size) {
                    it = clusters.erase(it);
                    axe_count++;
                } else {
                    it++;
                }
            }
        }
        if (show_progress) {
            cout << "done. (Removed " << axe_count << " objects in " << stop_timer() << "s)" << endl;
            delete progress;
        }
    }

#pragma mark -
#pragma mark Writing/Reading

    template<typename T>
    void
    ClusterList<T>::save() {
        if (this->filename.empty()) {
            throw std::runtime_error("Can not use save() because cluster list was not written or read before.");
        }
        this->write(this->filename);
    }

    template<typename T>
    void
    ClusterList<T>::write(const std::string &path) {
        using namespace utils::vectors;

        try {
            NcFile *file = NULL;
            this->filename = std::string(path);

            bool file_existed = boost::filesystem::exists(path);
            std::string filename = file_existed ? path + "-new" : path;

            // Make sure the new file is deleted if it exists.
            // This can happen if a previous run didn't fully complete.
            if (boost::filesystem::exists(filename)) {
                boost::filesystem::remove(filename);
            }

            // We need to get the size of the dimensions and other
            // data from somewhere. For this we have to rely on either
            // the source being present, or a previous instance of the
            // cluster file (in case of overwriting).
            string source_path = file_existed ? path : source_file;
            NcFile *sourcefile = new NcFile(source_path, NcFile::read);
            if (sourcefile == NULL || sourcefile->isNull()) {
                cerr << "FATAL:could not open file '" << source_path
                     << "' for obtaining dimension data" << endl;
                exit(EXIT_FAILURE);
            }

            try {
                // Be aware of the fact that this->ncFile is probably
                // open from the tracking at this point. It also needs
                // to be open, because the dimensions etc. are referencing
                // it. This creates a paradoxical situation, which is solved
                // by creating a new file with an altered name first, writing
                // it off and finally deleting the original when done, replacing
                // the original in that way.
                file = new NcFile(filename, NcFile::replace);
            } catch (const netCDF::exceptions::NcException &e) {
                cerr << "FATAL:exception opening file " << filename
                     << " for writing : " << e.what() << endl;
                exit(EXIT_FAILURE);
            }

            // write version attribute
            file->putAtt("version", m3D::VERSION);

            // Create feature-space variables
            vector<string> featurespace_variables = dimension_variables;
            for (size_t i = 0; i < variables.size(); i++) {
                featurespace_variables.push_back(variables[i]);
            }

            // This is one dimension of the clusters and also the rank
            // (spatial rank + value rank) of the featurespace
            NcDim dim = file->addDim("rank", (int) featurespace_variables.size());

            // Record the individual ranks as well
            file->putAtt("spatial_rank", ncInt, (int) dimensions.size());
            file->putAtt("value_rank", ncInt, (int) variables.size());

            // General dimension/variable info
            file->putAtt("variables", to_string(variables));
            file->putAtt("dimensions", to_string(dimensions));
            file->putAtt("dimension_variables", to_string(dimension_variables));

            // The actual variables composing the featurespace
            file->putAtt("featurespace_variables", to_string(featurespace_variables));

            // Add 'time' information
            netcdf::add_time(file, this->timestamp, true);
            file->putAtt("time_index", ncInt, time_index);

            // copy dimensions
            vector<NcDim> ncDimensions
                    = netcdf::copy_dimensions(dimensions, sourcefile, file);

            // Create dummy variables, attributes and other meta-info
            file->putAtt("num_clusters", ncInt, (int) clusters.size());
            file->putAtt("source", this->source_file);

            // Save highest ID
            unsigned long long hid = boost::numeric_cast<unsigned long long>(this->highest_id);
            file->putAtt("highest_id", boost::lexical_cast<std::string>(hid));

            // Save highest UUID
            unsigned long long huuid = boost::numeric_cast<unsigned long long>(this->highest_uuid);
            file->putAtt("highest_uuid", boost::lexical_cast<std::string>(huuid));

            // Record IDs in attribute
            id_set_t cluster_ids;
            for (size_t ci = 0; ci < clusters.size(); ci++)
                cluster_ids.insert(clusters[ci]->id);
            file->putAtt("cluster_ids", sets::to_string(cluster_ids));

            // Add tracking meta-info
            if (this->tracking_performed) {
                file->putAtt("tracking_performed", "yes");
                file->putAtt("tracking_time_difference", ncInt, this->tracking_time_difference);
                file->putAtt("tracked_ids", sets::to_string(this->tracked_ids));
                file->putAtt("new_ids", sets::to_string(this->new_ids));
                file->putAtt("dropped_ids", sets::to_string(this->dropped_ids));
                file->putAtt("merges", maps::id_map_to_string(this->merges));
                file->putAtt("splits", maps::id_map_to_string(this->splits));
            }

            // Copy dimension variables including data. This is required
            // so that on reading a coordinate system can be constructed

            for (size_t i = 0; i < dimension_variables.size(); i++) {
                string var = dimension_variables[i];
                netcdf::copy_variable<T>(var, sourcefile, file, true);
            }

            // Copy other variables without data

            for (size_t i = 0; i < variables.size(); i++) {
                string var = variables[i];
                netcdf::copy_variable<T>(var, sourcefile, file, false);
            }

            // Featurespace Variables
            std::string fvarnames = to_string(variables);

            // Add cluster dimensions and variables
            for (size_t ci = 0; ci < clusters.size(); ci++) {
                typename Cluster<T>::ptr cluster = clusters.at(ci);

                // NOTE: some problem exists with the normal id_t used
                // everywhere else and NetCDF. Using unsigned long long
                // produces a compiler warning but also correct results.
                unsigned long long cid = (unsigned long long) cluster->id;
                unsigned long long uuid = (unsigned long long) cluster->uuid;

                // Create a dimension
                stringstream dim_name(stringstream::in | stringstream::out);
                dim_name << "cluster_dim_" << cid;
                NcDim cluster_dim;
                try {
                    cluster_dim = file->addDim(dim_name.str(), cluster->size());
                } catch (const netCDF::exceptions::NcException &e) {
                    cerr << "ERROR:exception creating dimension " << dim_name.str()
                         << ":" << e.what() << endl;
                    exit(EXIT_FAILURE);
                }

                // Create variable
                stringstream var_name(stringstream::in | stringstream::out);
                var_name << "cluster_" << cid;

                vector<NcDim> dims(2);
                dims[0] = cluster_dim;
                dims[1] = dim;

                NcVar var;
                try {
                    var = file->addVar(var_name.str(), ncDouble, dims);
                    var.setCompression(false, true, 3);
                } catch (const netCDF::exceptions::NcException &e) {
                    cerr << "ERROR:exception creating dimension " << var_name.str()
                         << ":" << e.what() << endl;
                    continue;
                }

                // size
                var.putAtt("size", ncInt, (int) cluster->size());

                // margin flag
                std::string flag = (cluster->has_margin_points() ? "Y" : "N");
                var.putAtt("has_margin_points", flag);

                // check if there's any merge
                id_map_t::iterator mi = this->merges.find(cluster->id);

                if (mi != this->merges.end()) {
                    std::string merged_from = utils::sets::to_string(mi->second);
                    var.putAtt("merged_from", merged_from);
                }

                // check if there's any split
                for (mi = this->splits.begin(); mi != this->splits.end(); mi++) {
                    id_set_t csplits = mi->second;
                    if (csplits.find(cluster->id) != csplits.end()) {
                        std::string split_from = boost::lexical_cast<string>(mi->first);
                        var.putAtt("split_from", split_from);
                        break;
                    }
                }

                // id
                var.putAtt("id", boost::lexical_cast<string>(cid));

                // uuid
                var.putAtt("uuid", boost::lexical_cast<string>(uuid));

                // mode
                string mode = to_string(cluster->mode);
                var.putAtt("mode", mode);

                // displacement
                string displacement = to_string(cluster->displacement);
                var.putAtt("displacement", displacement);

                // bounding box min
                string bound_min = to_string(cluster->get_bounding_box_min());
                var.putAtt("bounding_box_min", bound_min);

                // bounding box max
                string bound_max = to_string(cluster->get_bounding_box_max());
                var.putAtt("bounding_box_max", bound_max);

                // Write cluster away

                size_t numElements = cluster->size() * cluster->rank();
                T *data = (T *) malloc(sizeof(T) * numElements);
                if (data == NULL) {
                    cerr << "FATAL:out of memory" << endl;
                    exit(EXIT_FAILURE);
                }

                for (size_t pi = 0; pi < cluster->size(); pi++) {
                    typename Point<T>::ptr p = cluster->at(pi);
                    for (size_t di = 0; di < dim.getSize(); di++) {
                        size_t index = pi * cluster->rank() + di;
                        data[index] = p->values.at(di);
                    }
                }
                var.putVar(data);
                delete data;
            }

            if (file_existed) {
                // close the original and delete it
                if (this->file != NULL) {
                    delete this->file;
                    this->file = NULL;
                }

                if (boost::filesystem::remove(path)) {
                    boost::system::error_code ec;
                    boost::filesystem::rename(path + "-new", path, ec);
                    if (ec.value() != boost::system::errc::success) {
                        cerr << "ERROR: could not rename " << (path + "-new")
                             << " to " << path << ":" << ec.message() << endl;
                    }
                } else {
                    // for some reason, the old file could not be removed. In
                    // this case, just move it aside and try again
                    cerr << "ERROR: could not delete " << path << endl;

                    std::string moved_path = path + "-moved";
                    cerr << "renaming " << path << " to " << moved_path << endl;
                    boost::system::error_code ec;
                    boost::filesystem::rename(path, moved_path, ec);

                    if (ec.value() == boost::system::errc::success) {
                        boost::filesystem::rename(path + "-new", path, ec);
                        if (ec.value() != boost::system::errc::success) {
                            cerr << "ERROR: could not rename " << (path + "-new") << " to " << path << endl;
                            cerr << "REASON: " << ec.message() << endl;
                        }
                    } else {
                        cerr << "ERROR: could not move " << path << " to " << moved_path << endl;
                        cerr << "REASON: " << ec.message() << endl;
                    }
                }
            }

            this->file = file;

        } catch (const std::exception &e) {
            std::cerr << "ERROR:exception while writing cluster file: " << e.what() << endl;
            throw e;
        }
    }

    template<typename T>
    typename ClusterList<T>::ptr
    ClusterList<T>::read(const std::string &path, CoordinateSystem<T> **cs_ptr) {
        // meta-info
        vector<string> variables;
        vector<string> dimensions;
        vector<string> dimension_variables;
        vector<string> featurespace_variables;
        string source_file;
        id_set_t cluster_ids;
        bool tracking_performed = false;
        id_set_t tracked_ids;
        id_set_t new_ids;
        id_set_t dropped_ids;
        id_map_t merges;
        id_map_t splits;
        int tracking_time_difference = NO_TIME;
        int time_index = NO_TIME;
        timestamp_t timestamp = 0;
        m3D::id_t highest_id = NO_ID;
        m3D::uuid_t highest_uuid = NO_UUID;
        typename Cluster<T>::list list;
        NcFile *file = NULL;

        file = new NcFile(path, NcFile::read);
        try {
            // Read the dimensions
            NcDim fs_dim = file->getDim("rank");

            // variables
            string buffer;
            file->getAtt("variables").getValues(buffer);
            variables = vectors::from_string<string>(buffer);

            // dimensions
            file->getAtt("dimensions").getValues(buffer);
            dimensions = vectors::from_string<string>(buffer);

            // dimension variables
            file->getAtt("dimension_variables").getValues(buffer);
            dimension_variables = vectors::from_string<string>(buffer);

            // featurespace variables
            file->getAtt("featurespace_variables").getValues(buffer);
            featurespace_variables = vectors::from_string<string>(buffer);

            // Read time index
            file->getAtt("time_index").getValues(&time_index);

            // Read time
            timestamp = netcdf::get_time_checked<timestamp_t>(path, 0);

            // Source file
            file->getAtt("source").getValues(source_file);

            int number_of_clusters;
            file->getAtt("num_clusters").getValues(&number_of_clusters);

            std::string value;
            file->getAtt("cluster_ids").getValues(value);
            cluster_ids = sets::from_string<m3D::id_t>(value);

            // Tracking-related
            try {
                NcGroupAtt tracked = file->getAtt("tracking_performed");

                if (!tracked.isNull()) {
                    tracked.getValues(value);
                    tracking_performed = (value == "yes");
                }

                if (tracking_performed) {
                    file->getAtt("tracked_ids").getValues(value);
                    tracked_ids = sets::from_string<id_t>(value);

                    file->getAtt("new_ids").getValues(value);
                    new_ids = sets::from_string<id_t>(value);

                    file->getAtt("dropped_ids").getValues(value);
                    dropped_ids = sets::from_string<id_t>(value);

                    file->getAtt("merges").getValues(value);
                    merges = utils::maps::id_map_from_string(value);

                    file->getAtt("splits").getValues(value);
                    splits = utils::maps::id_map_from_string(value);

                    file->getAtt("highest_id").getValues(value);
                    highest_id = boost::lexical_cast<m3D::id_t>(value);

                    file->getAtt("tracking_time_difference").getValues(&tracking_time_difference);
                }

                file->getAtt("highest_uuid").getValues(value);
                highest_uuid = boost::lexical_cast<m3D::uuid_t>(value);

            } catch (netCDF::exceptions::NcException &e) {
            }

            // Read the feature-variables

            file->getAtt("featurespace_variables").getValues(value);
            featurespace_variables = vectors::from_string<string>(value);

            // Coordinate system wanted?
            CoordinateSystem<T> *cs = new CoordinateSystem<T>(file, dimensions, dimension_variables);
            if (cs_ptr != NULL) {
                *cs_ptr = cs;
            }

            // Read clusters one by one
            id_set_t::iterator cid_iter;

            for (cid_iter = cluster_ids.begin(); cid_iter != cluster_ids.end(); cid_iter++) {
                // Identifier
                m3D::id_t cid = *cid_iter;

                // cluster dimension
                stringstream dim_name(stringstream::in | stringstream::out);
                dim_name << "cluster_dim_" << cid;
                NcDim cluster_dim = file->getDim(dim_name.str().c_str());
                size_t cluster_size = cluster_dim.getSize();

                // Read the variable
                stringstream var_name(stringstream::in | stringstream::out);
                var_name << "cluster_" << cid;
                NcVar var = file->getVar(var_name.str().c_str());

                // mode
                std::string mode_str;
                var.getAtt("mode").getValues(mode_str);
                vector<T> mode = vectors::from_string<T>(mode_str);

                var.getAtt("uuid").getValues(value);
                m3D::uuid_t uuid = boost::lexical_cast<m3D::uuid_t>(value);

                // displacement
                std::string displacement_str;
                var.getAtt("displacement").getValues(displacement_str);
                vector<T> displacement = vectors::from_string<T>(displacement_str);

                std::string bounds_min_str;
                var.getAtt("bounding_box_min").getValues(bounds_min_str);
                vector<T> bounds_min = vectors::from_string<T>(bounds_min_str);

                std::string bounds_max_str;
                var.getAtt("bounding_box_max").getValues(bounds_max_str);
                vector<T> bounds_max = vectors::from_string<T>(bounds_max_str);

                // margin flag
                std::string margin_char;
                var.getAtt("has_margin_points").getValues(margin_char);
                bool margin_flag = margin_char == "Y";

                // Create a cluster object
                typename Cluster<T>::ptr cluster = new Cluster<T>(mode, dimensions.size());
                cluster->id = cid;
                cluster->uuid = uuid;
                cluster->mode = mode;
                cluster->displacement = displacement;
                cluster->set_bounding_box_min(bounds_min);
                cluster->set_bounding_box_max(bounds_max);
                cluster->set_has_margin_points(margin_flag);

                // Read the cluster
                size_t numElements = cluster_size * cluster->rank();
                T *data = (T *) malloc(sizeof(T) * numElements);
                if (data == NULL) {
                    cerr << "FATAL:out of memory" << endl;
                    exit(EXIT_FAILURE);
                }

                var.getVar(data);
                for (size_t pi = 0; pi < cluster_size; pi++) {
                    vector<T> values(cluster->rank(), 0.0);

                    // copy point from data
                    for (size_t di = 0; di < cluster->rank(); di++) {
                        values[di] = data[pi * cluster->rank() + di];
                    }

                    // get coordinate subvector
                    vector<T> coordinate(values.begin(), values.begin() + cs->rank());

                    // transform to gridpoint
                    try {
                        vector<int> gp(cs->rank(), 0);
                        cs->reverse_lookup(coordinate, gp);

                        // only when this succeeds do we have the complete
                        // set of data for the point
                        typename Point<T>::ptr p = PointFactory<T>::get_instance()->create();
                        p->values = values;
                        p->coordinate = coordinate;
                        p->gridpoint = gp;

                        // add to cluster
                        cluster->add_point(p);
                    } catch (std::out_of_range &e) {
                        cerr << "ERROR:reverse coordinate transformation failed for coordinate=" << coordinate << endl;
                    }
                }

                delete data;
                list.push_back(cluster);
            }

            if (cs_ptr == NULL) {
                delete cs;
            }
        } catch (const std::exception &e) {
            cerr << "ERROR:exception " << e.what() << endl;
            throw e;

        }

        // When reading, use the cluster file itself as source
        // path so that the timestamp can be read. Set the real
        // source path up afterwards.
        ClusterList<T>::ptr cl = new ClusterList(list,
                                                 source_file,
                                                 variables,
                                                 dimensions,
                                                 dimension_variables,
                                                 time_index,
                                                 false);

        cl->timestamp = timestamp;
        cl->highest_id = highest_id;
        cl->highest_uuid = highest_uuid;
        cl->tracking_time_difference = tracking_time_difference;
        if (tracking_performed) {
            cl->tracked_ids = tracked_ids;
            cl->new_ids = new_ids;
            cl->dropped_ids = dropped_ids;
            cl->merges = merges;
            cl->splits = splits;
        }

        cl->filename = path;
        cl->file = file;

        return cl;
    }

    template<typename T>
    bool sortBySize(const typename Cluster<T>::ptr c1, const typename Cluster<T>::ptr c2) {
        return c1->size() < c2->size();
    }

    template<typename T>
    void
    ClusterList<T>::print(bool includePoints) {
        std::sort(clusters.begin(), clusters.end(), sortBySize < T > );

        for (size_t ci = 0; ci < clusters.size(); ci++) {
            typename Cluster<T>::ptr c = clusters[ci];
            c->print(includePoints);
        }
    }

#pragma mark -
#pragma mark Clustering by Graph Theory

    template<typename T>
    T
    ClusterList<T>::weight_function_tendency(typename Point<T>::ptr p,
                                             const WeightFunction<T> *weight_function,
                                             const typename Point<T>::list &neighbours,
                                             ArrayIndex<T> &index) {
        T result = 0;
        if (!neighbours.empty()) {
            T wx = weight_function->operator()(p);
            for (size_t ni = 0; ni < neighbours.size(); ni++) {
                Point<T> *n = neighbours.at(ni);
                if (n == p) continue;
                result += (weight_function->operator()(n) - wx);
            }
        }
        return result;
    }

    template<typename T>
    void
    ClusterList<T>::aggregate_zeroshifts(FeatureSpace<T> *fs,
                                         const WeightFunction<T> *weight_function,
                                         ArrayIndex<T> &index,
                                         bool coalesceWithStrongestNeighbour,
                                         bool show_progress) {
        using namespace utils::vectors;
        boost::progress_display *progress = NULL;

        // find the zero-shift points
        typedef set<typename Point<T>::ptr> pset_t;
        pset_t zeroshifts;

#if WITH_OPENMP
#pragma omp parallel for schedule(dynamic)
#endif
        for (size_t i = 0; i < fs->points.size(); i++) {
            Point<T> *current_point = fs->points[i];

#if REPLACE_ZEROSHIFT_VECTORS

            //
            // #209
            //
            // replace the zero-shift vectors with the average of their neighbours

            typename Point<T>::list neighbours = find_neighbours(current_point->gridpoint, index);

            if (!neighbours.empty()) {
                vector<T> m(current_point->values.size(), 0.0);

                for (size_t ni = 0; ni < neighbours.size(); ni++) {
                    M3DPoint<T> *n = (M3DPoint<T> *) neighbours.at(ni);

                    if (n == current_point) continue;

                    m += n->shift;
                }

                // average, rounded to grid

                m /= ((T) neighbours.size());

                current_point->shift = fs->coordinate_system->round_to_grid(m);

                vector<T> spatial_shift = fs->spatial_component(m);

                current_point->gridded_shift = fs->coordinate_system->rounded_gridpoint(spatial_shift);
            }
#endif
            // If the vector is (still) zero, add to the list of zeroshift points

            if (vector_norm(fs->spatial_component(current_point->shift)) == 0) {
#if WITH_OPENMP
#pragma omp critical
#endif
                zeroshifts.insert(current_point);
            }
        }

        if (show_progress) {
            cout << endl << "Clustering zero-shift areas ...";
            progress = new boost::progress_display(zeroshifts.size());
            start_timer();
        }

        for (typename pset_t::iterator pi = zeroshifts.begin(); pi != zeroshifts.end(); pi++) {
            if (show_progress) {
                progress->operator++();
            }
            Point<T> *current_point = *pi;
            typename Point<T>::list neighbours = index.find_neighbours(current_point->gridpoint);

            for (size_t ni = 0; ni < neighbours.size(); ni++) {
                Point<T> *n = neighbours.at(ni);
                if (n == current_point) continue;

                // only consider neighbours that are zero-shift themselves
                if (vector_norm(fs->spatial_component(n->shift)) == 0) {
                    if (current_point->cluster == NULL && n->cluster == NULL) {
                        // Neither current point nor neighbour have cluster
                        // => create new cluster
                        size_t spatial_dims = fs->coordinate_system->rank();
                        typename Cluster<T>::ptr c = new Cluster<T>(current_point->values, spatial_dims);
                        c->add_point(current_point);
                        c->add_point(n);
                        clusters.push_back(c);
                    } else if (current_point->cluster == NULL && n->cluster != NULL) {
                        // neighbour has cluster
                        // => add current point to neighbour's cluster
                        n->cluster->add_point(current_point);
                    } else if (current_point->cluster != NULL && n->cluster == NULL) {
                        // current point has cluster
                        // => add neighbour to current point's cluster
                        current_point->cluster->add_point(n);
                    } else if ((current_point->cluster != NULL && n->cluster != NULL)
                               && (current_point->cluster != n->cluster)) {
                        // current point's cluster and neighbour's cluster
                        // => merge current point's cluster into neighbour's cluster
                        typename Cluster<T>::ptr c = current_point->cluster;
                        n->cluster->add_points(c->get_points(), false);
                        clusters.erase(find(clusters.begin(), clusters.end(), c));
                        delete c;
                    }
                }
            }
        }

#if ADD_STRONGEST_NEIGHBOUR
        // Find neighbours that are not part of the clusters yet
        // and have a stronger weight function response. Assign those
        // to the zero-shift cluster as 'crystallization' points
        // TODO: if it works, incorporate into above loop to save time

        for (size_t i = 0; i < clusters.size(); i++) {
            typename Cluster<T>::ptr c = clusters.at(i);

            typename Point<T>::list::iterator pi;

            T strongest_response = numeric_limits<T>::min();

            T strongest_own_response = numeric_limits<T>::min();

            typename Point<T>::ptr strongest_point = NULL;

            for (pi = c->points.begin(); pi != c->points.end(); pi++) {
                typename Point<T>::ptr p = *pi;

                // track own response

                T own_response = weight_function->operator()(p);

                if (own_response > strongest_own_response) {
                    strongest_own_response = own_response;
                }

                // Find the neighbour with the strongest response

                typename Point<T>::list neighbours = find_neighbours(p->gridpoint, index);

                for (size_t ni = 0; ni < neighbours.size(); ni++) {
                    M3DPoint<T> *n = (M3DPoint<T> *) neighbours.at(ni);

                    T response = weight_function->operator()(n);

                    if (response > strongest_response) {
                        strongest_response = response;
                        strongest_point = n;
                    }
                }
            }

            if (strongest_response > strongest_own_response && strongest_point != NULL) {
                // found a higher point in the vicinity
                c->add_point(strongest_point);
            }
        }
#endif
        // Assign ID
        if (show_progress) {
            cout << "done (found " << clusters.size() << " zero-shift clusters in " << stop_timer() << "s)." << endl;
            delete progress;
        }
    }

    template<typename T>
    void
    ClusterList<T>::aggregate_cluster_graph(FeatureSpace<T> *fs,
                                            const WeightFunction<T> *weight_function,
                                            bool coalesceWithStrongestNeighbour,
                                            bool show_progress) {
        using namespace utils::vectors;
        // PointIndex<T>::write_index_searches = true;
        boost::progress_display *progress = NULL;
#if DEBUG_GRAPH_AGGREGATION
        for (size_t i = 0; i < fs->points.size(); i++) {
            Point<T> *p = fs->points[i];
            if (p->coordinate.size() != p->gridpoint.size() || p->gridpoint.size() > 5) {
                cerr << "ERROR: bogus point " << p << endl;
            }


        }
#endif
        vector<size_t> dimensions = fs->coordinate_system->get_dimension_sizes();
        ArrayIndex<T> index(dimensions, fs->points, false);
#if DEBUG_GRAPH_AGGREGATION
        for (size_t i = 0; i < fs->points.size(); i++) {
            Point<T> *p = fs->points[i];
            if (p->coordinate.size() != p->gridpoint.size()) {
                cerr << "ERROR: bogus point " << p << endl;
            }
        }
#endif
        this->aggregate_zeroshifts(fs, weight_function, index, coalesceWithStrongestNeighbour, show_progress);

#if DEBUG_GRAPH_AGGREGATION
        for (size_t i = 0; i < fs->points.size(); i++) {
            Point<T> *p = fs->points[i];
            if (p->coordinate.size() != p->gridpoint.size()) {
                cerr << "ERROR: bogus point " << p << endl;
            }
        }
#endif

#if WRITE_ZEROSHIFT_CLUSTERS
        typename Cluster<T>::list::iterator ci;
        size_t id = 0;
        for (ci = clusters.begin(); ci != clusters.end(); ci++) {
            typename Cluster<T>::ptr c = *ci;
            c->id = id++;
        }

        NetCDFDataStore<T> *ds = (NetCDFDataStore<T> *) fs->data_store();
        boost::filesystem::path path(ds->filename());

        std::string basename = path.stem().generic_string() + "-zeroshift";
        VisitUtils<T>::write_clusters_vtu(this, fs->coordinate_system, basename);
#endif
        // Sanity checking
        // this->check_clusters(fs,index);
        size_t cluster_id = this->clusters.size();
        if (show_progress) {
            cout << endl << "Analysing meanshift vector graph ...";
            start_timer();
            progress = new boost::progress_display(fs->points.size());
        }

        //        #if WITH_OPENMP
        //        #pragma omp parallel for schedule(dynamic)
        //        #endif
        for (size_t i = 0; i < fs->points.size(); i++) {
            if (show_progress) {
                //                #if WITH_OPENMP
                //                #pragma omp critical
                //                #endif
                progress->operator++();
            }

            Point<T> *current_point = fs->points[i];
            // skip zeroshift and non-original points
            // TODO: revisit the idea of skipping non-original points
            if (vector_norm(fs->spatial_component(current_point->shift)) == 0 || !current_point->isOriginalPoint)
                continue;
            // Find the predecessor through gridded shift
            vector<int> gridpoint = current_point->gridpoint + current_point->gridded_shift;
            Point<T> *predecessor = NULL;
            try {
                predecessor = index.get(gridpoint);
            } catch (std::invalid_argument &e) {
#if DEBUG_GRAPH_AGGREGATION
                cout << "gridpoint=" << current_point->gridpoint
                        << " + gridded_shift = " << current_point->gridded_shift
                        << " = " << gridpoint
                        << " which caused exception " << e.what() << endl;
#endif
            }

            // Start testing
            if (predecessor != NULL) {
                // we're pointing to somebody?
                current_point->isBoundary = true;

                // whoever we're pointing to, he's
                // not a boundary point.
                predecessor->isBoundary = false;

#if DEBUG_GRAPH_AGGREGATION
                cout << endl;
                cout << "current point : " << current_point << " @ " << current_point->gridpoint << " (" << current_point->cluster << ")" << endl;
                cout << "predecessor   : " << predecessor << " @ " << predecessor->gridpoint << " (" << predecessor->cluster << ")" << endl;
                // cout << "(reverse lookup of " << x << " = " << gp << ")" << endl;
#endif
                if (current_point->cluster == NULL && predecessor->cluster == NULL) {
                    // Neither point has a cluster
                    // => create new one

                    //                    #if WITH_OPENMP
                    //                    #pragma omp critical
                    //                    #endif
                    {
                        typename Cluster<T>::ptr c = new Cluster<T>(current_point->values,
                                                                    fs->coordinate_system->rank());
                        c->id = cluster_id++;
                        c->add_point(current_point);
                        c->add_point(predecessor);
                        clusters.push_back(c);

#if DEBUG_GRAPH_AGGREGATION
                        cout << "created new cluster " << c
                                << " (" << c->size()
                                << " points)" << endl;
#endif
                    }
                } else if (current_point->cluster == NULL && predecessor->cluster != NULL) {
                    // current point has no cluster, but predecessor has one
                    // => add current point to predecessor's cluster

                    //                    #if WITH_OPENMP
                    //                    #pragma omp critical
                    //                    #endif
                    predecessor->cluster->add_point(current_point);

#if DEBUG_GRAPH_AGGREGATION
                    cout << "added current point to cluster "
                            << predecessor->cluster << " ("
                            << predecessor->cluster->size()
                            << " points)" << endl;
#endif
                } else if (current_point->cluster != NULL && predecessor->cluster == NULL) {
                    // current point has a cluster, but predecessor has none
                    // => add predecessor to current point's cluster

                    //                    #if WITH_OPENMP
                    //                    #pragma omp critical
                    //                    #endif
                    current_point->cluster->add_point(predecessor);

#if DEBUG_GRAPH_AGGREGATION
                    cout << "added predecessor to cluster "
                            << current_point->cluster
                            << " (" << current_point->cluster->size()
                            << " points)" << endl;
#endif
                } else if (current_point->cluster != predecessor->cluster) {
                    // both points have different clusters
                    // => merge current cluster's points to predecessor's cluster
                    //    and delete current cluster

                    // Save a little time by merging the smaller into the bigger cluster

                    typename Cluster<T>::ptr merged;
                    typename Cluster<T>::ptr mergee;

                    if (current_point->cluster->size() >= predecessor->cluster->size()) {
                        merged = current_point->cluster;
                        mergee = predecessor->cluster;
                    } else {
                        merged = predecessor->cluster;
                        mergee = current_point->cluster;
                    }

#if DEBUG_GRAPH_AGGREGATION
                    cout << "merging cluster " << mergee << " (" << mergee->size() << " points)"
                            << "into " << merged << " (" << merged->size() << " points)"
                            << endl;
#endif
                    //                    #if WITH_OPENMP
                    //                    #pragma omp critical
                    //                    #endif
                    {
                        // absorb predecessor
                        merged->add_points(mergee->get_points(), false);

                        // remove it
                        typename Cluster<T>::list::iterator fi = find(clusters.begin(), clusters.end(), mergee);
                        clusters.erase(fi);
                        delete mergee;
                    }
                } else {
                    // both points are already part of the same cluster
                    // => do nothing

#if DEBUG_GRAPH_AGGREGATION
                    cout << "Both points are part of the same cluster. Skip." << endl;
#endif
                }
            }
        }

        if (show_progress) {
            cout << "done. (Found " << clusters.size() << " clusters in " << stop_timer() << "s)" << endl;
            delete progress;
        }

        // TODO: parallelize
        if (coalesceWithStrongestNeighbour) {
            if (show_progress) {
                cout << endl << "Running coalescence post-procesing ";
                start_timer();
            }

            for (size_t i = 0; i < clusters.size(); i++) {
                typename Cluster<T>::ptr c = clusters.at(i);
                T strongest_response = numeric_limits<T>::min();
                T strongest_own_response = numeric_limits<T>::min();
                typename Cluster<T>::ptr strongest_cluster = NULL;
                typename Point<T>::list::iterator pi;
                for (pi = c->get_points().begin(); pi != c->get_points().end(); pi++) {
                    typename Point<T>::ptr p = *pi;

                    // track own response
                    T own_response = weight_function->operator()(p);
                    if (own_response > strongest_own_response) {
                        strongest_own_response = own_response;
                    }

                    // Find the neighbour with the strongest response
                    typename Point<T>::list neighbours = index.find_neighbours(p->gridpoint);
                    for (size_t ni = 0; ni < neighbours.size(); ni++) {
                        Point<T> *n = neighbours.at(ni);

                        // only interested in different clusters here
                        if (n->cluster == c) {
                            continue;
                        }

                        // figure out the response
                        T response = weight_function->operator()(n);
                        if (response > strongest_response) {
                            strongest_response = response;
                            strongest_cluster = n->cluster;
                        }
                    }
                }

                if (strongest_response >= strongest_own_response && strongest_cluster != NULL) {
                    // found a higher ranking cluster in the direct
                    // vicinity. Merge!
                    c->add_points(strongest_cluster->get_points(), false);

                    typename Cluster<T>::list::iterator cfi = find(clusters.begin(), clusters.end(), strongest_cluster);
                    if (cfi != clusters.end()) {
                        clusters.erase(cfi);
                        delete strongest_cluster;
                    }

                    // start over!
                    // TODO: this could be done a little smarter, probably
                    // by remembering the clusters to be deleted and skip
                    // them in the above procedure, then remove them later
                    // in bulk?
                    cout << ".";
                    i = 0;
                }
            }

            if (show_progress) {
                cout << "done. (Coalesced " << clusters.size() << " clusters in " << stop_timer() << "s)" << endl;
            }
        }

        // Finally remove all points from all clusters that were not part of the original 
        // data set. Make their modes the arithmetic mean of the remaining points.
        if (show_progress) {
            cout << endl << "Erasing non-original points ...";
            start_timer();
            progress = new boost::progress_display(clusters.size());
        }

        for (typename Cluster<T>::list::iterator clit = clusters.begin(); clit != clusters.end();) {
            if (show_progress) {
                progress->operator++();
            }

            typename Cluster<T>::ptr c = *clit;
            vector<T> mode = vector<T>(fs->rank(), 0.0);
            typename Point<T>::list keepers;

            // Make pointers unique
            typedef std::set<typename Point<T>::ptr> point_set_t;
            point_set_t point_set;
            point_set.insert(c->get_points().begin(), c->get_points().end());

            // Iterate over the unique set
            for (typename point_set_t::iterator si = point_set.begin(); si != point_set.end(); ++si) {
                typename Point<T>::ptr p = *si;
                if (p->isOriginalPoint) {
                    keepers.push_back(p);
                    mode += p->values;
                }
            }

            if (keepers.empty()) {
                // removed them all? Kill cluster
                clusters.erase(clit);
                delete c;
            } else {
                mode /= ((T) keepers.size());
                c->set_points(keepers);
                c->mode = mode;
                clit++;
            }
        }

        if (show_progress) {
            cout << "done. (Result: " << clusters.size() << " clusters in " << stop_timer() << "s)" << endl;
            delete progress;
        }

        // PointIndex<T>::write_index_searches = false;
    }

    template<typename T>
    typename Cluster<T>::list
    ClusterList<T>::neighbours_of(typename Cluster<T>::ptr cluster,
                                  ArrayIndex<T> &index) {
        typename Cluster<T>::list neighbouring_clusters;
        typename Point<T>::list::const_iterator pi;

        for (pi = cluster->points.begin(); pi != cluster->points.end(); pi++) {
            typename Point<T>::ptr p = *pi;
            typename Point<T>::list neighbours = this->find_neighbours(index, p->gridpoint);
            typename Point<T>::list::const_iterator ni;
            for (ni = neighbours->begin(); ni != neighbours->end(); ni++) {
                Point<T> *n = *ni;
                // Exclude points, that have not been clustered.
                // This can happen because scale-space filtering
                // creates new points, but those are not associated
                // with clusters in later steps
                if (n->cluster == NULL) continue;

                if (n->cluster != p->cluster) {
                    typename Cluster<T>::list::const_iterator fi = find(neighbouring_clusters.begin(),
                                                                        neighbouring_clusters.end(), n->cluster);
                    if (fi == neighbouring_clusters.end()) {
                        neighbouring_clusters.push_back(n->cluster);
                    }
                }
            }
        }
        return neighbouring_clusters;
    }

    template<typename T>
    typename Point<T>::list
    ClusterList<T>::get_boundary_points(typename Cluster<T>::ptr c1,
                                        typename Cluster<T>::ptr c2,
                                        ArrayIndex<T> &index) {
        typename Point<T>::list boundary_points;
        typename Point<T>::list::const_iterator pi;

        for (pi = c1->points.begin(); pi != c1->points.end(); pi++) {
            typename Point<T>::ptr p = *pi;
            typename Point<T>::list neighbours = find_neighbours(index, p->gridpoint);
            typename Point<T>::list::const_iterator ni;

            for (ni = neighbours->begin(); ni != neighbours->end(); ni++) {
                typename Point<T>::ptr n = *ni;

                if (n->cluster == c2) {
                    // check every time to avoid double adding
                    typename Point<T>::list::const_iterator fi = find(boundary_points.begin(), boundary_points.end(),
                                                                      n);
                    if (fi == boundary_points.end()) {
                        boundary_points.push_back(n);
                    }

                    fi = find(boundary_points.begin(), boundary_points.end(), p);
                    if (fi == boundary_points.end()) {
                        boundary_points.push_back(p);
                    }
                }
            }
        }

        for (pi = c2->points.begin(); pi != c2->points.end(); pi++) {
            typename Point<T>::ptr p = *pi;
            typename Point<T>::list neighbours = find_neighbours(index, p->gridpoint);
            typename Point<T>::list::const_iterator ni;
            for (ni = neighbours->begin(); ni != neighbours->end(); ni++) {
                typename Point<T>::ptr n = *ni;

                if (n->cluster == c1) {
                    // check every time to avoid double adding
                    typename Point<T>::list::const_iterator fi = find(boundary_points.begin(), boundary_points.end(),
                                                                      n);
                    if (fi == boundary_points.end()) {
                        boundary_points.push_back(n);
                    }

                    fi = find(boundary_points.begin(), boundary_points.end(), p);
                    if (fi == boundary_points.end()) {
                        boundary_points.push_back(p);
                    }
                }
            }
        }
    }

    template<typename T>
    void
    ClusterList<T>::write_boundaries(const WeightFunction<T> *weight_function,
                                     FeatureSpace<T> *fs,
                                     PointIndex<T> *index,
                                     const vector<T> &resolution) {
        // collate the data

        typedef vector<typename Point<T>::list> boundaries_t;
        boundaries_t boundaries;
        typedef vector<std::string> boundary_key_t;
        boundary_key_t boundary_keys;
        vector<T> var_c1, var_c2, var_boundary;
        vector<T> range_factor_c1, range_factor_c2;
        vector<typename Cluster<T>::id_t> cluster_index_1, cluster_index_2;
        typename Cluster<T>::list::const_iterator ci;

        for (ci = clusters.begin(); ci != clusters.end(); ci++) {
            typename Cluster<T>::ptr c = *ci;
            typename Cluster<T>::list neighbours = neighbours_of(c, index, resolution, weight_function);

            if (neighbours.size() > 0) {
                // go over the list of neighbours and find candidates for merging
                typename Cluster<T>::list::const_iterator ni;

                for (ni = neighbours.begin(); ni != neighbours.end(); ni++) {
                    typename Cluster<T>::ptr n = *ni;
                    std::string key = boost::lexical_cast<string>(inf(c->id, n->id)) + "-" +
                                      boost::lexical_cast<string>(sup(c->id, n->id));
                    typename boundary_key_t::const_iterator fi = find(boundary_keys.begin(), boundary_keys.end(), key);
                    if (fi == boundary_keys.end()) {
                        boundary_keys.push_back(key);
                        typename Point<T>::list boundary_points;
                        this->get_boundary_points(c, n, boundary_points, index, resolution);
                        if (boundary_points.size() == 0) continue;

                        boundaries.push_back(boundary_points);
                        var_boundary.push_back(relative_variability(weight_function, boundary_points));
                        var_c1.push_back(relative_variability(weight_function, c->points));
                        var_c2.push_back(relative_variability(weight_function, n->points));

                        range_factor_c1.push_back(dynamic_range_factor(c, boundary_points, weight_function));
                        range_factor_c2.push_back(dynamic_range_factor(n, boundary_points, weight_function));

                        cluster_index_1.push_back(c->id);
                        cluster_index_2.push_back(n->id);
                    }
                }
            }
        }

#if WITH_VTK
        for (size_t index = 0; index < boundaries.size(); index++) {
            typename Point<T>::list b = boundaries[index];
            std::string fn = fs->filename() + "_boundary_" + boost::lexical_cast<string>(index) + ".vtk";
            boost::replace_all(fn, "/", "_");
            boost::replace_all(fn, "..", "");
            VisitUtils<T>::write_pointlist_vtk(fn, &b, fs->coordinate_system->rank());
        }
#endif
        std::string fn = fs->filename() + "_boundary_correlations.txt";
        std::ofstream f(fn.c_str());
        f << "#\t"
          << "c1\t"
          << "c2\t"
          //        << "var_c1\t"
          //        << "var_c2\t"
          //        << "var_b\t"
          << "drf_1\t"
          << "drf_2\t"
          << std::endl;
        for (size_t index = 0; index < boundaries.size(); index++) {
            f << index << "\t"
              << cluster_index_1[index] << "\t"
              << cluster_index_2[index] << "\t"
              //            << var_c1[index] << "\t"
              //            << var_c2[index] << "\t"
              //            << var_boundary[index] << "\t"
              << range_factor_c1[index] << "\t"
              << range_factor_c2[index] << std::endl;
        }
    }

    template<typename T>
    typename Cluster<T>::ptr
    ClusterList<T>::merge_clusters(typename Cluster<T>::ptr c1, typename Cluster<T>::ptr c2) {
        vector<T> merged_mode = (T) 0.5 * (c1->mode + c2->mode);
        typename Cluster<T>::ptr merged_cluster = new Cluster<T>(merged_mode, this->dimensions.size());
        merged_cluster->add_points(c1->points);
        merged_cluster->add_points(c2->points);
        merged_cluster->id = c1->id;
        if (c1->m_weight_range_calculated && c2->m_weight_range_calculated) {
            merged_cluster->m_min_weight = inf(c1->m_min_weight, c2->m_min_weight);
            merged_cluster->m_max_weight = sup(c1->m_max_weight, c2->m_max_weight);
            merged_cluster->m_weight_range_calculated = true;
        }
        return merged_cluster;
    }

    template<typename T>
    void
    ClusterList<T>::erase_identifiers() {
        for (size_t i = 0; i < clusters.size(); i++) {
            clusters[i]->id = m3D::NO_ID;
        }
    }

    template<typename T>
    struct clear_cluster
    {
        void operator()(void *p) {
            static_cast<typename Point<T>::ptr> (p)->cluster = NULL;
        };
    };

    template<typename T>
    void
    ClusterList<T>::reset_clustering(FeatureSpace<T> *fs) {
        for_each(fs->points.begin(), fs->points.end(), clear_cluster<T>());
    }

    template<typename T>
    void
    ClusterList<T>::sanity_check(const FeatureSpace<T> *fs) {
        size_t point_count = 0;
        for (size_t i = 0; i < clusters.size(); i++) {
            point_count += clusters[i]->points.size();
        }
        assert(point_count == fs->size());
    }

#pragma mark -
#pragma mark Coalescence Merging

    template<typename T>
    bool
    ClusterList<T>::are_neighbours(const Cluster<T> *c1,
                                   const Cluster<T> *c2,
                                   ArrayIndex<T> &index) {
        bool isNeighbour = false;
        typename Point<T>::list::const_iterator pi;
        for (pi = c1->points.begin(); pi != c1->points.end(); pi++) {
            typename Point<T>::ptr p = *pi;
            typename Point<T>::list neighbours = this->find_neighbours(c1, index);
            typename Point<T>::list::const_iterator ni;
            for (ni = neighbours->begin(); ni != neighbours->end(); ni++) {
                Point<T> *n = *ni;
                if (n->cluster == c2) {
                    isNeighbour = true;
                    break;
                }
            }
        }
        return isNeighbour;
    }

    // Sort all clusters in ascending order by weight response

    template<typename T>
    class ModalWeightComparator
    {
    private:
        const WeightFunction<T> *m_weight;
    public:

        ModalWeightComparator(const WeightFunction<T> *w) {
            m_weight = w;
        }

        bool
        operator()(const Cluster<T> *c1, const Cluster<T> *c2) {
            T w1 = c1->modal_weight_response(m_weight);
            T w2 = c2->modal_weight_response(m_weight);
            return w1 < w2;
        }
    };

} //namespace

#endif
