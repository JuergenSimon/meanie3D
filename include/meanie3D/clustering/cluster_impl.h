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


    #ifndef M3D_CLUSTER_IMPL_H
    #define M3D_CLUSTER_IMPL_H

    #include <algorithm>
    #include <iostream>
    #include <limits>
    #include <vector>

    #include <meanie3D/featurespace/point.h>

    #include "cluster.h"

    namespace m3D {

        using namespace std;

    #pragma mark -
    #pragma mark Constructor/Destructor

        template<typename T>
        Cluster<T>::Cluster()
                : m_radius(-1), m_index(NULL), m_rank(0), m_spatial_rank(0), m_weight_range_calculated(false),
                m_min_weight(0), m_max_weight(0), mode(vector<T>()), displacement(vector<T>()), id(NO_ID), uuid(NO_UUID) {
        }

        template<typename T>
        Cluster<T>::Cluster(const vector<T> &mode, size_t spatial_dimension)
                : m_radius(-1), m_index(NULL), m_rank(mode.size()), m_spatial_rank(spatial_dimension),
                m_weight_range_calculated(false), m_min_weight(0), m_max_weight(0), mode(mode),
                displacement(vector<T>(spatial_dimension)), id(NO_ID), uuid(NO_UUID) {
            assert(m_rank > m_spatial_rank);
        }

        template<typename T>
        Cluster<T>::Cluster(const Cluster<T> &o)
                : m_points(o.get_points()), m_radius(-1), m_index(NULL), m_rank(o.m_rank), m_spatial_rank(o.m_spatial_rank),
                m_weight_range_calculated(o.m_weight_range_calculated), m_min_weight(o.m_min_weight),
                m_max_weight(o.m_max_weight), mode(o.mode), displacement(o.displacement), id(o.id), uuid(o.uuid) {
        }

        template<typename T>
        Cluster<T>::~Cluster() {
            this->clear_histogram_cache();
            this->clear_index();
        }

    #pragma mark -
    #pragma mark Adding / Removing points

        template<typename T>
        void
        Cluster<T>::add_point(Point <T> *point) {
            m_points.push_back(point);
            point->cluster = this;
        }

        template<typename T>
        void
        Cluster<T>::add_points(const vector<Point < T> *

        > &list,
        bool addOriginalPointsOnly
        ) {
        typename Point<T>::list::const_iterator li;
        for (
        li = list.begin();
        li != list.

        end();

        li++) {
        typename Point<T>::ptr p = *li;
        if (!p->
        isOriginalPoint &&addOriginalPointsOnly
        ) {
        continue;
    }
    this->
    add_point(p);
    }
    }

    template<typename T>
    void
    Cluster<T>::remove_point(Point <T> *point) {
        typename vector<Point < T> * >::iterator it = this->get_points().find(*point);
        if (it != this->get_points().end()) {
            this->get_points().erase(it);
            point->cluster = NULL;
        }
    }

    template<typename T>
    bool
    Cluster<T>::has_point(typename Point<T>::ptr point) {
        typename Point<T>::list::iterator fi;
        fi = find(this->get_points().begin(), this->get_points().end(), point);
        return fi != this->get_points().end();
    }

    template<typename T>
    typename Point<T>::list &
    Cluster<T>::get_points() {
        return m_points;
    }

    template<typename T>
    void
    Cluster<T>::set_points(const typename Point<T>::list &points, bool delete_flag) {
        this->clear(delete_flag);
        m_points = points;
    }

    template<typename T>
    bool
    Cluster<T>::empty() const {
        return m_points.empty();
    }

    template<typename T>
    size_t
    Cluster<T>::size() const {
        return m_points.size();
    };

    template<typename T>
    typename Point<T>::ptr
    Cluster<T>::operator[](const size_t &index) {
        return m_points.at(index);
    };

    template<typename T>
    typename Point<T>::ptr
    Cluster<T>::at(const size_t &index) const {
        return m_points.at(index);
    }

    template<typename T>
    void
    Cluster<T>::clear(bool deletion_flag) {
        if (deletion_flag) {
            typename Point<T>::list::const_iterator pi;
            for (pi = m_points.begin(); pi != m_points.end(); ++pi) {
                typename Point<T>::ptr p = *pi;
                delete p;
            }
        }
        m_points.clear();
    }

    template<typename T>
    bool Cluster<T>::has_margin_points() const {
        return m_has_margin_points;
    }

    template<typename T>
    void Cluster<T>::set_has_margin_points(bool value) {
        m_has_margin_points = value;
    }

    #pragma mark -
    #pragma mark Derived properties

    template<typename T>
    vector <T>
    Cluster<T>::weighed_center(const size_t &variable_index) {
        throw "not implemented";
    }

    #pragma mark -
    #pragma mark Operators

    template<typename T>
    bool
    Cluster<T>::operator==(const Cluster <T> &o) {
        return o.mode() == mode();
    }

    template<typename T>
    Cluster <T>
    Cluster<T>::operator=(const Cluster <T> &o) {
        return Cluster<T>(o);
    }

    #pragma mark -
    #pragma mark Histograms

    template<typename T>
    const typename Histogram<T>::ptr
    Cluster<T>::histogram(size_t variable_index,
                        T valid_min,
                        T valid_max,
                        size_t number_of_bins) {
        Histogram <T> *h = NULL;

        try {
            h = this->m_histograms.at(variable_index);
        } catch (const std::exception &e) {
            h = Histogram<T>::create(m_points, variable_index, valid_min, valid_max, number_of_bins);
            this->m_histograms.insert(std::pair<size_t, typename Histogram<T>::ptr>(variable_index, h));
        }

        return h;
    }

    template<typename T>
    void
    Cluster<T>::clear_histogram_cache() {
        typename histogram_map_t::iterator it;
        for (it = this->m_histograms.begin(); it != this->m_histograms.end(); ++it) {
            delete it->second;
        }
        this->m_histograms.clear();
    }

    #pragma mark -
    #pragma mark Index

    template<typename T>
    PointIndex <T> *
    Cluster<T>::index() {
        if (this->m_index == NULL) {
            // index is constructed using the spatial components only
            vector <size_t> indexes(m_spatial_rank);
            for (size_t i = 0; i < m_spatial_rank; i++) {
                indexes[i] = i;
            }
            this->m_index = PointIndex<T>::create(&this->points, indexes);
        }
        return m_index;
    }

    template<typename T>
    void
    Cluster<T>::clear_index() {
        if (this->m_index != NULL) {
            delete this->m_index;
            this->m_index = NULL;
        }
    }

    #pragma mark -
    #pragma mark Coverage

    template<typename T>
    vector <T>
    Cluster<T>::geometrical_center() {
        using namespace utils::vectors;

        if (m_geometrical_center.empty()) {
            vector <T> center(m_spatial_rank, 0.0);
            typename Point<T>::list::iterator pi;
            for (pi = this->get_points().begin(); pi != this->get_points().end(); ++pi) {
                vector <T> tmp = center;
                center += (*pi)->coordinate;
                if (isnan(center[0])) {
                    cerr << "ERROR:ARGHH!" << endl;
                }
            }
            m_geometrical_center = center / ((T) this->get_points().size());
        }

        return m_geometrical_center;
    }

    template<typename T>
    vector <T>
    Cluster<T>::weighed_center(size_t variable_index) {
        vector <T> wc;
        try {
            wc = this->m_weighed_centers.at(variable_index);
        } catch (const std::exception &e) {
            // first, figure out the min/max values

            typename Point<T>::list::const_iterator pi;

            T min = std::numeric_limits<T>::max();
            T max = std::numeric_limits<T>::min();
            for (pi = this->get_points().begin(); pi != this->get_points().end(); pi++) {
                T val = (*pi)->values[variable_index];
                if (val < min) {
                    min = val;
                }
                if (val > max) {
                    max = val;
                }
            }

            T range = max - min;

            // weight is assigned based on percentage of the range
            // within [min..max]

            vector <T> center(m_spatial_rank, 0.0);
            T overall_mass_percent = 0.0;
            for (pi = this->get_points().begin(); pi != this->get_points().end(); pi++) {
                T mass = (*pi)->values[variable_index];
                T massPercent = (max - mass) / range;
                center += (*pi)->coordinate * massPercent;
                overall_mass_percent += massPercent;
            }

            center /= overall_mass_percent;
            m_weighed_centers.insert(std::pair<size_t, vector < T> > (variable_index, center));
            wc = m_weighed_centers[variable_index];
        }

        return wc;
    }

    template<typename T>
    void
    Cluster<T>::clear_center_caches() {
        m_geometrical_center.clear();
        m_weighed_centers.clear();
    }

    template<typename T>
    ::units::values::m
    Cluster<T>::radius(const CoordinateSystem <T> *cs) {
        using namespace utils::vectors;
        if (m_radius == ::units::values::m(-1)) {
            vector <T> mode_spatial(&mode[0], &mode[m_spatial_rank]);
            vector <T> mode_in_meters = cs->to_meters(mode_spatial);
            typename Point<T>::list::iterator pi;
            ::units::values::m sum;
            for (pi = this->get_points().begin(); pi != this->get_points().end(); pi++) {
                typename Point<T>::ptr p = *pi;
                vector <T> coord_in_meters = cs->to_meters(p->coordinate);
                ::units::values::m dist = ::units::values::m(vector_norm(coord_in_meters - mode_in_meters));
                sum += dist;
            }
            m_radius = sum / ((T) this->get_points().size());
        }
        return m_radius;
    }

    template<typename T>
    T
    Cluster<T>::modal_weight_response(const WeightFunction <T> *w) const {
        T result = 0;

        // pick the first point of this cluster to figure out the
        // spatial and value dimensions

        if (!m_points.empty() && w != NULL) {
            const CoordinateSystem <T> *cs = this->m_index->feature_space()->coordinate_system;

            // extract coordinate of the mode and do a reverse
            // lookup on the grid point
            vector <T> coordinate(&mode[0], &mode[cs->rank()]);
            vector<int> gridpoint;
            cs->reverse_lookup(coordinate, gridpoint);

            // Construct a point object for the weight function
            Point <T> point;
            point.values = mode;
            point.coordinate = coordinate;
            point.gridpoint = gridpoint;

            result = w->operator()(&point);
        }

        return result;
    }

    template<typename T>
    T
    Cluster<T>::average_weight_response(const WeightFunction <T> *w) const {
        T result = 0;

        // pick the first point of this cluster to figure out the
        // spatial and value dimensions

        for (size_t i = 0; i < this->size(); i++) {
            Point <T> *p = this->at(i);
            result += w->operator()(p);
        }

        if (m_points.empty()) {
            result /= ((T) this->size());
        }

        return result;
    }

    #pragma mark -
    #pragma mark Dynamic Range Calculation

    template<class T>
    void
    Cluster<T>::dynamic_range(const typename Point<T>::list &points,
                            const WeightFunction <T> *weight_function,
                            T &lower_bound,
                            T &upper_bound) {
        lower_bound = std::numeric_limits<T>::max();
        upper_bound = std::numeric_limits<T>::min();

        typename Point<T>::list::const_iterator pi;
        for (pi = points.begin(); pi != points.end(); pi++) {
            typename Point<T>::ptr p = *pi;
            T value = weight_function->operator()(p);
            if (value < lower_bound) {
                lower_bound = value;
            }
            if (value > upper_bound) {
                upper_bound = value;
            }
        }
    }

    template<typename T>
    void
    Cluster<T>::dynamic_range(const WeightFunction <T> *weight_function,
                            T &lower_bound,
                            T &upper_bound) {
        if (!m_weight_range_calculated) {
            Cluster<T>::dynamic_range(this->points, weight_function, m_min_weight, m_max_weight);
            m_weight_range_calculated = true;
        }
        lower_bound = m_min_weight;
        upper_bound = m_max_weight;
    }

    template<typename T>
    void
    Cluster<T>::variable_ranges(std::vector<T> &min,
                                std::vector<T> &max,
                                std::vector<T> &median) {
        size_t var_rank = m_rank - m_spatial_rank;

        min.clear();
        min.resize(var_rank, std::numeric_limits<T>::max());

        max.clear();
        max.resize(var_rank, std::numeric_limits<T>::min());

        median.clear();
        median.resize(var_rank, 0);

        // For median calculation
        vector < vector<T> > median_values;
        median_values.resize(var_rank, vector<T>());

        typename Point<T>::list::const_iterator pi;

        for (pi = this->get_points().begin(); pi != this->get_points().end(); ++pi) {
            vector <T> values = (*pi)->values;

            for (size_t i = 0; i < var_rank; i++) {
                T value = values.at(m_spatial_rank + i);
                // record values for median
                median_values.at(i).push_back(value);
                // min/max checks
                if (value < min.at(i)) min.at(i) = value;
                if (value > max.at(i)) max.at(i) = value;
            }
        }

        // finish median calculation

        for (size_t i = 0; i < var_rank; i++) {
            vector <T> values = median_values.at(i);
            if (!values.empty()) {
                if (values.size() == 1) {
                    // special case: only one element
                    median.at(i) = values.at(0);
                } else {
                    // two or more elements
                    std::sort(values.begin(), values.end());
                    if (values.size() % 2 == 0) {
                        size_t index = values.size() / 2 - 1;
                        median.at(i) = (values.at(index) + values.at(index + 1)) / 2.0;
                    } else {
                        size_t index = (values.size() - 1) / 2;
                        median.at(i) = values.at(index);
                    }
                }
            }
        }
    }

    template<typename T>
    void
    Cluster<T>::print(bool includePoints) {
        cout << "Cluster uuid:" << this->uuid
            << " id:" << this->id
            << " mode:" << this->mode
            << " size:" << this->size()
            << endl;

        if (includePoints) {
            for (size_t i = 0; i < this->size(); i++) {
                typename Point<T>::ptr p = this->at(i);
                p->print(1);
            }
        }
    }


    template<typename T>
    void
    Cluster<T>::set_bounding_box_min(const vector <T> &bounds) {
        m_bounding_box_min = bounds;
    };

    template<typename T>
    const vector <T> &
    Cluster<T>::get_bounding_box_min() {
        if (m_bounding_box_min.empty()) {
            vector <T> inf(spatial_rank(), std::numeric_limits<T>::max());
            typename Point<T>::list::iterator pi;
            for (pi = m_points.begin(); pi != m_points.end(); ++pi) {
                typename Point<T>::ptr p = *pi;
                for (size_t j = 0; j < spatial_rank(); j++) {
                    if (p->coordinate[j] < inf[j]) {
                        inf[j] = p->coordinate[j];
                    }
                }
            }
            m_bounding_box_min = inf;

        }
        return m_bounding_box_min;
    };

    template<typename T>
    void
    Cluster<T>::set_bounding_box_max(const vector <T> &bounds) {
        m_bounding_box_max = bounds;
    };

    template<typename T>
    const vector <T> &
    Cluster<T>::get_bounding_box_max() {
        if (m_bounding_box_max.empty()) {
            vector <T> sup(spatial_rank(), -std::numeric_limits<T>::max());
            typename Point<T>::list::iterator pi;
            for (pi = m_points.begin(); pi != m_points.end(); ++pi) {
                typename Point<T>::ptr p = *pi;
                for (size_t j = 0; j < spatial_rank(); j++) {
                    if (p->coordinate[j] > sup[j]) {
                        sup[j] = p->coordinate[j];
                    }
                }
            }
            m_bounding_box_max = sup;

        }
        return m_bounding_box_max;
    };

    }

    #endif
