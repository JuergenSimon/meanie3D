#ifndef _M3D_ClusterList_H_
#define _M3D_ClusterList_H_

#include <meanie3D/defines.h>
#include <meanie3D/namespaces.h>

#include <vector>
#include <map>
#include <assert.h>
#include <boost/shared_ptr.hpp>
#include <stdlib.h>
#include <netcdf>

#include <cf-algorithms/cf-algorithms.h>

#include <meanie3D/types/cluster.h>
#include <meanie3D/types/point.h>

namespace m3D {
    
    using namespace netCDF;
	using namespace std;
	using ::cfa::meanshift::Point;
    using ::cfa::meanshift::WeightFunction;

    /** Cluster of points in feature space. A cluster is a point in feature space,
     * where multiple trajectories of original feature space points end. This end
     * point is called the cluster's 'mode'.
     *
     * At the same time, this class represents a serialized form of a list of clusters
     * to be used in further processing, such as tracking. Serialization/Deserialization
     * to and from files are done using the read/write methods.
     */
    template <class T>
    class ClusterList
    {
        
    private:
        
        typedef map< vector<T>, typename Cluster<T>::ptr > ClusterMap;
        
        ClusterMap      m_cluster_map;
        
        
        /** Standard constructor is private
         */
        ClusterList()
        {};
        
    public:
        
#pragma mark -
#pragma mark Public typedefs
        
        typedef ClusterList<T> *    ptr;
        
#pragma mark -
#pragma mark Public Properties

        // meta-info
        
        NcFile          *ncFile;
        
        vector<NcVar>   feature_variables;            // all variables, including dimension variables
        
        vector<NcDim>   dimensions;                   // all dimensions that were in the featurespace
        
        string          source_file;                  // Name of the file that the clusters were created from
        
        // payload

        typename Cluster<T>::list   clusters;
        
        // tracking help
        
        bool            tracking_performed;
        
        vector<size_t>  tracked_ids;
        
        vector<size_t>  dropped_ids;
        
        vector<size_t>  new_ids;
        
        // control
        
        size_t          m_predecessor_maxdistance_gridpoints;
        
        // make the width of the boundary half the resolution
        // that is 0.25 left and right of the boundary = 0.5 * resolution
        // Note: this will start to generate problems, if the bandwidth
        // chosen is too small.
        T               m_neighbourhood_range_search_multiplier;
        
        bool            m_use_original_points_only;
        

#pragma mark -
#pragma mark Constructor/Destructor

        /** @constructor
         * @param all variables used to contruct the points from, in the same order
         * @param all dimensions that were in the featurespace
         * @param name of the file the clusters were created from
         * @param the command line parameters used to cluster
         * @param when searching for predecessors in the meanshift vector graph,
         *          there is a maximum distance in gridpoints, after which the 
         *          neighbourhood search will not accept points. This parameter
         *          sets that distance in #grid-points.
         * @param In the process of linking up meanshift vectors to a graph, the
         *          vicinity of vector endpoints is searched for other points to
         *          link up to. The maximum distance of that search (in grid points)
         *          is determined here.
         * @param When adding points to clusters, this switch can be used to control
         *          if only points from the original data set are added or also points
         *          that resulted from filtering (for example scale-space)
         */
        ClusterList(const vector<NcVar> &variables,
                    const vector<NcDim> &dims,
                    const string& sourcefile,
                    size_t predecessor_maxdistance_gridpoints=3,
                    T neighbourhood_range_search_multiplier=1.0,
                    bool use_original_points_only=true)
        : ncFile(NULL)
        , feature_variables(variables)
        , dimensions(dims)
        , source_file(sourcefile)
        , tracking_performed(false)
        , m_predecessor_maxdistance_gridpoints(predecessor_maxdistance_gridpoints)
        , m_neighbourhood_range_search_multiplier(neighbourhood_range_search_multiplier)
        , m_use_original_points_only(use_original_points_only)
        {};

        /** @constructor
         * @param a list of clusters
         * @param all variables used to contruct the points from, in the same order
         * @param the spatial dimension (2D/3D)
         * @param name of the file the clusters were created from
         * @param the command line parameters used to cluster
         */
        ClusterList(const typename Cluster<T>::list &list,
                    const vector<NcDim> &dims,
                    const vector<NcVar> &variables,
                    const string& sourcefile )
        : ncFile(NULL)
        , feature_variables(variables)
        , dimensions(dims)
        , source_file(sourcefile)
        , clusters(list)
        , tracking_performed(false)
        {};

        /** Destructor
         */
        ~ClusterList()
        {
            close_ncFile();
        };
        
        void close_ncFile()
        {
            if (ncFile!=NULL)
            {
                delete ncFile;
                ncFile=NULL;
            }
        };
        
#pragma mark -
#pragma mark Accessing the list
        
        /** @return number of clusters in the list
         */
        size_t size();
        
