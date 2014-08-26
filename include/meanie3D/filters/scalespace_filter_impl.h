
#ifndef _M3D_ScaleSpaceFilter_Impl_H_
#define _M3D_ScaleSpaceFilter_Impl_H_

#include <exception>
#include <stdexcept>
#include <cmath>
#include <map>
#include <string>

#if OPENMP
    #include <omp.h>
#endif

#include <boost/progress.hpp>

#include <cf-algorithms/cf-algorithms.h>
#include <meanie3D/utils.h>

#include "scalespace_filter.h"

namespace m3D {

	using namespace std;
	using namespace ::cfa::meanshift;
	using ::cfa::utils::VisitUtils;
	using ::cfa::utils::coords::CoordinateSystem;

	using namespace std;
 
    template <typename T>
    ScaleSpaceFilter<T>::ScaleSpaceFilter(T scale,
                                          const vector<T> &resolution,
                                          vector<NcVar> &excluded_vars,
                                          T decay,
                                          bool show_progress)
    : FeatureSpaceFilter<T>(show_progress)
    , m_scale(scale)
    , m_decay(decay)
    , m_progress_bar(NULL)
    , m_excluded_vars(excluded_vars)
    {
        if ( scale < 0 )
        {
            throw logic_error("scale can not be less than zero");
        }
        
        if ( decay < 0 || decay >= 1 )
        {
            throw logic_error("decay must be > 0 and < 1");
        }
        
        T filter_width = sqrt(ceil(-2.0*scale*log(decay)))/2;
        
        m_kernels.clear();
        
        for (size_t i = 0; i < resolution.size(); i++)
        {
            // calculate the distances vector
            
            size_t mask_size = filter_width / resolution[i];
            
            vector<T> distances(mask_size,0.0);
            
            for (size_t j=0; j < mask_size; j++)
            {
                distances[j] = ((T)j) * resolution[i];
            }
            
            // create the kernel
            
            ScaleSpaceKernel<T> kernel(scale,distances);
            
            m_kernels.push_back(kernel);
        }
    }

    template <typename T>
    ScaleSpaceFilter<T>::~ScaleSpaceFilter()
    {
    }
    
#pragma mark -
#pragma mark Abstract filter method
    
