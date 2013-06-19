#ifndef _M3D_ClusterList_Impl_H_
#define _M3D_ClusterList_Impl_H_

#include <algorithm>
#include <sstream>
#include <netcdf>

#include <boost/tokenizer.hpp>
#include <boost/algorithm/string/predicate.hpp>

#include <stdlib.h>

namespace m3D {

	using namespace std;
    using namespace netCDF;
    using namespace cfa::utils::console;
    using namespace cfa::utils::timer;
    using namespace cfa::utils::vectors;
    using namespace cfa::utils::visit;
	using cfa::meanshift::Point;
	using cfa::meanshift::FeatureSpace;
	using cfa::meanshift::PointIndex;
	using cfa::meanshift::KNNSearchParams;
	using cfa::meanshift::RangeSearchParams;
	using cfa::meanshift::SearchParameters;

#pragma mark -
#pragma mark Macros
    
	#define sup( v1,v2 ) (v1 > v2 ? v1:v2)
	#define inf( v1,v2 ) (v1 < v2 ? v1:v2)
    
#pragma mark -
#pragma mark Accessing the list

    template <typename T>
    size_t
    ClusterList<T>::size()
    {
        return clusters.size();
    }
    
    template <typename T>
    typename Cluster<T>::ptr
    ClusterList<T>::operator[] (size_t index)
    {
        return clusters[index];
    }

    
#pragma mark -
#pragma mark Adding / Removing points
    
    template <typename T> 
    void 
    ClusterList<T>::add_trajectory( const typename Point<T>::ptr x,
                                    Trajectory *trajectory,
                                    FeatureSpace<T> *fs )
    {
        vector<T> mode = trajectory->back();
        
        //cout << "Adding mode " << mode << " to cluster list" << endl;
        
#if WRITE_MODES
        m_trajectory_endpoints.push_back( mode );
        m_trajectory_lengths.push_back( trajectory->size() );
#endif
        
        // Round the mode to resolution
        
        fs->round_to_resolution( mode );
        
        // Already have this mode?

        typename ClusterMap::iterator cm_node = m_cluster_map.find( mode );
        
        typename Cluster<T>::ptr cluster;
        
        if ( cm_node != m_cluster_map.end() )
        {
            // We have it
            cluster = cm_node->second;
        }
        else
        {
            // Don't have it, create it fresh
            cluster = new Cluster<T>(mode);
            
            m_cluster_map[mode] = cluster;
            
            clusters.push_back( cluster );
        }
        
        cluster->add_point(x);
    };
    
    template <typename T>
    void
    ClusterList<T>::apply_size_threshold( unsigned int min_cluster_size, const bool& show_progress )
    {
        boost::progress_display *progress = NULL;
        
        if ( show_progress )
        {
            cout << endl << "Applying size threshold of " << min_cluster_size << " ... " << endl;
            
            start_timer();
            
            progress = new boost::progress_display(clusters.size());
        }
        
        size_t axe_count = 0;
        
        if ( min_cluster_size > 1 )
        {
            typename Cluster<T>::list::iterator it;
            
            for ( it = clusters.begin(); it != clusters.end(); )
            {
                progress->operator++();
                
                typename Cluster<T>::ptr sc = *it;
                
                if ( sc->points.size() < min_cluster_size )
                {
                    it = clusters.erase( it );
                    
                    axe_count++;
                }
                else
                {
                    it++;
                }
            }
        }
        
        if ( show_progress )
        {
            cout << endl << "done. (Removed " << axe_count << " objects in " << stop_timer() << "s)" << endl;
            
            delete progress;
        }
    }
    
    template <typename T>
    void
    ClusterList<T>::aggregate_with_parent_clusters( const ClusterList<T> &parent_clusters )
    {
        vector< vector<T> > used_modes;
        
        typename Cluster<T>::list::iterator sci;
        
        for ( size_t sc_index = 0; sc_index < clusters.size(); sc_index++ )
        {
            size_t point_count = 0;
            
            typename Cluster<T>::ptr sc = clusters[sc_index];
            
            typename Point<T>::list aggregated_points;
            
            size_t matching_cluster_count = 0;
            
            for ( size_t pci = 0; pci < parent_clusters.clusters.size(); pci++ )
            {
                typename Cluster<T>::ptr parent_cluster = parent_clusters.clusters[pci];
                
                // If the parent's mode is this point's value, add it's points
                // to the list
                
                for ( size_t sc_point_index = 0; sc_point_index < sc->points.size(); sc_point_index++ )
                {
                    typename Point<T>::ptr sc_point = sc->points[sc_point_index];
                
                    if ( parent_cluster->mode == sc_point->values )
                    {
                        typename vector< vector<T> >::iterator got_mode = find( used_modes.begin(), used_modes.end(), parent_cluster->mode );
                        
                        if ( got_mode != used_modes.end() )
                        {
                            cout << "Assigning cluster #" << pci << " at " << parent_cluster->mode << " to supercluster at " << sc->mode << " AGAIN" << endl;
                        }
                        
                        used_modes.push_back( parent_cluster->mode );
                        
                        matching_cluster_count++;
                        
                        // replace the 'cluster' pointer in the points with
                        // the supercluster and add those points to the list
                        // of aggregated points
                        
                        // cout << "Point matches mode of cluster at " << parent_cluster->mode << " (" << parent_cluster->points.size() << " points.)" << endl;
                        
                        size_t parent_point_count = 0;
                        
                        for ( size_t pc_point_index = 0; pc_point_index < parent_cluster->points.size(); pc_point_index++ )
                        {
                            typename Point<T>::ptr pc_point = parent_cluster->points[ pc_point_index ];
                            
                            aggregated_points.push_back( pc_point );
                            
                            pc_point->cluster = sc;
                            
                            parent_point_count++;
                            
                            point_count++;
                        }
                        
                        // cout << "Added " << parent_point_count << " points to supercluster at " << sc->mode << endl;
                    }
                }
            }
            
            sc->points = aggregated_points;
        }
    }

