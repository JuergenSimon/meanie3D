#!/usr/bin/python

import csv
import getopt
import json
import sys


def load_json(filename):
  """ Load a json file
  :param filename:
  :return:
  """
  json_data = open(filename)
  data = json.load(json_data)
  json_data.close()
  return data;


def write_csv(json):
  """
  Converts a meanie3D track-dictionary.json into the output files
  required by the tracking intercomparison project. The output
  consists of two CSV files, that are constructed as follows:

  Table 1 characterizes the objects identified and should provide for all time steps the following information in 12 columns
  (1)	 Time step (e.g. ranging from 1 to 1819)					[uint]
  (2)	 Object ID (should be unique for each object per time step)			[uint]
  (3)	 X coordinate of object center							[float]
  (4)	 Y coordinate of object center							[float]
  (5)	 Size of object (=number of grid boxes included)				[uint]
  (6)	 X_min (minimal X location of the object)					[float]
  (7)	 X_max (maximal X location of the object)					[float]
  (8)	 Y_min (minimal Y location of the object)					[float]
  (9)	 Y_max (maximal Y location of the object)					[float]
  (10)	 Mean LWP of the object (=average value of LWP for all grid boxes included)	[float]
  (11)	 Min. LWP of the object (=smallest LWP value included)				[float]
  (12)	 Max LWP of the object (=maximal LWP value included)				[float]

  Table 2 should contain the tracking information for all time steps (rows) in 4 columns:
  (1)	Basis time step t_i (e.g. ranging from 1 to 1818)					[uint]
  (2)	Object ID identified in time step t_i (from Table 1)				[uint]
  (3)	Object ID of identified successor object in time step t_i+1 (from Table 1)	[uint]
  (4)	Weight factor to decide object relevance after split/merge event for life
      time statistics (optional), e.g. transition probability or overlap ratio

  :param json:
  :return:
  """

  #
  # Write the objects
  #

  objects_file = open('objects.csv', 'wb')
  if (not objects_file):
    sys.stderr.write("Could not open 'objects.csv' for writing.")
    sys.exit(1)

  objects = csv.writer(objects_file, delimiter=';', quotechar='|', quoting=csv.QUOTE_MINIMAL)

  print("Writing object information:\n")
  tracks = json['tracks'];
  for ti in range(0, len(tracks)):
    track = tracks[ti]
    clusters = track['clusters']

    # if the first row, write a header
    if ti == 0:
      header = ['timestep', 'id', 'centerx', 'centery', 'volume',
                'boxxmin', 'boxxmax', 'boxymin', 'boxymax',
                'lwpmean', 'lwpmin', 'lwpmax']
      print(header)
      objects.writerow(header)

    for ci in range(0, len(clusters)):
      cluster = clusters[ci]

      row = []

      # (1)	 Time step (e.g. ranging from 1 to 1819)
      row.append(cluster['step'] + 1),

      # (2)	 Object ID (should be unique for each object per time step)
      row.append(cluster['uuid'])

      # (3)	 X coordinate of object center
      # (4)	 Y coordinate of object center
      mode = cluster['mode']
      for mi in range(0, len(mode)):
        row.append(mode[mi])

      # (5)	 Size of object (=number of grid boxes included)
      row.append(cluster['size'])

      # (6)	 X_min (minimal X location of the object)
      # (7)	 X_max (maximal X location of the object)
      # (8)	 Y_min (minimal Y location of the object)
      # (9)	 Y_max (maximal Y location of the object)
      row.append(cluster['bounding_box_min'][1]);
      row.append(cluster['bounding_box_max'][1]);
      row.append(cluster['bounding_box_min'][0]);
      row.append(cluster['bounding_box_max'][0]);

      # (10)	 Mean LWP of the object (=average value of LWP for all grid boxes included)
      row.append(cluster['median'][0])

      # (11)	 Min. LWP of the object (=smallest LWP value included)
      row.append(cluster['min'][0])

      # (12)	 Max LWP of the object (=maximal LWP value included)
      row.append(cluster['max'][0])

      print(row)
      objects.writerow(row)

  #
  # Write the tracks
  #
  print("Writing tracking information:\n")

  tracks_file = open('tracks.csv', 'wb')
  if (not tracks_file):
    sys.stderr.write("Could not open 'tracks.csv' for writing.")
    sys.exit(1)

  tracks = csv.writer(tracks_file, delimiter=';', quotechar='|', quoting=csv.QUOTE_MINIMAL)

  tree = json['tree']
  nodes = tree['nodes']
  links = tree['links']

  for li in range(0, len(links)):
    link = links[li]

    source_index = link['source'] - 1
    source = nodes[source_index]

    target_index = link['target'] - 1
    target = nodes[target_index]

    # if the first row, write a header
    if li == 0:
      header = ['timestep', 'idstep1', 'idstep2', 'weight']
      print(header)
      tracks.writerow(header)

    row = []
    # (1)	Basis time step t_i (e.g. ranging from 1 to 1818)
    row.append(source['step'] + 1)

    # (2)	Object ID identified in time step t_i (from Table 1)
    row.append(source['uuid'])

    # (3)	Object ID of identified successor object in time step t_i+1 (from Table 1)
    row.append(target['uuid'])

    # (4)	Weight factor to decide object relevance after split/merge event for life
    row.append(0)

    print(row)
    tracks.writerow(row)


# ----------------------------------------------------------------------------
## Prints usage and exits
#
def usage():
  print("meanie2tic.py [--file,-f] <json file> [--help,-h]")
  print("Converts meanie3D track-dictionary.json into CSV files for Tracking Intercomparison project")
  print("--file,-f : track dictionary file (defaults to 'track-dictionary.json' if omitted)")
  print("--help, -h  : print this message and exit.")
  sys.exit(1)
  return


# ----------------------------------------------------------------------------

# ----------------------------------------------------------------------------
## Main function
def main():
  # Parse command line
  try:
    argv = sys.argv[1:]
    opts, args = getopt.getopt(argv, "f:h", ["file", "help"])
  except getopt.GetoptError as detail:
    print(detail)
    sys.exit(2)

  if len(sys.argv) < 2:
    usage()

  filename = "track-dictionary.json"
  for o, a in opts:
    if o in ["--help", "-h"]:
      usage()
    elif o in ["--file", "-f"]:
      filename = a

  json = load_json(filename)
  if not json:
    sys.stderr.write("ERROR:could not read file %s" % filename)
    sys.exit(1)

  write_csv(json)
  return


# ----------------------------------------------------------------------------

# ----------------------------------------------------------------------------
# entry point
# ----------------------------------------------------------------------------

if __name__ == "__main__":
  main()
  sys.exit(0)