    template <typename T>
    void
    ScaleSpaceFilter<T>::applyWithArrayIndexRecursive(FeatureSpace<T> *fs,
                                                      ArrayIndex<T> *originalIndex,
                                                      ArrayIndex<T> *filteredPoints,
                                                      vector<size_t> &dimensionIndexes,
                                                      size_t dimensionIndex,
                                                      typename CoordinateSystem<T>::GridPoint& gridpoint)
    {
        // loop over the dimension with the given index. For each
        // coordinate, loop over the other two. At Each point, apply
        // the one-dimensional kernel

        using namespace std;
        
        const CoordinateSystem<T> *cs = fs->coordinate_system;
        
        size_t realDimIndex = dimensionIndexes[dimensionIndex];
        
        NcDim dim = cs->dimensions()[realDimIndex];

        // iterate over dimensions

        for ( int index = 0; index < dim.getSize(); index++ )
        {
            gridpoint[realDimIndex] = index;

            if ( dimensionIndex < (gridpoint.size()-1) )
            {
                applyWithArrayIndexRecursive(fs, originalIndex, filteredPoints, dimensionIndexes, dimensionIndex+1, gridpoint);
            }
            else
            {
                // we reached the fixed dimension

                if (this->show_progress())
                    m_progress_bar->operator++();

                // exclude points that were off limits
                // in any of the original data sets

                //if (fs->off_limits()->get(gridpoint))
                //    continue;

                ScaleSpaceKernel<T> g = this->m_kernels[realDimIndex];
                
                // Find the boundaries. Take care not to step
                // outside the bounds of the array
                
                int width = g.values().size() - 1;
                int gpIndex = (int)gridpoint[realDimIndex];
                int minIndex = (gpIndex - width >= 0) ? (gpIndex - width) : 0;
                int maxIndex = ((gpIndex + width) < (dim.getSize()-1)) ? (gpIndex + width) : (dim.getSize()-1);
                
                // Convolute in 1D around the given point with
                // the mask size determined by the kernel
                // Run the convolution for each feature variable
                
                typename CoordinateSystem<T>::GridPoint gridIter = gridpoint;
            
                vector<T> sum(fs->value_rank(),0.0);
                
                size_t sumCount = 0;
                
                for (int i=minIndex; i<maxIndex; i++)
                {
                    // set gridpoint to current position
                    
                    gridIter[realDimIndex] = i;
                    
                    // Again, make sure no points originally marked as
                    // off limits are used
                    
//                    if (fs->off_limits()->get(gridIter))
//                        continue;
                    
                    // get the point at the iterated position

                    Point<T> *pIter = originalIndex->get(gridIter);

                    if (pIter == NULL)
                        continue;
                    
                    // apply the pre-sampled gaussian and sum up

                    size_t d = (i <= index) ? (index-i) : (i-index);
                    
                    for (int varIndex=0; varIndex < fs->value_rank(); varIndex++)
                    {
                        T value = pIter->values[cs->rank()+varIndex];
                        sum[varIndex] += g.value(d) * value;
                    }
                    
                    sumCount++;
                }
                
                // No muss, no fuss
                
                if (sumCount == 0)
                    continue;
                
                // Fuss! Fetch the point from the array index
                
                Point<T> *p = filteredPoints->get(gridpoint);
                
                // If no point existed, decide if we need to create one
                
                if (p == NULL)
                {
                    // Create a new point with default values
                    // and insert into array index
                    
                    typename CoordinateSystem<T>::Coordinate coordinate = cs->newCoordinate();
                    cs->lookup(gridpoint,coordinate);
                    vector<T> values = coordinate;
                    values.resize(fs->rank(),0.0);
                    
                    p = PointFactory<T>::get_instance()->create(gridpoint,coordinate,values);
                    
                    // Did this exist in the original index?
                    
                    Point<T> *op = originalIndex->get(gridpoint);
                    p->isOriginalPoint = ((op == NULL) ? false : op->isOriginalPoint);
                    
                    // Since we just created this point, there
                    // is no need to copy it again when adding
                    // it to the array index
                    
                    filteredPoints->set(gridpoint,p,false);
                    
                    if (!p->isOriginalPoint)
                        m_created_points++;
                    else
                        m_modified_points++;
                }
                
                // If we have a point after all that, update it with the
                // filtered value
                
                if (p != NULL)
                {
                    // copy values and track limits
                    
                    for (int varIndex=0; varIndex < fs->value_rank(); varIndex++)
                    {
                        p->values[fs->spatial_rank()+varIndex] = sum[varIndex];

                        if (sum[varIndex] < m_min[varIndex])
                            m_min[varIndex] = sum[varIndex];
                        
                        if (sum[varIndex] > m_max[varIndex])
                            m_max[varIndex] = sum[varIndex];
                    }
                }
            }
        }
    }
            