    template <typename T> 
    void 
    ClusterList<T>::write( const std::string& path )
    {
        try
        {
            // use NetCDF C - API (instead of C++) because the C++ API has no
            // support for variable length arrays :(
            
            bool fileExists = this->ncFile != NULL;
            
            NcFile *file = NULL;
            
            try
            {
                if ( fileExists )
                {
                    file = this->ncFile;
                }
                else
                {
                    file = new NcFile( path, NcFile::replace );
                }
            }
            catch ( const netCDF::exceptions::NcException &e )
            {
                cerr << "Exception opening file " << path << " for writing : " << e.what() << endl;
                exit(-1);
            }
            
            // Create dimensions
            
            NcDim dim,spatial_dim;
            
            if ( fileExists )
            {
                dim = file->getDim("featurespace_dim");
                
                spatial_dim = file->getDim("spatial_dim");
            }
            else
            {
                dim = file->addDim("featurespace_dim", (int) this->feature_variables.size() );
            
                spatial_dim = file->addDim("spatial_dim", (int) this->dimensions.size() );
                
                // write featurespace_dimensions attribute
                
                vector<string> fs_dims;
                
                for (size_t di=0; di<this->dimensions.size(); di++)
                {
                    fs_dims.push_back(this->dimensions[di].getName());
                }
                
                file->putAtt("featurespace_dimensions", to_string(fs_dims) );
                
                // copy dimensions
                
                for (size_t di=0; di<this->dimensions.size(); di++)
                {
                    NcDim d = this->dimensions[di];
                    
                    file->addDim(d.getName(), d.getSize());
                }
                
                // Create dummy variables, attributes and other meta-info
                
                file->putAtt( "num_clusters", ncInt, (int) clusters.size() );
                
                file->putAtt( "source", this->source_file );

                // compile a list of the feature space variables used,
                // including the spatial dimensions
                
                vector<string> variable_names;
                
                // feature variables
                
                for ( size_t i=0; i < this->feature_variables.size(); i++ )
                {
                    NcVar var = this->feature_variables[i];

                    // append to list
                    
                    variable_names.push_back(var.getName());
                    
                    NcDim *dim = NULL;

                    // Copy data (in case of dimension variables)
                    // exploiting once more the fact, that dimension variables
                    // have by convention the same name as the dimension
                    
                    for (size_t di=0; di<this->dimensions.size() && dim==NULL; di++)
                    {
                        NcDim d = this->dimensions[di];
                        dim = (d.getName() == var.getName()) ? &d : NULL;
                    }
                    
                    // create a dummy variable
                    
                    NcVar dummyVar;
                    
                    if (dim != NULL)
                    {
                        dummyVar = file->addVar( var.getName(), var.getType(), *dim);
                        
                        T *data = (T*)malloc(sizeof(T) * dim->getSize());
                        
                        var.getVar(data);
                        
                        dummyVar.putVar(data);
                        
                        delete data;
                    }
                    else
                    {
                        dummyVar = file->addVar( var.getName(), var.getType(), spatial_dim );
                    }
                    
                    // Copy attributes
                    
                    map< string, NcVarAtt > attributes = var.getAtts();
                    
                    map< string, NcVarAtt >::iterator at;
                    
                    for ( at = attributes.begin(); at != attributes.end(); at++ )
                    {
                        NcVarAtt a = at->second;
                        
                        size_t size = a.getAttLength();
                        
                        void *data = (void *)malloc( size );
                        
                        a.getValues( data );
                        
                        NcVarAtt copy = dummyVar.putAtt( a.getName(), a.getType(), size, data );
                        
                        free(data);
                    }
                    
                }
                
                file->putAtt("featurespace_variables", to_string(variable_names));
            }
            
            // Add tracking meta-info
            
            if ( this->tracking_performed )
            {
                file->putAtt( "tracking_performed", "yes" );
                file->putAtt( "tracked_ids", to_string( this->tracked_ids ) );
                file->putAtt( "new_ids", to_string( this->new_ids ) );
                file->putAtt( "dropped_ids", to_string( this->dropped_ids ) );
            }
            
            // Add cluster dimensions and variables
            
            for ( size_t ci = 0; ci < clusters.size(); ci++ )
            {
                // Create a dimension
                
                stringstream dim_name(stringstream::in | stringstream::out);
                
                dim_name << "cluster_dim_" << ci;
                
                NcDim cluster_dim;
                
                if ( fileExists )
                {
                    cluster_dim = file->getDim( dim_name.str() );
                }
                else
                {
                    cluster_dim = file->addDim( dim_name.str(), clusters[ci]->points.size() );
                }
                
                // Create variable
                
                stringstream var_name(stringstream::in | stringstream::out);
                
                var_name << "cluster_" << ci;
                
                vector<NcDim> dims(2);
                dims[0] = cluster_dim;
                dims[1] = dim;
                
                NcVar var;
                
                if ( fileExists )
                {
                    var = file->getVar( var_name.str() );
                }
                else
                {
                    var = file->addVar( var_name.str(), ncDouble, dims );
                }
                
                // size
                
                var.putAtt( "size", ncInt, (int) clusters[ci]->points.size() );
                
                // id
                
                unsigned long long cid = (unsigned long long) clusters[ci]->id;
                
                var.putAtt( "id", ncInt64, cid );
                
                // mode

                string mode = to_string( clusters[ci]->mode );

                var.putAtt( "mode", mode );

                // Write the clusters away point by point
                
                // index and counter
                vector<size_t> index(2,0);
                vector<size_t> count(2,0);
                count[0] = 1;
                count[1] = dim.getSize();

                // iterate over points
                for ( size_t pi = 0; pi < clusters[ci]->points.size(); pi++ )
                {
                    Point<T> *p = clusters[ci]->points[pi];
                    
                    double data[ dim.getSize() ];
                    
                    for ( size_t di = 0; di < dim.getSize(); di++ )
                    {
                        data[di] = (double)p->values[di];
                    }
                    index[0] = pi;
                    var.putVar(index, count, &data[0] );
                }
            }
        }
        catch (const std::exception &e)
        {
            std::cerr << "Exception while writing cluster file: " << e.what() << endl;
        }
    }