        /** @param index
         * @return cluster at index
         */
        typename Cluster<T>::ptr operator[] (size_t index);
        
#pragma mark -
#pragma mark Adding / Removing points
        
        typedef typename FeatureSpace<T>::Trajectory Trajectory;
        
        /** Adds the end point of the trajectory as cluster. If a cluster already
         * exists, this point is added to it. All points within grid resolution 
         * along the trajectory are also added as points to this cluster as well.
         * @param feature space point x (the start point)
         * @param list of feature space coordinates (trajectory)
         * @param bandwidth of the iteration
         * @param fuzziness, which defines how close clusters can get to each other.
         * @param index
         */
        void add_trajectory( const typename Point<T>::ptr x,
                            Trajectory *trajectory,
                            FeatureSpace<T> *fs );
        
#pragma mark -
#pragma mark Superclustering
        
        /** Aggregate all clusters who's modes are within grid resolution of each other
         * into superclusters. The resulting superclusters will replace the existing
         * clusters.
         * @param Original featurespace
         * @param How close the clusters must be, in order to be added to the same supercluster.
         */
        void
        aggregate_by_superclustering( const FeatureSpace<T> *fs, const vector<T> &resolution );
        
        /** For each cluster in this list, checks the parent_clusters and finds the modes, that
         * are in the cluster's point list and collects all their points into a list. Finally,
         * it replaces the cluster's point list with this aggregated list of parent points.
         * Used to build superclusters hierarchically from smaller clusters.
         * @param parent cluster
         * @param min_cluster_size : drop clusters from the result that have fewer points
         */
        void aggregate_with_parent_clusters( const ClusterList<T> &parent_clusters );
        
#pragma mark -
#pragma mark Clustering by Graph Theory
        
        // TODO: make the API more consistent by ordering the parameters equally
        // TODO: assign access classifiers (private/protected/public) where needed
        
        /** This requires that the shift has been calculated at each point
         * in feature-space and is stored in the 'shift' property of each
         * point. Aggregates clusters by taking each point, calculate the
         * shift target and find the closest point in feature-space again.
         * If two or more points have the same distance to the shifted
         * coordinate, the point with the steeper vector (=longer) is
         * chosen. If
         */
        void aggregate_cluster_graph( const WeightFunction<T> *weight_function,
                                     FeatureSpace<T> *fs, const
                                     vector<T> &resolution,
                                     const bool& show_progress );
        
        /** Finds the 'best' graph predecessor for given point p. This is done
         * by adding the 'shift' property to calculate an end point and find
         * the closest neighbour.
         * TODO: find the closest n neighbours and pick the one with the smallest
         * shift (closer to local maximum)
         * @param feature-space
         * @param feature-space index
         * @param grid resolution
         * @param weight function (can not be null)
         * @param point
         * @return best predecessor along the shift. Might return the argument,
         *         in which case we have found a mode.
         */
        typename Point<T>::ptr
        predecessor_of(FeatureSpace<T> *fs,
                       PointIndex<T> *index,
                       const vector<T> &resolution,
                       const WeightFunction<T> *weight_function,
                       typename Point<T>::ptr p);
        
        /** Find all directly adjacent clusters to the given cluster
         * @param cluster
         * @param feature-space index for searching
         * @param resolution search radius for finding neighbours (use cluster_resolution)
         * @return list of neighbouring clusters (can be empty)
         */
        typename Cluster<T>::list
        neighbours_of( typename Cluster<T>::ptr cluster,
                      PointIndex<T> *index,
                      const vector<T> &resolution,
                      const WeightFunction<T> *weight_function );
        
        /** Analyses the two clusters and decides, if they actually belong to the same
         * object or not. Make sure this is only invoked on direct neighbours!
         * @param cluster 1
         * @param cluster 2
         * @param variable index
         * @param feature-space index
         * @param search range (use cluster_resolution)
         * @return yes or no
         */
        bool
        should_merge_neighbouring_clusters( typename Cluster<T>::ptr c1,
                                           typename Cluster<T>::ptr c2,
                                           const WeightFunction<T> *weight_function,
                                           PointIndex<T> *index,
                                           const vector<T> &resolution,
                                           const double &drf_threshold);
        
        /** Find the boundary points of two clusters.
         * @param cluster 1
         * @param cluster 2
         * @return vector containing the boundary points
         * @param feature-space index
         * @param search range (use cluster resolution)
         */
        void
        get_boundary_points( typename Cluster<T>::ptr c1,
                            typename Cluster<T>::ptr c2,
                            typename Point<T>::list &boundary_points,
                            PointIndex<T> *index,
                            const vector<T> &resolution );
        
        
        /** Calculates the relative variablity of values in the given list
         * of points. High variability indicates strong profile in the given
         * value across the boundary
         * @param weight function
         * @param list of points
         * @return CV = s / m
         */
        T
        relative_variability( const WeightFunction<T> *weight_function,
        		  	  	  	  const typename Point<T>::list &points );
        