    template <typename T>
    void
    ScaleSpaceFilter<T>::applyWithArrayIndexForDimension(FeatureSpace<T> *fs,
                                                         ArrayIndex<T> *originalIndex,
                                                         ArrayIndex<T> *filteredPoints,
                                                         size_t fixedDimensionIndex)
    {
        // iterate over the other two
        
        // Create a vector, that enumerates the dimensions such, that the
        // fixed dimension comes last
     
        vector<size_t> dimensionIndexes;
        
        for (size_t j=0; j<fs->coordinate_system->rank(); j++)
        {
            if (j==fixedDimensionIndex) continue;
            dimensionIndexes.push_back(j);
        }
        
        dimensionIndexes.push_back(fixedDimensionIndex);
        
        typename CoordinateSystem<T>::GridPoint gridpoint = fs->coordinate_system->newGridPoint();
        
        // Now recurse into the structure, bearing in mind that
        // the dimensions have been re-ordered
        
        applyWithArrayIndexRecursive(fs, originalIndex, filteredPoints, dimensionIndexes, 0, gridpoint);
    }
    
    
    template <typename T>
    void
    ScaleSpaceFilter<T>::applyWithArrayIndex(FeatureSpace<T> *fs)
    {
        using namespace std;
        
        const CoordinateSystem<T> *cs = fs->coordinate_system;
        
        // index the original
        
        if ( this->show_progress() )
        {
            cout << endl << "Constructing array indexes ...";
            
            start_timer();
        }

        ArrayIndex<T> *originalIndex = new ArrayIndex<T>(cs->get_dimension_sizes(), fs->points, true);
        
        ArrayIndex<T> *filteredIndex = new ArrayIndex<T>(cs->get_dimension_sizes(), false);
        
        if ( this->show_progress() )
        {
            cout << "done. (" << stop_timer() << "s)" << endl;
        }

        if ( this->show_progress() )
        {
            cout << endl << "Applying scale filter t=" << m_scale << " decay=" << m_decay << " ... " << endl;

            long numPoints = 1;

            for ( size_t i=0; i < fs->coordinate_system->rank(); i++)
            {
                numPoints *= fs->coordinate_system->dimensions()[i].getSize();
            }

            m_progress_bar = new boost::progress_display( fs->spatial_rank() * numPoints );
            
            start_timer();
        }
        
        // initialize min/max and re-set counts
        
        for (size_t varIndex=0; varIndex<fs->value_rank(); varIndex++)
        {
            m_min[varIndex] = std::numeric_limits<T>::max();
            m_max[varIndex] = std::numeric_limits<T>::min();
        }
        
        m_modified_points = m_created_points = 0;
        
        // Apply dimension by dimension (exploiting separability)
        
        for (size_t dimIndex=0; dimIndex < fs->spatial_rank(); dimIndex++)
        {
            applyWithArrayIndexForDimension(fs, originalIndex, filteredIndex, dimIndex);
            
            delete originalIndex;

            if (dimIndex < (fs->spatial_rank()-1))
            {
                originalIndex = filteredIndex;
                
                filteredIndex = new ArrayIndex<T>(cs->get_dimension_sizes(),false);
            }
        }
        
        // replace the points in the original with the filtered
        // array index results
        filteredIndex->replace_points(fs->points);
        
        size_t originalPoints = 0;
        for (size_t i=0; i < fs->points.size(); i++)
        {
            if (fs->points[i]->isOriginalPoint) originalPoints++;
        }
        
        if ( this->show_progress() )
        {
            cout << "done. (" << stop_timer() << "s)" << endl;
            cout << "Filtered featurespace contains " << fs->size() << " points (" << originalPoints << " original points, "
                 << "(" << m_created_points << " new points))" << endl;
            delete m_progress_bar;
            m_progress_bar = NULL;
        }
        
        // Clean up
        
        delete filteredIndex;
    }
    
    template <typename T>
    void
    ScaleSpaceFilter<T>::apply( FeatureSpace<T> *fs )
    {
        this->m_unfiltered_min = fs->min();
        this->m_unfiltered_max = fs->max();
        this->applyWithArrayIndex(fs);
    }
    
#pragma mark -
#pragma mark Range handling
    
    template <typename T>
    map<size_t,T> 
    ScaleSpaceFilter<T>::getRangeFactors()
    {
        map<size_t,T> factors;
        typename map<size_t,T>::iterator mi;
        
        for (mi = m_min.begin(); mi != m_min.end(); mi++)
        {
            size_t i = mi->first;
            factors[i] = (m_max[i]-m_min[i])/(m_unfiltered_max[i]-m_unfiltered_min[i]);
        }
        
        return factors;
    }
    
    template <typename T>
    const map<size_t,T> &
    ScaleSpaceFilter<T>::get_filtered_min()
    {
        return m_min;
    }
    
    template <typename T>
    const map<size_t,T> &
    ScaleSpaceFilter<T>::get_filtered_max()
    {
        return m_max;
    }
}

#endif