    template <typename T> 
    typename ClusterList<T>::ptr
    ClusterList<T>::read(const std::string& path)
    {
        // meta-info
        
        vector<NcVar>                   feature_variables;
        vector<NcDim>                   dimensions;
        string                          source_file;
        typename Cluster<T>::list       list;
        NcFile                          *file = NULL;

        // TODO: let this exception go up
        try
        {
            file = new NcFile( path, NcFile::write );
        }
        catch ( const netCDF::exceptions::NcException &e )
        {
            cerr << "Could not open file " << path << " : " << e.what() << endl;
            exit( -1 );
        }
        
        try
        {
            // Read the dimensions

            NcDim fs_dim = file->getDim( "featurespace_dim" );
            
            // Read the dimensions attribute
            
            string dim_str;
            
            file->getAtt("featurespace_dimensions").getValues(dim_str);
            
            // Fill the dimensions vector from that
            
            vector<string> fs_dim_names = from_string<string>(dim_str);
            
            for (size_t di=0; di < fs_dim_names.size(); di++)
            {
                NcDim d = file->getDim(fs_dim_names[di]);
                
                dimensions.push_back(d);
            }

            // Read global attributes
            
            file->getAtt("source").getValues( source_file );
            
            int number_of_clusters;
            
            file->getAtt("num_clusters").getValues( &number_of_clusters );
            
            // Read the feature-variables
            
            multimap<string,NcVar> vars = file->getVars();
            
            typename multimap<string,NcVar>::iterator vi;
            
            for ( vi = vars.begin(); vi != vars.end(); vi++ )
            {
                if (! boost::starts_with( vi->first, "cluster_"))
                {
                    feature_variables.push_back( vi->second );
                }
            }
            
            // Read clusters one by one
            
            for ( size_t i = 0; i < number_of_clusters; i++ )
            {
                // cluster dimension
                
                stringstream dim_name(stringstream::in | stringstream::out);
                
                dim_name << "cluster_dim_" << i;
                
                NcDim cluster_dim = file->getDim( dim_name.str().c_str() );
                
                // Read the variable
                
                stringstream var_name(stringstream::in | stringstream::out);
                
                var_name << "cluster_" << i;
                
                NcVar var = file->getVar( var_name.str().c_str() );
                
                // Decode mode
                
                typedef boost::tokenizer<boost::char_separator<char> > tokenizer;
                
                boost::char_separator<char> sep(",");
                
                std::string mode_str;
                
                var.getAtt("mode").getValues(mode_str);
                
                boost::replace_all(mode_str, "(", "" );
                boost::replace_all(mode_str, ")", "" );
                
                tokenizer tokens( mode_str, sep );
                
                vector<T> mode;
                
                for ( tokenizer::iterator tok_iter = tokens.begin(); tok_iter != tokens.end(); ++tok_iter )
                {
                    string token = *tok_iter;

                    mode.push_back( (T)atof( token.c_str() ) );
                }
                
                // Create a cluster object
                
                typename Cluster<T>::ptr cluster = new Cluster<T>( mode, dimensions.size() );
                
                // read cluster id
                
                var.getAtt("id").getValues( &(cluster->id) );
                
                // iterate over the data

                // Read the points, one by one
                
                vector<size_t> index(2,0);
                vector<size_t> count(2,0);
                count[0] = 1;
                count[1] = fs_dim.getSize();
                
                for ( size_t point_index = 0; point_index < cluster_dim.getSize(); point_index++ )
                {
                    // Allocate data
                    
                    T data[fs_dim.getSize()];
                    
                    // set index up and read
                    
                    index[0] = point_index;
                    
                    var.getVar(index,count,&data[0]);
                    
                    // copy data over to vectors
                    
                    vector<T> coordinate(dimensions.size(),0);
                    
                    vector<T> values(fs_dim.getSize(),0);
                    
                    for ( size_t i=0; i<fs_dim.getSize(); i++)
                    {
                        values[i] = data[i];
                        
                        if ( i < dimensions.size() )
                        {
                            coordinate[i] = values[i];
                        }
                    }
                    
                    // Create a point and add it to the cluster
                    
                    Point<T> *p = PointFactory<T>::get_instance()->create( coordinate, values );
                    
                    cluster->add_point(p);
                }
                
                list.push_back( cluster );
            }
        }
        catch (const std::exception &e)
        {
            cerr << e.what() << endl;
        }
        
        ClusterList<T>::ptr cl = new ClusterList<T>( list, dimensions, feature_variables, source_file );
        
        cl->ncFile = file;
        
        return cl;
    }
    
