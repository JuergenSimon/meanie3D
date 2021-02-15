# How to use this software

Please make sure the software is compiled and available by following the
instructions in the README.md file. 

## General overview
The process of tracking objects over time is generally comprised of two steps:
### Cluster detection
Using the mean-shift algorithm, objects are detected in your data. The objects are
the result first running the mean-shift algorithm with vectors discretized to snap
onto the data set's own grid. The vectors are then linked into graphs. Each graph 
has a mode, which is a point or area where the graph ends. A cluster is made up of 
all points in the data set that end at the same mode. Each cluster is given a unique
ID (for the given time).

### Tracking
When clustering a data set that varies over time (Example: satellite data), it is
interesting to study the development of clusters over time. For this to happen, the
clusters from time `t1` must be identified again in `t2` (if possible). The meanie3D
package provides a tool for doing this based on a weighed sum of three factors:
* Proximity 
  This is a measure of the spatial distance of the clusters modes between `t1` and `t2` 
  (the closer, the more likely it is a match)
* Cluster size
  This is based on the assumption that the tracked phenomenon change slowly enough. 
  The more equal in size clusters from `t1` and `t2` are, the more likely a match. 
* Histogram 
  A selected value is used and histogram of it's values is levvied for each cluster. 
  When comparing the `t1` and `t2` clusters, those histograms are compared using a
  Kendall Rank Coefficient. The more highly correllated the histograms are, the more
  likely a match

All three factors are summed (with weights) to arrive at the final numbers. The clusters
are then paired off in the descending order of the match likelihood. The result is, that
clusters from `t2` are given identical ids of clusters in `t1`. The set of clusters with
the same ID over time constitute a track. 

## Python frontend

## Visualization

### Clusters and tracks
In order to see a visual representation of the algorithms workings and it's results, 
you will have to install the VisIt package. 

### Track Graph
A track graph describes the development of clusters over time, but not in terms of spatial 
location, but rather in terms of it's development history, including merges and splits. 
It is a useful tool to get a good overview over the provided tracks in terms of tracking 
quality and for picking suitable tracks for subsequent analysis. 

TODO: instructions how to generate and view tracking graph.

## Docker version
The docker version does not support any of the visualisation features. This has
something to do with the fact, that the VisIt application used to generate imagery
can not run in headless mode. Therefore the only functions available in the docker
version are:
* Cluster detection
* Tracking
* Track statistics and -Graph