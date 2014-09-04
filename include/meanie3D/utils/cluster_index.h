#ifndef _M3D_ClusterIndex_H_
#define _M3D_ClusterIndex_H_

#include <meanie3D/defines.h>
#include <meanie3D/namespaces.h>

#include <cf-algorithms/cf-algorithms.h>

namespace m3D { namespace utils {

    // Matrix
    
    template <typename T>
    struct ClusterIndex
    {
        
    public:

        typedef MultiArray<cfa::id_t> index_t;
        
    private:
        
        index_t *m_index;
        
    public:
        
        ClusterIndex(typename Cluster<T>::list &list,
                     const vector<size_t> &dimensions);
        
        ~ClusterIndex();
        
        // Accessors
        
        index_t *data();
        
        // Static functions
        
        /** Iterates over the other array, finds all point
         */
        static size_t count_common_points(const ClusterIndex<T> *a,
                                          const ClusterIndex<T> *b,
                                          ::cfa::id_t id);
        
        /** What is the ratio of gridpoints in cluster A that
         * are also occupied by points from cluster B? 
         *
         * Important: cluster B must have been indexed by this 
         * index prior to calling.
         *
         * @param cluster A
         * @param cluster B
         */
        double occupation_ratio(const Cluster<T> *cluster_a,
                                const Cluster<T> *cluster_b) const;
        
    };
    
}}

#endif