    template <typename T>
    void
    ClusterList<T>::print()
    {
        for ( size_t ci = 0; ci < clusters.size(); ci++ )
        {
            typename Cluster<T>::ptr c = clusters[ci];
            
            cout << "Cluster #" << ci << " at " << c->mode << " (" << c->points.size() << " points.)" << endl;
        }

    }
    
#pragma mark -
#pragma mark Clustering by Graph Theory
    
    template <typename T>
    void
    ClusterList<T>::aggregate_cluster_graph( const WeightFunction<T> *weight_function, FeatureSpace<T> *fs, const vector<T> &resolution, const bool& show_progress )
    {
        // PointIndex<T>::write_index_searches = true;
        
        size_t cluster_id = 0;
        
        boost::progress_display *progress = NULL;
        
        if (show_progress)
        {
            cout << endl << "Analysing meanshift vector graph ...";
            start_timer();
            progress = new boost::progress_display( fs->points.size() );
        }
        
        // Create own index for KNN search
        
        PointIndex<T> *index = PointIndex<T>::create( fs );
        
        for ( size_t i = 0; i < fs->points.size(); i++ )
        {
            if (show_progress)
            {
                progress->operator++();
            }
            
            M3DPoint<T> *p = (M3DPoint<T> *) fs->points[i];
            
            // Skip points that have been assigned already
            
            if ( p->cluster != NULL ) continue;
            
            // Skip points, that do not belong to the original
            // feature-space, but were created by scale-space
            // filtering etc.
            
            if ( ! p->isOriginalPoint()) continue;
            
            typename Point<T>::list nodes;
            
            bool have_predecessor = true;
            
            typename M3DPoint<T>::ptr current_point = p;

            do
            {
                // Guard against cycles
                
                typename Point<T>::list::const_iterator f = find( nodes.begin(), nodes.end(), current_point );
                
                if ( f != nodes.end() ) break;
                
                // Add current point to the list and move on
                
                nodes.push_back( current_point );

                M3DPoint<T> *predecessor = (M3DPoint<T> *) predecessor_of( fs, index, resolution, weight_function, current_point );
                
                if ( predecessor->cluster != NULL )
                {
                    // add all points so far to the existing cluster
                    // and exit
                    
                    typename Cluster<T>::ptr c = predecessor->cluster;
                    
                    // Do a sanity check
                    
                    typename Cluster<T>::list::const_iterator fi = find( this->clusters.begin(), this->clusters.end(), c );
                    
                    assert( fi != this->clusters.end() );
                    
                    c->add_points( nodes );
                    
                    nodes.clear();
                    
                    break;
                }
                
                have_predecessor = ( predecessor != current_point );
                
                if ( have_predecessor )
                {
                    current_point = predecessor;
                }
                
            } while ( have_predecessor );
            
            if ( nodes.size() > 0 )
            {
                // Create a cluster and add it to the list
                
                typename Cluster<T>::ptr cluster = new Cluster<T>( nodes.back()->values, this->dimensions.size() );
                
                cluster->add_points( nodes, m_use_original_points_only );
                
                cluster->id = cluster_id++;
                
                this->clusters.push_back( cluster );
            }
        }
        
        delete index;
        
        if ( show_progress )
        {
            cout << "done. (Found " << clusters.size() << " clusters in " << stop_timer() << "s)" << endl;
            delete progress;
        }
        
        // PointIndex<T>::write_index_searches = false;
    }
    
