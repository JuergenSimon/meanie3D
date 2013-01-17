#ifndef _M3D_DEFINES_H_
#define _M3D_DEFINES_H_

// Threading

#define PROVIDE_THREADSAFETY 0

#if WITH_BOOST_THREADS
	#define PROVIDE_MUTEX 1
#endif

#ifdef WITH_TBB
	#define PROVIDE_MUTEX 1
#endif

// Featurespace debugging

#define DEBUG_MEANSHIFT_GRAPH 0
#define DEBUG_CLUSTER_MERGING 0

#define WRITE_CLUSTERS 0
#define WRITE_BOUNDARIES 0
#define WRITE_MODES 0

#endif
