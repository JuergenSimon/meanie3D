#ifndef M3D_TEST_FS_CLUSTERING_IMPL_H
#define M3D_TEST_FS_CLUSTERING_IMPL_H

#include <meanie3D/meanie3D.h>

#include "variable_weighed_impl.h"

template<class T>
void FSClusteringTest2D<T>::write_cloud( const NcVar &var, vector<T> mean, vector<T> deviation )
{
    using namespace netCDF;
    using namespace m3D;
    using namespace m3D::utils::vectors;
    
    // start generating random points (with random values between 0 and 1)
    
    size_t numPoints = 0;
    
    // Use a simple gaussian normal function to simulate a value distribution
    
    GaussianNormal<T> gauss;
    
    T gauss_zero = gauss( vector<T>(this->coordinate_system()->rank(),0) );
    
    typename CoordinateSystem<T>::GridPoint gridpoint = this->coordinate_system()->newGridPoint();
    
    do
    {
        // genrate a random coordinate
        
        for ( size_t d = 0; d < this->coordinate_system()->rank(); d++ )
        {
            bool valid = false;
            
            while ( !valid )
            {
                NcDim dim = this->coordinate_system()->dimensions()[d];
                
                NcVar dimVar = this->coordinate_system()->dimension_variable( dim );
                
                T min, max;
                
                dimVar.getAtt("valid_min").getValues( &min );
                
                dimVar.getAtt("valid_max").getValues( &max );
                
                float rand = box_muller( mean[d], deviation[d] );
                
                // re-transform to grid coordinates
                
                size_t n = (size_t) round( (dim.getSize()-1) * (rand - min) / ( max - min ) );
                
                if ( n < dim.getSize() )
                {
                    gridpoint[d] = n;
                    
                    valid = true;
                }
            }
        }
        
        // value goes to
        
        vector<T> x = this->coordinate_system()->newCoordinate();
        
        this->coordinate_system()->lookup( gridpoint, x );
        
        T value = (T) (FS_VALUE_MAX * gauss( x-mean )) / gauss_zero;
        
        // cout << "x=" << x << " mean=" << mean << " g(x-m)=" << value << endl;
        
        vector<size_t> gp(gridpoint.begin(), gridpoint.end());
        
        var.putVar( gp, value );
        
        numPoints++;
        
        this->m_pointCount++;
        
    } while ( numPoints < m_cloudSize );
}

template<class T>
void FSClusteringTest2D<T>::create_clouds_recursive( const NcVar &var, size_t dimensionIndex, typename CoordinateSystem<T>::GridPoint &gridpoint )
{
    using namespace netCDF;
    
    NcDim dim = var.getDim(dimensionIndex);
    
    size_t increment = this->m_division_increments[dim];
    
    if ( dimensionIndex < (var.getDimCount()-1))
    {
        for ( int index = 1; index < m_divisions; index++ )
        {
            gridpoint[dimensionIndex] = index * increment;
            
            create_clouds_recursive( var, dimensionIndex+1, gridpoint );
        }
    }
    else
    {
        for ( int index = 1; index < m_divisions; index++ )
        {
            gridpoint[dimensionIndex] = index * increment;
            
            // get the variables together and construct the cartesian coordinate
            // of the current point. If it's on the ellipse, put it in the variable
            
            vector<T> coordinate( var.getDimCount() );
            
            this->coordinate_system()->lookup( gridpoint, coordinate );
            
            cout << "\tWriting cloud at gridpoint=" <<  gridpoint << " coordinate=" << coordinate << " deviation=" << m_deviation << endl;
            
            write_cloud( var, coordinate, m_deviation );
        }
    }
}