    template <typename T>
    typename Point<T>::ptr
    ClusterList<T>::predecessor_of( FeatureSpace<T> *fs,
                                    PointIndex<T> *index,
                                    const vector<T> &resolution,
                                    const WeightFunction<T> *weight_function,
                                    typename Point<T>::ptr p )
    {
        typename Point<T>::ptr result = p;
        
        // Search coordinate
        vector<T> x = p->values + p->shift;
        
        // Find dim^2 closest neighbours
        
        KNNSearchParams<T> knn( resolution.size() * resolution.size() );
        
        typename Point<T>::list *neighbours = index->search( x, &knn );
        
        // In case the search turns up more than one point,
        // find the strongest point
        
        T max_dist = m_predecessor_maxdistance_gridpoints * vector_norm( fs->spatial_component(resolution) );

        T max_value = std::numeric_limits<T>::min();
        
        for ( size_t i=0; i < neighbours->size(); i++ )
        {
            typename Point<T>::ptr n = neighbours->at(i);
            
            // skip yourself and points too far away
            
            if ( n == p ) continue;
            
            // too far?
            
            T distance = vector_norm( p->coordinate - n->coordinate );
            
            if ( distance > max_dist ) continue;
            
            // Get weight function response
            
            T weight_function_response = weight_function->operator()( p->gridpoint, p->values );
            
            if ( weight_function_response > max_value )
            {
                max_value = weight_function_response;
                
                result = n;
            }
        }
        
        delete neighbours;
        
        return result;
    };
    

    template <typename T>
    typename Cluster<T>::list
    ClusterList<T>::neighbours_of( typename Cluster<T>::ptr cluster, PointIndex<T> *index, const vector<T> &resolution, const WeightFunction<T> *weight_function )
    {
        typename Cluster<T>::list neighbouring_clusters;
        
        RangeSearchParams<T> search_params( resolution );
        
        typename Point<T>::list::const_iterator pi;
        
        for ( pi = cluster->points.begin(); pi != cluster->points.end(); pi++ )
        {
        	typename M3DPoint<T>::ptr p = (M3DPoint<T> *) *pi;
            
            typename Point<T>::list *neighbours = index->search( p->values, &search_params );
            
            typename Point<T>::list::const_iterator ni;
            
            for ( ni = neighbours->begin(); ni != neighbours->end(); ni++ )
            {
                M3DPoint<T> *n = (M3DPoint<T> *) *ni;
                
                // Exclude points, that have not been clustered.
                // This can happen because scale-space filtering
                // creates new points, but those are not associated
                // with clusters in later steps
                if ( n->cluster == NULL ) continue;
                
                if ( n->cluster != p->cluster )
                {
                    typename Cluster<T>::list::const_iterator fi = find( neighbouring_clusters.begin(), neighbouring_clusters.end(), n->cluster );
                    
                    if ( fi == neighbouring_clusters.end() )
                    {
                        neighbouring_clusters.push_back( n->cluster );
                    }
                }
            }
            
            delete neighbours;
        }
        
        return neighbouring_clusters;
    }
    
    template <typename T>
    void
    ClusterList<T>::get_boundary_points(typename Cluster<T>::ptr c1,
                                        typename Cluster<T>::ptr c2,
                                        typename Point<T>::list &boundary_points,
                                        PointIndex<T> *index,
                                        const vector<T> &resolution )
    {
        RangeSearchParams<T> search_params( m_neighbourhood_range_search_multiplier * resolution );
        
        typename Point<T>::list::const_iterator pi;
        
        for ( pi = c1->points.begin(); pi != c1->points.end(); pi++ )
        {
            typename Point<T>::ptr p = *pi;
            
            typename Point<T>::list *neighbours = index->search( p->values, &search_params );
            
            typename Point<T>::list::const_iterator ni;
            
            for ( ni = neighbours->begin(); ni != neighbours->end(); ni++ )
            {
                typename M3DPoint<T>::ptr n = (M3DPoint<T> *) *ni;
                
                if ( n->cluster == c2 )
                {
                    // check every time to avoid double adding
                    
                    typename Point<T>::list::const_iterator fi = find( boundary_points.begin(), boundary_points.end(), n );
                    
                    if ( fi == boundary_points.end() )
                    {
                        boundary_points.push_back( n );
                    }
                    
                    fi = find( boundary_points.begin(), boundary_points.end(), p );
                    
                    if ( fi == boundary_points.end() )
                    {
                        boundary_points.push_back( p );
                    }
                }
            }
            
            delete neighbours;
        }

        for ( pi = c2->points.begin(); pi != c2->points.end(); pi++ )
        {
            typename Point<T>::ptr p = *pi;
            
            typename Point<T>::list *neighbours = index->search( p->values, &search_params );
            
            typename Point<T>::list::const_iterator ni;
            
            for ( ni = neighbours->begin(); ni != neighbours->end(); ni++ )
            {
            	typename M3DPoint<T>::ptr n = (M3DPoint<T> *) *ni;
                
                if ( n->cluster == c1 )
                {
                    // check every time to avoid double adding
                    
                    typename Point<T>::list::const_iterator fi = find( boundary_points.begin(), boundary_points.end(), n );
                    
                    if ( fi == boundary_points.end() )
                    {
                        boundary_points.push_back( n );
                    }
                    
                    fi = find( boundary_points.begin(), boundary_points.end(), p );
                    
                    if ( fi == boundary_points.end() )
                    {
                        boundary_points.push_back( p );
                    }
                }
            }
            
            delete neighbours;
        }
    };
    

    
    // Couple of macros
    