        /** Finds the lower and upper bound of the weight function response
         * @param point list
         * @param weight function
         * @param lower_bound (return value)
         * @param upper_bound (return value)
         */
        static void
        dynamic_range(const typename Point<T>::list &list,
                      const WeightFunction<T> *weight_function,
                      T &lower_bound,
                      T &upper_bound );
        
        /** Finds the lower and upper bound of weight function response in the 
         * whole cluster
         * @param cluster
         * @param weight function
         * @param lower_bound (return value)
         * @param upper_bound (return value)
         */
        static void
        dynamic_range(const typename Cluster<T>::ptr cluster,
                      const WeightFunction<T> *weight_function,
                      T &lower_bound,
                      T &upper_bound);

        
        /** Classifies the properties of the dynamic range of the given list of points, compared to
         * the given range in the response of the weight function
         * Strong dynamic range component indicates a boundary, that cuts through high signal areas.
         * Weak dynamic range component indicates a more clean cut in a through.
         * @param list of points to check
         * @param weight function
         * @param lower bound of comparison range
         * @param upper bound of comparison range
         * @return classification
         */
        T
        dynamic_range_factor(typename Cluster<T>::ptr cluster,
                             const typename Point<T>::list &points,
                             const WeightFunction<T> *weight_function );
        
        /** Merges the two clusters into a new cluster and removes the mergees from
         * the list of clusters, while inserting the new cluster. The mode of the merged
         * cluster is the arithmetic mean of the modes of the merged clusters.
         * @param cluster 1
         * @param cluster 2
         * @return pointer to the merged cluster
         */
        typename Cluster<T>::ptr
        merge_clusters( typename Cluster<T>::ptr c1, typename Cluster<T>::ptr c2 );
        
        
        /** Finds the neighbours of each cluster and analyses the boundaries between them.
         * If the analysis indicates that a merge is due, the clusters are merged and the
         * procedure starts over. This is repeated, until no more merges are indicated.
         * @param index of variable used for boundary analysis
         * @param feature-space index (for searching)
         * @param resolution (use cluster_resolution)
         */
        void aggregate_clusters_by_boundary_analysis(const WeightFunction<T> *weight_function,
                                                     PointIndex<T> *index,
                                                     const vector<T> &resolution,
                                                     const double &drf_threshold,
                                                     const bool& show_progress);
        
#pragma mark -
#pragma mark ID helpers
        
        /** Iterates over all clusters and sets their ID to NO_ID 
         */
        void erase_identifiers();
        
        /** Re-tags sequentially, starting with 0 
         */
        void retag_identifiers();
        
#pragma mark -
#pragma mark Post-Processing
        
        /** Iterate through the list of clusters and trow all with smaller number of
         * points than the given number out.
         * @param min number of points
         */
        void apply_size_threshold( unsigned int min_cluster_size, const bool& show_progress = true );
        
#pragma mark -
#pragma mark I/O
        
        /** Writes out the cluster list into a NetCDF-file.
         * For the format, check documentation at
         * http://git.meteo.uni-bonn.de/projects/meanie3d/wiki/Cluster_File
         * @param full path to filename, including extension '.nc'
         * @param feature space
         * @param parameters used in the run
         */
        void write( const string& path );
        
        /** Static method for reading cluster lists back in.
         * @param path      : path to the cluster file
         * @param list      : contains the clusters after reading
         * @param source    : contains the source file attribute's value after reading
         * @param parameters: contains the run's parameter list after reading
         * @param var_names : contains the list of variables used after reading
         */
        static
        typename ClusterList<T>::ptr
        read(const string& path );
        
        /** Prints the cluster list out to console
         */
        void print();
        /** Counts the number of points in all the clusters. Must be
         * equal to the number of points in the feature space.
         */
        
#pragma mark -
#pragma mark Miscellaneous
        
        /** Sets the cluster property of all points of the given
         * featurespace to NULL
         * @param featurespace
         * TODO: find a better place for this!
         */
        static
        void
        reset_clustering( FeatureSpace<T> *fs );
        
        /** Counts the number of points in all clusters and checks
         * if the number is equal to featureSpace->size()
         */
        void sanity_check( const FeatureSpace<T> *fs );
        
#pragma mark -
#pragma mark Debugging

        /** Used for analyzing the boundaries and signal correlation
         */
        void write_boundaries(const WeightFunction<T> *weight_function,
                              FeatureSpace<T> *fs,
                              PointIndex<T> *index,
                              const vector<T> &resolution );
#if WRITE_MODES
    protected:
        vector< vector<T> >         m_trajectory_endpoints;
        vector<size_t>              m_trajectory_lengths;
    public:
        const vector< vector<T> > &trajectory_endpoints() { return m_trajectory_endpoints; }
        const vector<size_t> &trajectory_lengths() { return m_trajectory_lengths; }
#endif

    };
};
    
#endif