template<class T>
void FSClusteringTest2D<T>::create_clouds( const NcVar &var )
{
    // calculate the divisions in terms of grid points
    // calculate the deviations
    
    vector<NcDim *>::iterator dim_iter;
    
    for ( size_t index = 0; index < this->coordinate_system()->rank(); index++ )
    {
        NcDim dim = this->coordinate_system()->dimensions()[index];
        
        NcVar dim_var = this->coordinate_system()->dimension_variable( dim );
        
        size_t number_gridpoints = utils::netcdf::num_vals(dim_var) / m_divisions;
        
        m_division_increments[ dim ] = number_gridpoints;
        
        m_deviation.push_back( 0.4 / m_divisions );
    }
    
    typename CoordinateSystem<T>::GridPoint gridpoint = this->coordinate_system()->newGridPoint();
    
    this->m_pointCount = 0;
    
    cout << "Creating clusters at the intersection of " << m_divisions << " lines per axis ..." << endl;
    
    create_clouds_recursive( var, 0, gridpoint );
    
    cout << "done. (" << this->m_pointCount << " points)" << endl;
    
    this->m_totalPointCount += this->m_pointCount;
}

template<class T>
void FSClusteringTest2D<T>::SetUp()
{
    FSTestBase<T>::SetUp();
    
    m_smoothing_scale = 0.01;
    
    // Set the bandwidths
    
    size_t bw_size = this->m_settings->fs_dim();
    
    for ( size_t i=0; i<bw_size; i++ )
    {
        m_fuzziness.push_back( 1.0 / m_divisions );
    }
    
    this->m_bandwidths.push_back( vector<T>( bw_size, 1.0 / m_divisions ) );
    
    // Generate dimensions and dimension variables according to
    // the current settings
    
    this->generate_dimensions();
    
    // Create a variable
    
    // test case 1 : ellipsis for unweighed sample mean
    
    NcVar var = this->add_variable( "cluster_test", 0.0, FS_VALUE_MAX );
    
    create_clouds( var );
    
    FSTestBase<T>::generate_featurespace();
}

template<class T>
void FSClusteringTest2D<T>::TearDown()
{
    FSTestBase<T>::TearDown();
}

#pragma mark -
#pragma mark Test parameterization

template<class T>
FSClusteringTest2D<T>::FSClusteringTest2D() : FSTestBase<T>()
{
    this->m_settings = new FSTestSettings( 2, 1, NUMBER_OF_GRIDPOINTS, FSTestBase<T>::filename_from_current_testcase() );
    
    this->m_divisions = 3;
    
    this->m_cloudSize = 50 * NUMBER_OF_GRIDPOINTS / m_divisions;
    
    this->m_smoothing_scale = 0.01;
}

template<class T>
FSClusteringTest3D<T>::FSClusteringTest3D() : FSClusteringTest2D<T>()
{
    this->m_settings = new FSTestSettings( 3, 1, NUMBER_OF_GRIDPOINTS, FSTestBase<T>::filename_from_current_testcase() );
    
    this->m_divisions = 4;
    
    this->m_cloudSize = 50 * NUMBER_OF_GRIDPOINTS / this->m_divisions;

    this->m_smoothing_scale = 0.01;
}



// 2D
#if RUN_2D

TYPED_TEST_CASE( FSClusteringTest2D, DataTypes );