    template <typename T>
    bool
    ClusterList<T>::should_merge_neighbouring_clusters( typename Cluster<T>::ptr c1,
                                                        typename Cluster<T>::ptr c2,
                                                        const WeightFunction<T> *weight_function,
                                                        PointIndex<T> *index,
                                                        const vector<T> &resolution,
                                                        const double &drf_threshold )
    {
        bool should_merge = false;
        
        // Collect the points on each side of the boundary
        
        typename Point<T>::list boundary_points;
        
        this->get_boundary_points( c1, c2, boundary_points, index, resolution );
        
        // if no common boundary exists, the point is moot
        
#if DEBUG_CLUSTER_MERGING_DECISION
        std::cout << "===================================================================" << std::endl;
        std::cout << "comparing #" << c1->id << " = " << c1->mode << " (" << c1->size() << " points)" << std::endl;
        std::cout << "with      #" << c2->id << " = " << c2->mode << " (" << c2->size() << " points)" << std::endl;
        
#endif
        if ( boundary_points.size() > 0 )
        {
            // the classification of dynamic range of the signal on each side
            // of the boundary
            
            T c1_dyn_range_factor = dynamic_range_factor( c1, boundary_points, weight_function );
            
            T c2_dyn_range_factor = dynamic_range_factor( c2, boundary_points, weight_function );
            
            bool dynamic_range_test = sup( c1_dyn_range_factor, c2_dyn_range_factor ) >= drf_threshold;
            
#if DEBUG_CLUSTER_MERGING_DECISION
            cout << "\tc1-drf = " << c1_dyn_range_factor << " , c2-drf = " << c2_dyn_range_factor << endl;
#endif
            
            // Indicators for merging are: high dynamic range on both sides of the boundary
            // as well as high signal variablility in both

            should_merge =  dynamic_range_test;
        }
#if DEBUG_CLUSTER_MERGING_DECISION
        else
        {
            cout << "no common boundary" << endl;
        }
#endif 
        
#if DEBUG_CLUSTER_MERGING_DECISION
        std::cout << "\t==> should merge = " << (should_merge ? "yes" : "no") << std::endl;
        std::cout << "===================================================================" << std::endl;
#endif

        return should_merge;
    }
    