TYPED_TEST( FSClusteringTest2D, FS_Clustering_2D_Range_Test )
{
    using namespace m3D;
    using namespace m3D::utils;
    using namespace m3D::utils::vectors;

    GaussianNormal<TypeParam> gauss;
    // TypeParam gauss_zero = gauss( vector<TypeParam>(this->coordinate_system()->size(),0) );

    cout << setiosflags(ios::fixed) << setprecision(TEST_PRINT_PRECISION);
    
    for ( size_t i = 0; i < this->m_bandwidths.size(); i++ )
    {
        vector<TypeParam> h = this->m_bandwidths.at(i);
        
        Kernel<TypeParam> *kernel = new GaussianNormalKernel<TypeParam>( vector_norm(h) );
        
        typename Cluster<TypeParam>::list::iterator ci;
        
        start_timer();
        
        vector<netCDF::NcVar> excluded;
                
        vector<TypeParam> resolution = this->m_featureSpace->coordinate_system->resolution();
        
        h.push_back( numeric_limits<TypeParam>::max());
        
        RangeSearchParams<TypeParam> *params = new RangeSearchParams<TypeParam>( h );
        
        ClusterOperation<TypeParam> op(this->m_featureSpace,
                                       (NetCDFDataStore<TypeParam> *)this->m_data_store,
                                       this->m_featureSpaceIndex);
        
        ClusterList<TypeParam> clusters = op.cluster( params, kernel, NULL, false, true, true );
        
        size_t cluster_number = 1;
        for ( ci = clusters.clusters.begin(); ci != clusters.clusters.end(); ci++ )
        {
            Cluster<TypeParam> *c = *ci;
            cout << "Cluster #" << cluster_number++ << " at " << c->mode << " (" << c->points.size() << " points.)" << endl;
        }

        clusters.apply_size_threshold(20);

        EXPECT_EQ(clusters.clusters.size(),4);
        
        delete kernel;
        
        double time = stop_timer();
        cout << "done. (" << time << " seconds, " << clusters.clusters.size() << " modes found:" << endl;
        
        cluster_number = 1;
        for ( ci = clusters.clusters.begin(); ci != clusters.clusters.end(); ci++ )
        {
            Cluster<TypeParam> *c = *ci;
            cout << "Cluster #" << cluster_number++ << " at " << c->mode << " (" << c->points.size() << " points.)" << endl;
        }
        
        const ::testing::TestInfo* const test_info = ::testing::UnitTest::GetInstance()->current_test_info();
        
        ClusterList<TypeParam>::reset_clustering(this->m_featureSpace);
    }
}
#endif

// 3D
#if RUN_3D

TYPED_TEST_CASE( FSClusteringTest3D, DataTypes );

TYPED_TEST( FSClusteringTest3D, FS_Clustering_3D_Test )
{
    using namespace m3D;
    using namespace m3D::utils;
    using namespace m3D::utils::vectors;
    
    const ::testing::TestInfo* const test_info = ::testing::UnitTest::GetInstance()->current_test_info();
    
    cout << setiosflags(ios::fixed) << setprecision(TEST_PRINT_PRECISION);
    
    for ( size_t i = 0; i < this->m_bandwidths.size(); i++ )
    {
        vector<TypeParam> h = this->m_bandwidths.at(i);
        
        Kernel<TypeParam> *kernel = new GaussianNormalKernel<TypeParam>( vector_norm(h) );

        vector<netCDF::NcVar> excluded;
                
        typename Cluster<TypeParam>::list::iterator ci;
        
        start_timer();
        
        ClusterOperation<TypeParam> op(this->m_featureSpace,
                                       (NetCDFDataStore<TypeParam> *)this->m_data_store,
                                       this->m_featureSpaceIndex );
        
        // Create 'bandwidth' parameter from grid resolution and
        // the maximum value in the value range
        
        h.push_back( numeric_limits<TypeParam>::max());
        
        h *= ((TypeParam)0.25);
        
        // Search parameters for neighbourhood searches in clustering
        // should be in the order of the grid resolution
        
        RangeSearchParams<TypeParam> *params = new RangeSearchParams<TypeParam>( h );
        
        ClusterList<TypeParam> clusters = op.cluster( params, kernel, NULL, PostAggregationMethodNone, true, true );
        
        size_t cluster_number = 1;
        for ( ci = clusters.clusters.begin(); ci != clusters.clusters.end(); ci++ )
        {
            Cluster<TypeParam> *c = *ci;
            cout << "Cluster #" << cluster_number++ << " at " << c->mode << " (" << c->points.size() << " points.)" << endl;
        }

        clusters.apply_size_threshold(50);
        
        delete kernel;
        
        double time = stop_timer();
        cout << "done. (" << time << " seconds, " << clusters.clusters.size() << " modes found:" << endl;
        
        cluster_number = 1;
        for ( ci = clusters.clusters.begin(); ci != clusters.clusters.end(); ci++ )
        {
            Cluster<TypeParam> *c = *ci;
            
            cout << "Cluster #" << cluster_number++ << " at " << c->mode << " (" << c->points.size() << " points.)" << endl;
        }
        
        EXPECT_EQ(clusters.clusters.size(),27);
    }
}

#endif


#endif