    template <typename T>
    void
    ClusterList<T>::write_boundaries(const WeightFunction<T> *weight_function,
                                     FeatureSpace<T> *fs,
                                     PointIndex<T> *index,
                                     const vector<T> &resolution )
    {
        // collate the data
        
        typedef vector< typename Point<T>::list > boundaries_t;
        
        boundaries_t boundaries;
        
        
        typedef vector< std::string > boundary_key_t;
        
        boundary_key_t boundary_keys;
        
        
        vector<T> var_c1, var_c2, var_boundary;
        
        vector<T> range_factor_c1, range_factor_c2;
        
        vector< typename Cluster<T>::id_t >  cluster_index_1, cluster_index_2;
        
        typename Cluster<T>::list::const_iterator ci;

        for ( ci = clusters.begin(); ci != clusters.end(); ci++ )
        {
            typename Cluster<T>::ptr c = *ci;
            
            typename Cluster<T>::list neighbours = neighbours_of( c, index, resolution, weight_function );
            
            if ( neighbours.size() > 0 )
            {
                // go over the list of neighbours and find candidates for merging
                
                typename Cluster<T>::list::const_iterator ni;
                
                for ( ni = neighbours.begin(); ni != neighbours.end(); ni++ )
                {
                    typename Cluster<T>::ptr n = *ni;

                    std::string key = boost::lexical_cast<string>(inf(c->id,n->id)) + "-" + boost::lexical_cast<string>(sup(c->id,n->id));
                    
                    typename boundary_key_t::const_iterator fi = find( boundary_keys.begin(), boundary_keys.end(), key );
                    
                    if ( fi == boundary_keys.end() )
                    {
                        boundary_keys.push_back( key );
                        
                        typename Point<T>::list boundary_points;
                        
                        this->get_boundary_points( c, n, boundary_points, index, resolution );
                        
                        if ( boundary_points.size() == 0 ) continue;
                            
                        
                        boundaries.push_back( boundary_points );
                        

                        var_boundary.push_back( relative_variability( weight_function, boundary_points ) );
                        
                        var_c1.push_back( relative_variability( weight_function, c->points ) );

                        var_c2.push_back( relative_variability( weight_function, n->points ) );

                        
                        range_factor_c1.push_back( dynamic_range_factor( c, boundary_points, weight_function ) );

                        range_factor_c2.push_back( dynamic_range_factor( n, boundary_points, weight_function ) );
                        
                        
                        cluster_index_1.push_back(c->id);
                        
                        cluster_index_2.push_back(n->id);
                    }
                }
            }
        }
        
        for ( size_t index = 0; index < boundaries.size(); index++ )
        {
            typename Point<T>::list b = boundaries[index];
            std::string fn = fs->filename() + "_boundary_" + boost::lexical_cast<string>(index) + ".vtk";
            boost::replace_all( fn, "/", "_" );
            boost::replace_all( fn, "..", "" );
            cfa::utils::VisitUtils<T>::write_pointlist_vtk(fn, &b, fs->coordinate_system->size());
        }
    
        std::string fn = fs->filename() + "_boundary_correlations.txt";
        std::ofstream f( fn.c_str() );
        f << "#\t"
        << "c1\t"
        << "c2\t"
//        << "var_c1\t"
//        << "var_c2\t"
//        << "var_b\t"
        << "drf_1\t"
        << "drf_2\t"
	  << std::endl;
        for ( size_t index = 0; index < boundaries.size(); index++ )
        {
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

    
    template <typename T>
    T
    ClusterList<T>::relative_variability( const WeightFunction<T> *weight_function, const typename Point<T>::list &points )
    {
        // calculate mean
        
        T mean = 0.0;
        
        typename Point<T>::list::const_iterator pi;
        
        for ( pi = points.begin(); pi != points.end(); pi++ )
        {
            typename Point<T>::ptr p = *pi;
            
            T value = weight_function->operator()(p->coordinate,p->values);
            
            mean += value;
        }
        
        mean /= ((T)points.size());
        
        // calculate standard deviation
        
        T standard_deviation = 0.0;
        
        for ( pi = points.begin(); pi != points.end(); pi++ )
        {
            typename Point<T>::ptr p = *pi;
            
            T value = weight_function->operator()(p->coordinate,p->values);
        
            standard_deviation += ( value - mean ) * ( value - mean );
        }
        
        standard_deviation = sqrt( standard_deviation / ((T)points.size()) );
        
        // relative deviation
        
        T cv = standard_deviation / mean;
        
#if DEBUG_CLUSTER_MERGING
        std::cout << "Sample of " << points.size() << " points in variable index " << variable_index
             << " has m=" << mean << ", s=" << standard_deviation << ", cv=" << cv << std::endl;
#endif
        return cv;
    }

    template <typename T>
    T
    ClusterList<T>::dynamic_range_factor( typename Cluster<T>::ptr cluster,
    									  const typename Point<T>::list &points,
    									  const WeightFunction<T> *weight_function )
    {
        // Obtain the dynamic range if the list
        
        T points_lower_bound, points_upper_bound;
        
        ClusterList<T>::dynamic_range( points, weight_function, points_lower_bound, points_upper_bound );
        
        T cluster_lower_bound, cluster_upper_bound;
        
        ClusterList<T>::dynamic_range( cluster, weight_function, cluster_lower_bound, cluster_upper_bound );

        // Compare the two ranges
        
//        T p_range = points_upper_bound - points_lower_bound;
//        
//        T c_range = cluster_upper_bound - cluster_lower_bound;
//        
//        T factor = ( p_range == 0 || c_range == 0 ) ? 0.0 : inf(p_range,c_range) / sup(p_range,c_range);

        // Correction: compare upper values only and see if they're in the same
        // ballpark
        
        T factor = inf( points_upper_bound, cluster_upper_bound ) / sup( points_upper_bound, cluster_upper_bound );
        
#if DEBUG_CLUSTER_MERGING
        std::cout << "-- Dynamic Range Comparison " << std::endl;
        std::cout << "\tcluster at " << cluster->mode << "(" << cluster->size() << " points) compared to boundary line (" << points.size() << " points)" << std::endl;
        std::cout << "\t\tcluster  dynamic range = [" << cluster_lower_bound << "," << cluster_upper_bound << "]" << std::endl;
        std::cout << "\t\tboundary dynamic range = [" << points_lower_bound << "," << points_upper_bound << "]" << std::endl;
        std::cout << "\tfactor = " << factor << std::endl;
#endif
        return factor;
    }

    template <typename T>
    typename Cluster<T>::ptr
    ClusterList<T>::merge_clusters( typename Cluster<T>::ptr c1, typename Cluster<T>::ptr c2 )
    {
        vector<T> merged_mode = (T)0.5 * ( c1->mode + c2->mode );
        
        typename Cluster<T>::ptr merged_cluster = new Cluster<T>( merged_mode, this->dimensions.size() );
        
        merged_cluster->add_points( c1->points );
        
        merged_cluster->add_points( c2->points );
        
        merged_cluster->id = c1->id;
        
        typename Cluster<T>::list::iterator fi = find( clusters.begin(), clusters.end(), c1 );
        
        clusters.erase( fi );

        delete c1;
        
        fi = find( clusters.begin(), clusters.end(), c2 );
        
        clusters.erase( fi );

        delete c2;

        clusters.push_back( merged_cluster );
        
        return merged_cluster;
    }

    
    template <typename T>
    void
    ClusterList<T>::aggregate_clusters_by_boundary_analysis( const WeightFunction<T> *weight_function,
                                                             PointIndex<T> *index,
                                                             const vector<T> &resolution,
                                                             const double &drf_threshold,
                                                             const bool &show_progress )
    {
        // No merging
        if (drf_threshold==1.0) return;
        
        // Provide a second index for the merge examination. They use a smaller
        // resolution and would trigger re-calculation of the index every other
        // step. It's cheaper to provide them with their own index
        
        PointIndex<T> *index2 = PointIndex<T>::create( (FeatureSpace<T> *) index->feature_space() );
        
        ConsoleSpinner *spinner = NULL;

        if ( show_progress )
        {
            cout << endl << "Aggregating clusters into objects ...";
            spinner = new ConsoleSpinner();
            start_timer();
        }
        
        bool had_merge = false;
        
        size_t merge_count = 0;
        

        typedef vector< std::string > boundary_key_t;
        
        boundary_key_t boundary_keys;
        
        do
        {
            had_merge = false;
            
            // Start at the top and work your way down
            
            typename Cluster<T>::list::const_iterator ci;
            
            for ( ci = clusters.begin(); ci != clusters.end() && ! had_merge; ci++ )
            {
                typename Cluster<T>::ptr c = *ci;
                
                typename Cluster<T>::list neighbours = neighbours_of( c, index, resolution, weight_function );
                
                if ( neighbours.size() > 0 )
                {
                    // go over the list of neighbours and find candidates for merging
                    
                    typename Cluster<T>::list::const_iterator ni;
                    
                    for ( ni = neighbours.begin(); ni != neighbours.end(); ni++ )
                    {
                        typename Cluster<T>::ptr n = *ni;
                        
                        std::string key = boost::lexical_cast<string>(inf(c->id,n->id)) + "-" + boost::lexical_cast<string>(sup(c->id,n->id));
                        
                        typename boundary_key_t::const_iterator fi = find( boundary_keys.begin(), boundary_keys.end(), key );
                        
                        if ( fi == boundary_keys.end() )
                        {
                            boundary_keys.push_back( key );
                        
                            if ( should_merge_neighbouring_clusters( c, n, weight_function, index2, resolution, drf_threshold ) )
                            {
                                merge_clusters( c, n );
                            
                                had_merge = true;
                            
                                merge_count++;
                            
                                break;
                            }
                        }
                    }
                }
                
                // break out and start over
                if ( had_merge ) break;
            }
            
        } while (had_merge);
        
        
        if ( show_progress )
        {
            delete spinner;
            cout << "done. (Found " << clusters.size() << " objects in " << stop_timer() << "s, " << merge_count << " merges)" << endl;
        }
        
        delete index2;
    };
    
    template <typename T>
    void
    ClusterList<T>::erase_identifiers()
    {
        for ( size_t i=0; i < clusters.size(); i++ )
        {
            clusters[i]->id = Cluster<T>::NO_ID;
        }
    };
    
    template <typename T>
    void
    ClusterList<T>::retag_identifiers()
    {
        for ( size_t i=0; i < clusters.size(); i++ )
        {
            clusters[i]->id = i;
        }
    };
    
    
    template <typename T>
    void
    ClusterList<T>::reset_clustering( FeatureSpace<T> *fs )
    {
        struct clear_cluster
        {
            void operator() (void *p)
            {
                static_cast< M3DPoint<T> * >(p)->cluster = NULL;
            };
        } clear_cluster;
        
        for_each( fs->points.begin(), fs->points.end(), clear_cluster );
    };
    
    template <typename T>
    void
    ClusterList<T>::sanity_check( const FeatureSpace<T> *fs )
    {
        size_t point_count = 0;
        
        for ( size_t i=0; i < clusters.size(); i++ )
        {
            point_count += clusters[i]->points.size();
        }
        
        assert( point_count == fs->size() );
    };
    
#pragma mark -
#pragma mark Dynamic Range Calculation    
    
    template <class T>
    void
    ClusterList<T>::dynamic_range( const typename Point<T>::list &points, const WeightFunction<T> *weight_function, T &lower_bound, T&upper_bound )
    {
        lower_bound = std::numeric_limits<T>::max();
        
        upper_bound = std::numeric_limits<T>::min();
        
        typename Point<T>::list::const_iterator pi;
        
        for ( pi = points.begin(); pi != points.end(); pi++ )
        {
            typename Point<T>::ptr p = *pi;
            
            T value = weight_function->operator()(p->gridpoint,p->values);
            
            if ( value < lower_bound )
            {
                lower_bound = value;
            }
            
            if ( value > upper_bound )
            {
                upper_bound = value;
            }
        }
    }
    
    template <typename T>
    void
    ClusterList<T>::dynamic_range(const typename Cluster<T>::ptr cluster,
                                  const WeightFunction<T> *weight_function,
                                  T &lower_bound,
                                  T &upper_bound)
    {
        return ClusterList<T>::dynamic_range( cluster->points, weight_function, lower_bound, upper_bound );
    }


}; //namespace

#endif
