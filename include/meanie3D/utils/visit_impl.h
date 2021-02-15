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

#ifndef M3D_VISUALIZATIONUTILS_IMPL_H
#define M3D_VISUALIZATIONUTILS_IMPL_H

#if WITH_VTK

#include <iostream>
#include <stdlib.h>
#include <vector>
#include <list>

#include <vtkRenderer.h>
#include <vtkActor.h>
#include <vtkRenderWindowInteractor.h>
#include <vtkRenderWindow.h>
#include <vtkDataSetMapper.h>
#include <vtkProperty.h>

#include <vtkCellArray.h>
#include <vtkCellData.h>
#include <vtkCleanPolyData.h>
#include <vtkDataArray.h>
#include <vtkDataSetSurfaceFilter.h>
#include <vtkDelaunay3D.h>
#include <vtkDelaunay2D.h>
#include <vtkDoubleArray.h>
#include <vtkFieldData.h>
#include <vtkGeometryFilter.h>
#include <vtkHexahedron.h>
#include <vtkImageData.h>
#include <vtkLookupTable.h>
#include <vtkPointData.h>
#include <vtkPolyDataConnectivityFilter.h>
#include <vtkPolyDataWriter.h>
#include <vtkQuad.h>
#include <vtkPixel.h>
#include <vtkRectilinearGridWriter.h>
#include <vtkSmartPointer.h>
#include <vtkDataSetSurfaceFilter.h>
#include <vtkType.h>
#include <vtkUnsignedCharArray.h>
#include <vtkUnstructuredGrid.h>
#include <vtkUnstructuredGridWriter.h>
#include <vtkVersion.h>
#include <vtkVertex.h>
#include <vtkVoxel.h>
#include <vtkXMLImageDataWriter.h>
#include <vtkXMLPolyDataWriter.h>
#include <vtkXMLRectilinearGridWriter.h>
#include <vtkXMLUnstructuredGridWriter.h>

#include <meanie3D/clustering.h>

#include "visit.h"

namespace m3D
{
    namespace utils
    {

        using namespace std;

        /** Global variable maintaining the dimension mapping */
        template <typename T>
        vector<size_t> VisitUtils<T>::VTK_DIMENSION_INDEXES;

// Macro for safe access
#define get_vtk_index(i) VTK_DIMENSION_INDEXES.empty() ? i : VTK_DIMENSION_INDEXES[i]

        /* ---------------------------------------------------------------- */
        /* General VTK data structure mapping/handling                      */
        /* ---------------------------------------------------------------- */

        template <typename T>
        void
        VisitUtils<T>::update_vtk_dimension_mapping(vector<string> dim_names,
                                                    vector<string> vtk_dim_names)
        {
            // VTK dimension mapping

            if (!vtk_dim_names.empty())
            {
                vector<size_t> vtk_dimension_indexes;

                // Number of values should be the same in --dimensions and --vtk-dimensions

                if (vtk_dim_names.size() != dim_names.size())
                {
                    cerr << "FATAL: --dimensions and --vtk-dimensions must have \
                the same number of arguments"
                         << endl;
                    exit(EXIT_FAILURE);
                }

                // Check if all dimensions are accounted for in --vtk-dimensions

                for (size_t i = 0; i < vtk_dim_names.size(); i++)
                {
                    vector<string>::iterator vi = find(dim_names.begin(), dim_names.end(), vtk_dim_names[i]);

                    if (vi == dim_names.end())
                    {
                        cerr << "FATAL: --vtk-dimensions - entry "
                             << vtk_dim_names[i]
                             << " has no pendant in attribute \
                        'featurespace_dimensions'"
                             << endl;
                        exit(EXIT_FAILURE);
                    }

                    int index = utils::vectors::index_of_first(dim_names, vtk_dim_names[i]);

                    vtk_dimension_indexes.push_back((size_t)index);
                }

                utils::VisitUtils<T>::VTK_DIMENSION_INDEXES = vtk_dimension_indexes;

                VTK_DIMENSION_INDEXES = vtk_dimension_indexes;
            }
        }

        template <typename T>
        void
        VisitUtils<T>::get_vtk_image_dimensions(const CoordinateSystem<T> *cs, int &nx, int &ny, int &nz)
        {
            assert(cs->rank() > 0 && cs->rank() <= 3);
            nx = ny = nz = 1;
            if (cs->rank() == 1)
            {
                nx = cs->dimensions()[get_vtk_index(0)].getSize();
            }
            else if (cs->rank() == 2)
            {
                nx = cs->dimensions()[get_vtk_index(0)].getSize();
                ny = cs->dimensions()[get_vtk_index(1)].getSize();
            }
            else
            {
                nx = cs->dimensions()[get_vtk_index(0)].getSize();
                ny = cs->dimensions()[get_vtk_index(1)].getSize();
                nz = cs->dimensions()[get_vtk_index(2)].getSize();
            }
        }

        template <typename T>
        void
        VisitUtils<T>::get_vtk_coords(const vector<T> &coords, T &x, T &y, T &z)
        {
            assert(coords.size() > 0 && coords.size() <= 3);

            x = y = z = 0;

            if (coords.size() == 1)
            {
                x = coords.at(get_vtk_index(0));
            }
            else if (coords.size() == 2)
            {
                x = coords.at(get_vtk_index(0));
                y = coords.at(get_vtk_index(1));
            }
            else
            {
                x = coords.at(get_vtk_index(0));
                y = coords.at(get_vtk_index(1));
                z = coords.at(get_vtk_index(2));
            }
        }

        template <typename T>
        void
        VisitUtils<T>::get_vtk_gridpoint(const vector<int> &gp, int &gx, int &gy, int &gz)
        {
            assert(gp.size() > 0 && gp.size() <= 3);

            gx = gy = gz = 0;

            if (gp.size() == 1)
            {
                gx = gp.at(get_vtk_index(0));
            }
            else if (gp.size() == 2)
            {
                gx = gp.at(get_vtk_index(0));
                gy = gp.at(get_vtk_index(1));
            }
            else
            {
                gx = gp.at(get_vtk_index(0));
                gy = gp.at(get_vtk_index(1));
                gz = gp.at(get_vtk_index(2));
            }
        }

        template <typename T>
        void
        copy_vtk_coordinate_data(const CoordinateSystem<T> *cs, size_t index, vtkDoubleArray *coords)
        {
            NcVar vx = cs->dimension_variables().at(index);
            const T *dim_data = cs->get_dimension_data_ptr(vx);
            NcDim dx = cs->dimensions().at(index);
            coords->SetNumberOfValues(dx.getSize());
            for (int i = 0; i < dx.getSize(); i++)
            {
                double value = dim_data[i];
                coords->SetValue(i, value);
            }
        }

        template <typename T>
        vtkRectilinearGrid *
        VisitUtils<T>::allocate_vtk_rectilinear_grid(const CoordinateSystem<T> *cs,
                                                     vector<vtkDoubleArray *> &coord_pointers)
        {
            assert(cs->rank() > 0 && cs->rank() <= 3);
            int nx = 0, ny = 0, nz = 0;
            get_vtk_image_dimensions(cs, nx, ny, nz);
            vtkRectilinearGrid *rgrid = vtkRectilinearGrid::New();
            rgrid->SetDimensions(nx, ny, nz);
            for (int i = 0; i < cs->rank(); i++)
            {
                vtkDoubleArray *coords = vtkDoubleArray::New();
                coords->SetNumberOfComponents(1);
                copy_vtk_coordinate_data(cs, get_vtk_index(i), coords);
                coord_pointers.push_back(coords);
                if (i == 0)
                {
                    rgrid->SetXCoordinates(coords);
                }
                else if (i == 1)
                {
                    rgrid->SetYCoordinates(coords);
                }
                else
                {
                    rgrid->SetZCoordinates(coords);
                }
            }

            // supply 1-D arrays with value 0 for the
            // empty dimensions
            for (int i = 2; i >= 0; i--)
            {
                if (i >= cs->rank())
                {
                    vtkDoubleArray *coords = vtkDoubleArray::New();
                    coords->SetNumberOfComponents(1);
                    coords->InsertNextValue(0.0);
                    coord_pointers.push_back(coords);
                    if (i == 0)
                    {
                        rgrid->SetXCoordinates(coords);
                    }
                    else if (i == 1)
                    {
                        rgrid->SetYCoordinates(coords);
                    }
                    else
                    {
                        rgrid->SetZCoordinates(coords);
                    }
                }
            }
            return rgrid;
        }

        template <typename T>
        vtkIdType
        VisitUtils<T>::to_single_index(const CoordinateSystem<T> *cs,
                                       int nx, int ny, int nz,
                                       int gx, int gy, int gz)
        {
            vtkIdType index = 0;

            if (cs->rank() == 1)
            {
                // 'x'
                index = gx;
            }
            else if (cs->rank() == 2)
            {
                // 'x' + 'y' * 'Nx'
                index = gx + gy * nx;
            }
            else
            {
                // 'x' + 'y' * 'Nx' + 'z' * 'Nx' * 'Ny'
                index = gx + gy * nx + gz * nx * ny;
            }

            return index;
        }

        /* ---------------------------------------------------------------- */
        /* Specific writing routines                                        */
        /* ---------------------------------------------------------------- */

        /** Write out the cluster list as list of points. The point's 'value' is the size of the cluster
 * @param full path to filename, including extension '.vtk'
 * @param cluster list
 */
        template <typename T>
        void
        VisitUtils<T>::write_cluster_modes_vtk(const string &filename, const typename Cluster<T>::list &list,
                                               bool spatial_only)
        {
            ofstream f(filename.c_str());
            f << fixed << setprecision(4);

            // Write Header

            f << "# vtk DataFile Version 3.0" << endl;
            f << "point list " << endl;
            f << "ASCII" << endl;
            f << "DATASET UNSTRUCTURED_GRID" << endl;
            f << "POINTS " << list.size() << " FLOAT" << endl;

            for (size_t ci = 0; ci < list.size(); ci++)
            {
                typename Cluster<T>::ptr c = list[ci];
                size_t dim_count = spatial_only ? c->spatial_rank() : c->rank();
                vector<T> mode = c->mode;
                size_t dims_plotted = 0;

                for (size_t vi = 0; vi < dim_count; vi++)
                {
                    size_t dim_index = vi;

                    if (vi < VTK_DIMENSION_INDEXES.size())
                    {
                        dim_index = VTK_DIMENSION_INDEXES.empty() ? vi : VTK_DIMENSION_INDEXES[vi];
                    }

                    f << mode[dim_index] << "\t";
                    dims_plotted++;
                }

                while (dims_plotted < 3)
                {
                    f << "0.0\t";
                    dims_plotted++;
                }

                f << endl;
            }

            f << endl;
            f << "POINT_DATA " << list.size() << endl;
            f << "SCALARS mode INT" << endl;
            f << "LOOKUP_TABLE default" << endl;
            for (size_t pi = 0; pi < list.size(); pi++)
            {
                typename Cluster<T>::ptr c = list[pi];
                size_t dim_count = spatial_only ? c->spatial_rank() : c->rank();

                if (spatial_only)
                {
                    f << c->id << endl;
                }
                else
                {
                    f << c->mode[dim_count - 1];
                }
            }

            f.close();
        }

        template <class T>
        void
        VisitUtils<T>::write_geometrical_cluster_centers_vtk(const string &filename,
                                                             const typename Cluster<T>::list &list,
                                                             bool at_max_height)
        {
            ofstream f(filename.c_str());
            f << fixed << setprecision(4);

            // determine the spatial dimension
            typename Cluster<T>::ptr c = NULL;
            typename Point<T>::ptr p = NULL;

            // Write Header
            f << "# vtk DataFile Version 3.0" << endl;
            f << "point list " << endl;
            f << "ASCII" << endl;
            f << "DATASET UNSTRUCTURED_GRID" << endl;
            f << "POINTS " << list.size() << " FLOAT" << endl;

            for (size_t ci = 0; ci < list.size(); ci++)
            {
                typename Cluster<T>::ptr c = list[ci];
                // obtain the geometrical center
                size_t spatial_dims = c->get_points().at(0)->coordinate.size();
                vector<T> mode = c->geometrical_center();
                if (spatial_dims == 3 && at_max_height)
                {
                    T max = 0;
                    for (size_t pi = 0; pi < c->size(); pi++)
                    {
                        Point<T> *p = c->get_points().at(pi);
                        if (p->coordinate[0] > max)
                        {
                            max = p->coordinate[0];
                        }
                    }
                    mode[0] = 1.5 * max;
                }
                size_t dims_plotted = 0;
                for (size_t vi = 0; vi < spatial_dims; vi++)
                {
                    size_t dim_index = vi;
                    if (vi < VTK_DIMENSION_INDEXES.size())
                    {
                        dim_index = VTK_DIMENSION_INDEXES.empty() ? vi : VTK_DIMENSION_INDEXES[vi];
                    }
                    f << mode[dim_index] << "\t";
                    dims_plotted++;
                }

                while (dims_plotted < 3)
                {
                    f << "0.0\t";
                    dims_plotted++;
                }
                f << endl;
            }
            f << endl;
            f << "POINT_DATA " << list.size() << endl;
            f << "SCALARS geometrical_center INT" << endl;
            f << "LOOKUP_TABLE default" << endl;
            for (size_t pi = 0; pi < list.size(); pi++)
            {
                typename Cluster<T>::ptr c = list[pi];
                f << c->id << endl;
            }
            f.close();
        }

        static vtkSmartPointer<vtkLookupTable> _vtk_cluster_table = NULL;

        template <typename T>
        vtkSmartPointer<vtkLookupTable>
        VisitUtils<T>::cluster_lookup_table()
        {
            if (_vtk_cluster_table == NULL)
            {
                // Lookup table
                _vtk_cluster_table = vtkSmartPointer<vtkLookupTable>::New();
                _vtk_cluster_table->SetNumberOfTableValues(6);

                // Red
                _vtk_cluster_table->SetTableValue(0, 255.0 / 255.0, 0.0 / 255.0, 0.0 / 255.0);

                // Purple
                _vtk_cluster_table->SetTableValue(1, 205.0 / 255.0, 255.0 / 255.0, 0.0 / 255.0);

                // Green
                _vtk_cluster_table->SetTableValue(2, 0.0 / 255.0, 255.0 / 255.0, 0.0 / 255.0);

                // Orange
                _vtk_cluster_table->SetTableValue(3, 255.0 / 255.0, 102.0 / 255.0, 0.0 / 255.0);

                // Blue
                _vtk_cluster_table->SetTableValue(4, 0.0 / 255.0, 187.0 / 255.0, 255.0 / 255.0);

                // Yellow
                _vtk_cluster_table->SetTableValue(5, 255.0 / 255.0, 255.0 / 255.0, 0.0 / 255.0);

                _vtk_cluster_table->Build();
            }

            return _vtk_cluster_table;
        }

        template <typename T>
        void
        VisitUtils<T>::write_clusters_vtu(const ClusterList<T> *list,
                                          const CoordinateSystem<T> *cs,
                                          const string &base_name,
                                          unsigned int max_colors,
                                          bool use_ids,
                                          bool include_boundary,
                                          bool write_xml)
        {
            // escape dangerous characters from basename
            string basename = boost::filesystem::path(base_name).stem().string();
            string mesh_filename = basename + "-clusters" + (write_xml ? ".vtu" : ".vtk");
            string poly_filename = basename + "-boundaries" + (write_xml ? ".vtu" : ".vtk");

            size_t num_points = 0;
            size_t point_dim = 0;

            cout << "Cluster list has " << list->clusters.size() << " clusters" << endl;
            for (size_t ci = 0; ci < list->clusters.size(); ci++)
            {
                num_points += list->clusters[ci]->size();
                if (point_dim == 0)
                {
                    point_dim = list->clusters[ci]->get_points()[0]->coordinate.size();
                }
            }
            // Only process 2D/3D for now
            assert(point_dim == 2 || point_dim == 3);

            vtkSmartPointer<vtkCellArray> cellArray = vtkSmartPointer<vtkCellArray>::New();
            vtkSmartPointer<vtkPoints> points = vtkSmartPointer<vtkPoints>::New();

            vtkSmartPointer<vtkPoints> centerPoints = vtkSmartPointer<vtkPoints>::New();
            vtkSmartPointer<vtkCellArray> centerCellArray = vtkSmartPointer<vtkCellArray>::New();

            vtkSmartPointer<vtkDoubleArray> pointData = vtkSmartPointer<vtkDoubleArray>::New();
            pointData->SetName("point_data");

            vtkSmartPointer<vtkDoubleArray> cellData = vtkSmartPointer<vtkDoubleArray>::New();
            cellData->SetName("cell_data");

            vtkSmartPointer<vtkIntArray> pointColors = vtkSmartPointer<vtkIntArray>::New();
            pointColors->SetName("point_color");
            pointColors->SetLookupTable(cluster_lookup_table());

            vtkSmartPointer<vtkIntArray> cellColors = vtkSmartPointer<vtkIntArray>::New();
            cellColors->SetName("cell_color");
            cellColors->SetLookupTable(cluster_lookup_table());

            // vtkSmartPointer<vtkIntArray> clusterIds = vtkSmartPointer<vtkIntArray>::New();
            // clusterIds->SetName("geometrical_center");

            for (size_t ci = 0; ci < list->clusters.size(); ci++)
            {
                m3D::id_t id = use_ids ? list->clusters[ci]->id : ci;
                int color = id % 6;
                typename Cluster<T>::ptr cluster = list->clusters[ci];

                // Write ID
                T x, y, z;
                // VisitUtils<T>::get_vtk_coords(cluster->geometrical_center(), x, y, z);
                // vtkIdType center;
                // center = centerPoints->InsertNextPoint(x, y, point_dim == 2 ? 0 : z);
                // vtkSmartPointer<vtkPixel> centerPixel = vtkSmartPointer<vtkPixel>::New();
                // centerPixel->GetPointIds()->SetId(0, center);
                // centerCellArray->InsertNextCell(centerPixel);
                // clusterIds->InsertNextValue(id);

                // Write point_color and value information
                for (vtkIdType pi = 0; pi < cluster->size(); pi++)
                {
                    Point<T> *p = list->clusters[ci]->get_points()[pi];
                    double scalarValue = p->values[point_dim];
                    cellData->InsertNextValue(scalarValue);
                    cellColors->InsertNextValue(color);
                    x = y = z = 0.0;
                    VisitUtils<T>::get_vtk_coords(p->coordinate, x, y, z);
                    if (point_dim == 2)
                    {
                        T rx = cs->resolution()[VisitUtils<T>::index_of(0)] / 2.0;
                        T ry = cs->resolution()[VisitUtils<T>::index_of(1)] / 2.0;

                        vtkIdType p1 = points->InsertNextPoint(x - rx, y - ry, 0);
                        pointData->InsertNextValue(scalarValue);
                        pointColors->InsertNextValue(color);

                        vtkIdType p2 = points->InsertNextPoint(x + rx, y - ry, 0);
                        pointData->InsertNextValue(scalarValue);
                        pointColors->InsertNextValue(color);

                        vtkIdType p3 = points->InsertNextPoint(x + rx, y + ry, 0);
                        pointData->InsertNextValue(scalarValue);
                        pointColors->InsertNextValue(color);

                        vtkIdType p4 = points->InsertNextPoint(x - rx, y + ry, 0);
                        pointData->InsertNextValue(scalarValue);
                        pointColors->InsertNextValue(color);

                        // Create a quad on the four points
                        vtkSmartPointer<vtkQuad> quad = vtkSmartPointer<vtkQuad>::New();
                        quad->GetPointIds()->SetId(0, p1);
                        quad->GetPointIds()->SetId(1, p2);
                        quad->GetPointIds()->SetId(2, p3);
                        quad->GetPointIds()->SetId(3, p4);

                        cellArray->InsertNextCell(quad);
                    }
                    else
                    {
                        T rx = cs->resolution()[VisitUtils<T>::index_of(0)] / 2.0;
                        T ry = cs->resolution()[VisitUtils<T>::index_of(1)] / 2.0;
                        T rz = cs->resolution()[VisitUtils<T>::index_of(2)] / 2.0;

                        vtkIdType p1 = points->InsertNextPoint(x - rx, y - ry, z - rz);
                        pointData->InsertNextValue(scalarValue);
                        pointColors->InsertNextValue(color);

                        vtkIdType p2 = points->InsertNextPoint(x + rx, y - ry, z - rz);
                        pointData->InsertNextValue(scalarValue);
                        pointColors->InsertNextValue(color);

                        vtkIdType p3 = points->InsertNextPoint(x + rx, y + ry, z - rz);
                        pointData->InsertNextValue(scalarValue);
                        pointColors->InsertNextValue(color);

                        vtkIdType p4 = points->InsertNextPoint(x - rx, y + ry, z - rz);
                        pointData->InsertNextValue(scalarValue);
                        pointColors->InsertNextValue(color);

                        vtkIdType p5 = points->InsertNextPoint(x - rx, y - ry, z + rz);
                        pointData->InsertNextValue(scalarValue);
                        pointColors->InsertNextValue(color);

                        vtkIdType p6 = points->InsertNextPoint(x + rx, y - ry, z + rz);
                        pointData->InsertNextValue(scalarValue);
                        pointColors->InsertNextValue(color);

                        vtkIdType p7 = points->InsertNextPoint(x + rx, y + ry, z + rz);
                        pointData->InsertNextValue(scalarValue);
                        pointColors->InsertNextValue(color);

                        vtkIdType p8 = points->InsertNextPoint(x - rx, y + ry, z + rz);
                        pointData->InsertNextValue(scalarValue);
                        pointColors->InsertNextValue(color);

                        // Create a voxel on the eight points
                        vtkSmartPointer<vtkHexahedron> voxel = vtkSmartPointer<vtkHexahedron>::New();
                        voxel->GetPointIds()->SetId(0, p1);
                        voxel->GetPointIds()->SetId(1, p2);
                        voxel->GetPointIds()->SetId(2, p3);
                        voxel->GetPointIds()->SetId(3, p4);

                        voxel->GetPointIds()->SetId(4, p5);
                        voxel->GetPointIds()->SetId(5, p6);
                        voxel->GetPointIds()->SetId(6, p7);
                        voxel->GetPointIds()->SetId(7, p8);

                        cellArray->InsertNextCell(voxel);
                    }
                }
            }

            // 2D is a square, 3D a hexahedron
            VTKCellType type = point_dim == 2 ? VTK_QUAD : VTK_HEXAHEDRON;

            // TODO: research and finish this part. The problem seems to be to have two unstructured
            // grids with different numbers of points in one file. 
            // Create an unstructured grid for center labels
            // vtkSmartPointer<vtkUnstructuredGrid> centerGrid = vtkSmartPointer<vtkUnstructuredGrid>::New();
            // centerGrid->SetPoints(centerPoints);
            // centerGrid->SetCells(type, centerCellArray);
            // centerGrid->GetPointData()->AddArray(clusterIds);
            // centerGrid->GetCellData()->AddArray(clusterIds);

            // Create an unstructured grid and write it off
            vtkSmartPointer<vtkUnstructuredGrid> unstructuredGrid = vtkSmartPointer<vtkUnstructuredGrid>::New();
            unstructuredGrid->SetPoints(points);
            unstructuredGrid->SetCells(type, cellArray);
            unstructuredGrid->GetPointData()->AddArray(pointData);
            unstructuredGrid->GetPointData()->AddArray(pointColors);
            unstructuredGrid->GetCellData()->AddArray(cellData);
            unstructuredGrid->GetCellData()->AddArray(cellColors);
            //unstructuredGrid->GetCellData()->AddArray(clusterIds);

            // Extract the envelope
            vtkSmartPointer<vtkAlgorithm> enveloper;
            if (include_boundary)
            {
                // Geometry filter
                vtkSmartPointer<vtkGeometryFilter> geometry = vtkSmartPointer<vtkGeometryFilter>::New();
                geometry->SetInputData(unstructuredGrid);

                vtkSmartPointer<vtkCleanPolyData> cleaner = vtkSmartPointer<vtkCleanPolyData>::New();
                cleaner->SetInputConnection(geometry->GetOutputPort());

                // Delauny 2D/3D

                if (point_dim == 2)
                {
                    vtkSmartPointer<vtkDelaunay2D> delauny = vtkSmartPointer<vtkDelaunay2D>::New();
                    delauny->SetInputConnection(cleaner->GetOutputPort());
                    enveloper = delauny;
                }
                else
                {
                    vtkSmartPointer<vtkDelaunay3D> delauny = vtkSmartPointer<vtkDelaunay3D>::New();
                    delauny->SetInputConnection(cleaner->GetOutputPort());
                    enveloper = delauny;
                }
            }

            // Write .vtu/.vtk file
            // The reason this absurd macro is used is, that to much surprise
            // vtkXMLUnstructuredGridWriter and vtkUnstructuredGridWriter have
            // the same interface, but do not formally implement one or inherit
            // from the same base class. Without this construct, the code would
            // have to be duplicated ...

#define WRITE_CLUSTERS(writer, unstructuredGrid, include_boundary, poly_filename, enveloper) \
    writer->SetFileName(mesh_filename.c_str());                                                  \
    /* TODO: comment back in when the problem with the labels on the grid is solved  */          \
    /* writer->SetInputData(centerGrid); */                                                      \
    /* writer->Write(); */                                                                       \
    writer->SetInputData(unstructuredGrid);                                                      \
    writer->Write();                                                                             \
    if (include_boundary)                                                                        \
    {                                                                                            \
        writer->SetFileName(poly_filename.c_str());                                              \
        writer->SetInputConnection(enveloper->GetOutputPort());                                  \
        writer->Write();                                                                         \
    }
            if (write_xml) {
                vtkSmartPointer<vtkXMLUnstructuredGridWriter> writer = vtkSmartPointer<vtkXMLUnstructuredGridWriter>::New();
                WRITE_CLUSTERS(writer, /*centerGrid,*/ unstructuredGrid, include_boundary, poly_filename, enveloper);
            } else {
                vtkSmartPointer<vtkUnstructuredGridWriter> writer = vtkSmartPointer<vtkUnstructuredGridWriter>::New();
                WRITE_CLUSTERS(writer, /*centerGrid,*/ unstructuredGrid, include_boundary, poly_filename, enveloper);
            }
        }

        template <class T>
        void
        VisitUtils<T>::write_center_tracks_vtk(typename Track<T>::trackmap &track_map,
                                               const std::string &basename,
                                               size_t spatial_dimensions,
                                               bool exclude_degenerates)
        {
            typename Track<T>::trackmap::iterator tmi;

            for (tmi = track_map.begin(); tmi != track_map.end(); tmi++)
            {
                typename Track<T>::ptr track = tmi->second;

                if (exclude_degenerates && track->clusters.size() == 1)
                {
                    continue;
                }

                string filename = basename + "-track_" + boost::lexical_cast<string>(tmi->first) + ".vtk";

                ofstream f(filename.c_str());
                f << fixed << setprecision(4);

                // Write Header
                f << "# vtk DataFile Version 3.0" << endl;
                f << "Meanie3D Track" << endl;
                f << "ASCII" << endl;
                f << "DATASET UNSTRUCTURED_GRID" << endl;
                f << "POINTS " << track->size() << " FLOAT" << endl;

                // Write point coordinates out as unstructured grid

                typename std::list<typename Cluster<T>::ptr>::iterator ti;
                for (ti = track->clusters.begin(); ti != track->clusters.end(); ++ti)
                {
                    TrackCluster<T> *c = (TrackCluster<T> *)(*ti);

                    vector<T> center = c->geometrical_center();

                    for (size_t vi = 0; vi < center.size(); vi++)
                    {
                        size_t index = VTK_DIMENSION_INDEXES.empty() ? vi : VTK_DIMENSION_INDEXES[vi];

                        f << center[index] << "\t";
                    }

                    if (center.size() < 3)
                    {
                        f << "0.0";
                    }

                    f << endl;
                }

                // Write point data out. Only take the first value after coordinates

                f << endl;
                f << "POINT_DATA " << track->size() << endl;
                f << "SCALARS track_step FLOAT" << endl;
                f << "LOOKUP_TABLE default" << endl;

                size_t step = 0;

                for (ti = track->clusters.begin(); ti != track->clusters.end(); ti++)
                {
                    //f << ti->points.size() << endl;
                    f << step++ << endl;
                }

                f.close();
            }
        }

        template <class T>
        void
        VisitUtils<T>::write_center_tracks_vtk(typename ConradCluster<T>::trackmap_t &track_map,
                                               const std::string &basename,
                                               bool exclude_degenerates)
        {
            typename ConradCluster<T>::trackmap_t::const_iterator tmi;

            for (tmi = track_map.begin(); tmi != track_map.end(); tmi++)
            {
                typename ConradCluster<T>::track_t *track = tmi->second;

                if (exclude_degenerates && track->size() == 1)
                {
                    continue;
                }

                string filename = basename + "-track_" + boost::lexical_cast<string>(tmi->first) + ".vtk";

                ofstream f(filename.c_str());
                f << fixed << setprecision(4);

                // Write Header
                f << "# vtk DataFile Version 3.0" << endl;
                f << "Meanie3D Track" << endl;
                f << "ASCII" << endl;
                f << "DATASET UNSTRUCTURED_GRID" << endl;
                f << "POINTS " << track->size() << " FLOAT" << endl;

                // Write point coordinates out as unstructured grid

                typename ConradCluster<T>::track_t::const_iterator ti;

                for (ti = track->begin(); ti != track->end(); ti++)
                {
                    vector<T> center = ti->center();

                    for (size_t vi = 0; vi < center.size(); vi++)
                    {
                        f << center[vi] << "\t";
                    }

                    if (center.size() < 3)
                    {
                        f << "0.0";
                    }

                    f << endl;
                }

                // Write point data out. Only take the first value after coordinates

                f << endl;
                f << "POINT_DATA " << track->size() << endl;
                f << "SCALARS track_step FLOAT" << endl;
                f << "LOOKUP_TABLE default" << endl;

                size_t step = 0;

                for (ti = track->begin(); ti != track->end(); ti++)
                {
                    //f << ti->points.size() << endl;
                    f << step++ << endl;
                }

                f.close();
            }
        }

        template <class T>
        void
        VisitUtils<T>::write_cluster_weight_response_vtk(const string &base_name,
                                                         const typename Cluster<T>::list &clusters,
                                                         WeightFunction<T> *w,
                                                         bool useMode)
        {
            string basename(base_name);

            for (size_t ci = 0; ci < clusters.size(); ci++)
            {
                Cluster<T> *cluster = clusters[ci];

                vector<T> mode = cluster->mode;

                // Write cluster out

                boost::replace_all(basename, "/", "_");
                boost::replace_all(basename, "..", "");

                string filename = basename + "_" + boost::lexical_cast<string>(cluster->id) + ".vtk";

                ofstream f(filename.c_str());
                f << fixed << setprecision(4);

                // Write Header
                f << "# vtk DataFile Version 3.0" << endl;
                f << "Cluster weight function response" << endl;
                f << "ASCII" << endl;
                f << "DATASET UNSTRUCTURED_GRID" << endl;
                f << "POINTS " << cluster->size() << " FLOAT" << endl;

                // Write point coordinates out as unstructured grid

                size_t point_dim = cluster->at(0)->coordinate.size();

                for (size_t pi = 0; pi < cluster->size(); pi++)
                {
                    Point<T> *p = cluster->at(pi);

                    for (size_t vi = 0; vi < point_dim; vi++)
                    {
                        size_t index = VTK_DIMENSION_INDEXES.empty() ? vi : VTK_DIMENSION_INDEXES[vi];

                        f << p->coordinate[index] << "\t";
                    }

                    for (size_t j = p->coordinate.size(); j < 3; j++)
                    {
                        f << "0.0\t";
                    }

                    f << endl;
                }

                // Write point data out. Only take the first value after coordinates

                T wr = useMode ? cluster->modal_weight_response(w) : cluster->average_weight_response(w);

                f << endl;
                f << "POINT_DATA " << cluster->size() << endl;
                f << "SCALARS weight FLOAT" << endl;
                f << "LOOKUP_TABLE default" << endl;

                for (size_t pi = 0; pi < cluster->size() - 1; pi++)
                {
                    f << wr << endl;
                }

                f.close();
            }
        }

        template <class T>
        void
        VisitUtils<T>::write_cluster_meanshift_vtk(const string &base_name,
                                                   const typename Cluster<T>::list &clusters,
                                                   bool use_ids,
                                                   bool spatial_only)
        {
            std::string basename(base_name);
            boost::replace_all(basename, "/", "_");
            boost::replace_all(basename, "..", "");

            for (size_t ci = 0; ci < clusters.size(); ci++)
            {
                Cluster<T> *cluster = clusters[ci];

                typedef vector<vector<T>> vvector_t;

                vvector_t origins;

                vvector_t shifts;

                for (size_t pi = 0; pi < cluster->size(); pi++)
                {
                    typename Point<T>::ptr p = cluster->at(pi);

                    if (spatial_only)
                    {
                        origins.push_back(p->coordinate);

                        vector<T> shift = p->shift;

                        shifts.push_back(vector<T>(&shift[0], &shift[p->coordinate.size()]));
                    }
                    else
                    {
                        origins.push_back(p->values);
                        shifts.push_back(p->shift);
                    }
                }

                string filename = basename + "_" + boost::lexical_cast<string>(use_ids ? cluster->id : ci) + ".vtk";

                VisitUtils<T>::write_vectors_vtk(filename, origins, shifts, "shift");
            }
        }

        /** Helper function for drawing an ellipse (representing bandwidth and somesuch)
 * in a .curve file format.
 * @param filename
 * @param half axis data
 * @param number of segments in the curve (default 1000)
 * @param pointer to origin vector, defaults to NULL = (0,0)
 */
        template <typename T>
        void
        VisitUtils<T>::write_ellipsis_2d(const string &filename, const vector<T> &half_axis, size_t number_of_segements,
                                         const vector<T> *origin)
        {
            ofstream fs(filename.c_str());
            // fs.imbue( locale("de_DE.UTF-8") );
            fs << "# bandwidth" /* << fixed << setprecision(0)*/ << endl;
            vector<T> x0 = (origin == NULL) ? vector<T>(2, 0.0) : *origin;
            for (size_t i = 0; i < number_of_segements; i++)
            {
                float phi = i * 2 * M_PI / ((float)number_of_segements);
                float x = x0[1] + half_axis[0] * cos(phi);
                float y = x0[0] + half_axis[1] * sin(phi);
                fs << y << "\t" << x << endl;
            }
            fs.close();
        }

        /** Helper function for drawing an ellipse (representing bandwidth and somesuch)
 * in a .curve file format.
 * @param filename
 * @param half axis data
 * @param number of segments in the curve (default 1000)
 * @param pointer to origin vector, defaults to NULL = (0,0,0)
 */
        template <typename T>
        void
        VisitUtils<T>::write_ellipsis_3d(const string &filename, const vector<T> &half_axis, size_t number_of_segements,
                                         const vector<T> *origin)
        {
            ofstream fs(filename.c_str());
            // fs.imbue( locale("de_DE.UTF-8") );
            fs << "x\ty\tz\tv" << endl;
            vector<T> x0 = (origin == NULL) ? vector<T>(2, 0.0) : *origin;
            for (size_t i = 0; i < number_of_segements; i++)
            {
                float u = i * 2 * M_PI / ((float)number_of_segements);
                for (size_t j = 0; j < number_of_segements; j++)
                {
                    float v = j * 2 * M_PI / ((float)number_of_segements);
                    float x = half_axis[0] * cos(u) * sin(v);
                    float y = half_axis[1] * sin(u) * sin(v);
                    float z = half_axis[2] * cos(v);
                    if (origin != NULL)
                    {
                        x += origin->at(0);
                        y += origin->at(1);
                        z += origin->at(2);
                    }

                    fs << x << "\t" << y << "\t" << z << "\t" << 1.0 << endl;
                }
            }
            fs.close();
        }

        template <typename T>
        void
        VisitUtils<T>::write_featurespace_vtk(const string &filename, FeatureSpace<T> *fs, string variable_name)
        {
            ofstream f(filename.c_str());
            f << fixed << setprecision(8);

            // Write Header
            f << "# vtk DataFile Version 3.0" << endl;
            f << variable_name << endl;
            f << "ASCII" << endl;
            f << "DATASET UNSTRUCTURED_GRID" << endl;
            f << "POINTS " << fs->points.size() << " FLOAT" << endl;

            // Write point coordinates out as unstructured grid
            for (size_t pi = 0; pi < fs->points.size(); pi++)
            {
                typename Point<T>::ptr p = fs->points[pi];
                vector<T> c = p->values;
                for (size_t ri = 0; ri < c.size(); ri++)
                {
                    size_t index = ri;
                    if (ri < p->coordinate.size())
                    {
                        index = VTK_DIMENSION_INDEXES.empty() ? ri : VTK_DIMENSION_INDEXES[ri];
                    }
                    f << c[index] << "\t";
                }
                if (c.size() < 3)
                {
                    f << "0.0";
                }
                f << endl;
            }

            // use then LAST variable to colour the value
            f << endl;
            f << "POINT_DATA " << fs->points.size() << endl;
            f << "SCALARS " << variable_name << " FLOAT" << endl;
            f << "LOOKUP_TABLE default" << endl;
            for (size_t pi = 0; pi < fs->points.size(); pi++)
            {
                typename Point<T>::ptr p = fs->points[pi];
                vector<T> v = p->values;
                T value = v.back();
                f << value << endl;
            }
            f.close();
        }

        template <typename T>
        void
        VisitUtils<T>::write_featurespace_variables_vtk(const string &filename,
                                                        FeatureSpace<T> *fs,
                                                        const vector<string> &feature_variables,
                                                        const vector<string> &vtk_variables,
                                                        bool write_legacy)
        {
            const CoordinateSystem<T> *cs = fs->coordinate_system;

            int nx, ny, nz;
            VisitUtils<T>::get_vtk_image_dimensions(cs, nx, ny, nz);

            vector<vtkDoubleArray *> coords;
            vtkRectilinearGrid *rgrid = allocate_vtk_rectilinear_grid(cs, coords);
            rgrid->DebugOn();
            rgrid->CheckAttributes();

            for (size_t vi = 0; vi < vtk_variables.size(); vi++)
            {
                std::string var = vtk_variables[vi];
                size_t value_index = fs->spatial_rank() + vectors::index_of_first<string>(feature_variables, var);
                cout << "Writing variable " << var << " (index=" << value_index << ")" << endl;
                vtkDoubleArray *variable = vtkDoubleArray::New();
                variable->SetName(var.c_str());
                variable->SetNumberOfComponents(1); // scalar
                variable->SetNumberOfValues(nx * ny * nz);
                variable->FillComponent(0, 0);
                for (size_t i = 0; i < fs->points.size(); ++i)
                {
                    typename Point<T>::ptr p = fs->points.at(i);
                    double value = (double)p->values[value_index];
                    int gx, gy, gz;
                    VisitUtils<T>::get_vtk_gridpoint(p->gridpoint, gx, gy, gz);
                    int gridIndex = to_single_index(cs, nx, ny, nz, gx, gy, gz);
                    variable->SetValue(gridIndex, value);
                }

                rgrid->GetPointData()->AddArray(variable);
            }

            // Write out

            if (write_legacy)
            {
                // ASCII
                vtkSmartPointer<vtkRectilinearGridWriter>
                    writer = vtkSmartPointer<vtkRectilinearGridWriter>::New();
                std::string fn = filename + ".vtk";
                writer->SetFileName(fn.c_str());
#if VTK_MAJOR_VERSION <= 5
                writer->SetInput(rgrid);
#else
                writer->SetInputData(rgrid);
#endif
                writer->Write();
            }
            else
            {
                // XML
                vtkSmartPointer<vtkXMLRectilinearGridWriter>
                    xmlWriter = vtkSmartPointer<vtkXMLRectilinearGridWriter>::New();
                std::string xml_fn = filename + "." + xmlWriter->GetDefaultFileExtension();
                xmlWriter->SetFileName(xml_fn.c_str());

#if VTK_MAJOR_VERSION <= 5
                xmlWriter->SetInput(rgrid);
#else
                xmlWriter->SetInputData(rgrid);
#endif
                xmlWriter->Write();
            }

            // Clean up
            for (size_t ci = 0; ci < coords.size(); ci++)
            {
                coords.at(ci)->Delete();
            }
            rgrid->Delete();
        }

        template <typename T>
        void
        VisitUtils<T>::write_vectors_vtk(const string &filename,
                                         const vector<vector<T>> &origins,
                                         const vector<vector<T>> &vectors,
                                         string var_name)
        {
            assert(origins.size() == vectors.size());

            ofstream f(filename.c_str());
            f << fixed << setprecision(4);

            // Write Header
            f << "# vtk DataFile Version 3.0" << endl;
            f << "Vector List" << endl;
            f << "ASCII" << endl;
            f << "DATASET UNSTRUCTURED_GRID" << endl;
            f << "POINTS " << origins.size() << " FLOAT" << endl;

            // cap @ 3D

            size_t spatial_dims = origins.front().size();

            for (size_t index = 0; index < origins.size(); index++)
            {
                vector<T> v = origins[index];

                for (size_t ri = 0; ri < spatial_dims; ri++)
                {
                    size_t index = VisitUtils<T>::index_of(ri);

                    f << v[index] << "\t";
                }

                for (int i = spatial_dims; i < 3; i++)
                {
                    f << "0.0\t";
                }

                f << endl;
            }

            f << endl;
            f << "POINT_DATA " << origins.size() << endl;
            f << "VECTORS " << var_name << " FLOAT" << endl;
            for (size_t index = 0; index < vectors.size(); index++)
            {
                vector<T> v = vectors[index];

                for (size_t ri = 0; ri < spatial_dims; ri++)
                {
                    size_t index = VisitUtils<T>::index_of(ri);

                    f << v[index] << "\t";
                }

                for (int i = spatial_dims; i < 3; i++)
                {
                    f << "0.0\t";
                }

                f << endl;
            }
            f << endl;
        }

        template <typename T>
        void
        VisitUtils<T>::write_matrix_vtk(const string &filename, const boost::numeric::ublas::matrix<T> &matrix,
                                        string var_name)
        {
            ofstream f(filename.c_str());
            f << fixed << setprecision(4);

            // Write Header
            f << "# vtk DataFile Version 3.0" << endl;
            f << "Featurespace" << endl;
            f << "ASCII" << endl;
            f << "DATASET UNSTRUCTURED_GRID" << endl;
            f << "POINTS " << matrix.size1() << " FLOAT" << endl;

            // Write point coordinates out as unstructured grid

            for (size_t pi = 0; pi < matrix.size1(); pi++)
            {
                for (size_t ri = 0; ri < matrix.size2(); ri++)
                {
                    f << matrix.operator()(pi, ri) << "\t";
                }
                if (matrix.size2() < 3)
                {
                    f << "0.0";
                }

                f << endl;
            }

            // use then LAST variable to colour the value

            f << endl;
            f << "POINT_DATA " << matrix.size1() << endl;
            f << "SCALARS " << var_name << " FLOAT" << endl;
            f << "LOOKUP_TABLE default" << endl;
            for (size_t pi = 0; pi < matrix.size1(); pi++)
            {
                f << "0.0" << endl;
            }

            f.close();
        }

        template <typename T>
        void
        VisitUtils<T>::write_ublas_matrix_vtk(const string &filename, const vector<vector<T>> &origins,
                                              const vector<vector<T>> &vectors)
        {
            assert(origins.size() == vectors.size());

            ofstream f(filename.c_str());
            f << fixed << setprecision(4);

            // Write Header
            f << "# vtk DataFile Version 3.0" << endl;
            f << "Vector List" << endl;
            f << "ASCII" << endl;
            f << "DATASET UNSTRUCTURED_GRID" << endl;
            f << "POINTS " << origins.size() << " FLOAT" << endl;
            for (size_t index = 0; index < origins.size(); index++)
            {
                vector<T> v = origins[index];

                for (size_t ri = 0; ri < v.size(); ri++)
                {
                    f << v[ri] << "\t";
                }

                f << endl;
            }

            f << endl;
            f << "POINT_DATA " << origins.size() << endl;
            f << "FIELD FieldData 1" << endl;
            f << "vectors " << vectors.front().size() << " " << origins.size() << " FLOAT" << endl;
            for (size_t index = 0; index < vectors.size(); index++)
            {
                vector<T> v = vectors[index];

                for (size_t ri = 0; ri < v.size(); ri++)
                {
                    f << v[ri] << "\t";
                }

                f << endl;
            }
            f << endl;
        }

        template <typename T>
        void
        VisitUtils<T>::write_pointlist_vtk(const string &filename, typename Point<T>::list *list, size_t dim,
                                           string var_name)
        {
            ofstream f(filename.c_str());
            f << fixed << setprecision(4);

            // Write Header
            f << "# vtk DataFile Version 3.0" << endl;
            f << "point list " << endl;
            f << "ASCII" << endl;
            f << "DATASET UNSTRUCTURED_GRID" << endl;
            f << "POINTS " << list->size() << " FLOAT" << endl;

            // Write point coordinates out as unstructured grid
            for (size_t pi = 0; pi < list->size(); pi++)
            {
                Point<T> *p = list->at(pi);
                for (size_t ri = 0; ri < dim; ri++)
                {
                    f << p->values[get_vtk_index(ri)] << "\t";
                }
                if (dim < 3)
                {
                    f << "0.0";
                }
                f << endl;
            }

            f << endl;
            f << "POINT_DATA " << list->size() << endl;
            f << "SCALARS " << var_name << " FLOAT" << endl;
            f << "LOOKUP_TABLE default" << endl;

            for (size_t pi = 0; pi < list->size(); pi++)
            {
                Point<T> *p = list->at(pi);

                f << p->values[dim] << endl;
            }

            f.close();
        }

        template <typename T>
        void
        VisitUtils<T>::write_pointlist_all_vars_vtk(const string &filename, typename Point<T>::list *list,
                                                    const vector<string> &var_names)
        {
            ofstream f(filename.c_str());
            f << fixed << setprecision(4);

            // Write Header

            f << "# vtk DataFile Version 3.0" << endl;
            f << "point list " << endl;
            f << "ASCII" << endl;
            f << "DATASET UNSTRUCTURED_GRID" << endl;
            f << "POINTS " << list->size() << " FLOAT" << endl;

            // Write point coordinates out as unstructured grid

            Point<T> *p = NULL;

            for (size_t pi = 0; pi < list->size(); pi++)
            {
                p = list->at(pi);
                for (size_t ri = 0; ri < p->coordinate.size(); ri++)
                {
                    f << p->values[get_vtk_index(ri)] << "\t";
                }

                if (p->coordinate.size() < 3)
                {
                    f << "0.0";
                }

                f << endl;
            }

            for (size_t k = p->coordinate.size(); k < p->values.size(); k++)
            {
                std::string var_name = var_names.empty()
                                           ? "var_" + boost::lexical_cast<string>(k)
                                           : var_names[k - p->coordinate.size()];

                f << endl;
                f << "POINT_DATA " << list->size() << endl;
                f << "SCALARS " << var_name << " FLOAT" << endl;
                f << "LOOKUP_TABLE default" << endl;

                for (size_t pi = 0; pi < list->size(); pi++)
                {
                    Point<T> *vp = list->at(pi);

                    f << vp->values[k] << endl;
                }
            }

            f.close();
        }

        template <typename T>
        void
        VisitUtils<T>::write_pointlist_vtk(const string &filename, vector<vector<T>> *list, string var_name)
        {
            ofstream f(filename.c_str());
            f << fixed << setprecision(4);

            // Write Header

            f << "# vtk DataFile Version 3.0" << endl;
            f << "point list " << endl;
            f << "ASCII" << endl;
            f << "DATASET UNSTRUCTURED_GRID" << endl;
            f << "POINTS " << list->size() << " FLOAT" << endl;

            size_t dim = 0;

            if (list->size() > 0)
            {
                dim = list->front().size();
            }

            // Write point coordinates out as unstructured grid

            for (size_t pi = 0; pi < list->size(); pi++)
            {
                for (size_t ri = 0; ri < dim; ri++)
                {
                    f << list->at(pi)[get_vtk_index(ri)] << "\t";
                }

                f << endl;
            }

            // use then LAST dimension to 'colour' the point

            f << endl;
            f << "POINT_DATA " << list->size() << endl;
            f << "SCALARS " << var_name << " FLOAT" << endl;
            f << "LOOKUP_TABLE default" << endl;

            for (size_t pi = 0; pi < list->size(); pi++)
            {
                float value = static_cast<float>(pi) * 255.0f / static_cast<float>(list->size());

                f << value << endl;
            }

            f.close();
        }

        template <typename T>
        void
        VisitUtils<T>::write_modes_vtk(const string &filename, const vector<vector<T>> &list,
                                       const vector<size_t> &trajectory_lenghts, string var_name)
        {
            ofstream f(filename.c_str());
            f << fixed << setprecision(4);

            // Write Header

            f << "# vtk DataFile Version 3.0" << endl;
            f << "point list " << endl;
            f << "ASCII" << endl;
            f << "DATASET UNSTRUCTURED_GRID" << endl;
            f << "POINTS " << list.size() << " FLOAT" << endl;

            size_t dim = 0;
            if (list.size() > 0)
            {
                dim = list.front().size();
            }

            // Write point coordinates out as unstructured grid

            for (size_t pi = 0; pi < list.size(); pi++)
            {
                for (size_t ri = 0; ri < dim; ri++)
                {
                    f << list[pi][get_vtk_index(ri)] << "\t";
                }

                f << endl;
            }

            // use then LAST dimension to 'colour' the point

            f << endl;
            f << "POINT_DATA " << list.size() << endl;
            f << "SCALARS " << var_name << " FLOAT" << endl;
            f << "LOOKUP_TABLE default" << endl;

            for (size_t pi = 0; pi < list.size(); pi++)
            {
                float value = trajectory_lenghts[pi];

                f << value << endl;
            }

            f.close();
        }

        template <typename T>
        void
        VisitUtils<T>::write_radolan_vtk(const string &filename, const string &outfile,
                                         Radolan::RDDataType *threshold)
        {
            using namespace Radolan;

            RDScan *scan = RDAllocateScan();

            if (RDReadScan(filename.c_str(), scan, true) >= 0)
            {
                // figure out how many valid values we have

                size_t number_of_valid_points = 0;

                if (threshold == NULL)
                {
                    number_of_valid_points = scan->dimLat * scan->dimLon;
                }
                else
                {
                    for (size_t i = 0; i < (scan->dimLat * scan->dimLon); i++)
                    {
                        RDDataType value = scan->data[i];
                        if (value >= *threshold)
                        {
                            number_of_valid_points++;
                        }
                    }
                }

                ofstream f(outfile.c_str());
                f << fixed << setprecision(4);

                // Write Header

                f << "# vtk DataFile Version 3.0" << endl;
                f << "point list " << endl;
                f << "ASCII" << endl;
                f << "DATASET UNSTRUCTURED_GRID" << endl;
                f << "POINTS " << number_of_valid_points << " FLOAT" << endl;

                RDCoordinateSystem rcs(scan->header.scanType);

                int lat, lon;

                for (lat = 0; lat < scan->dimLat; lat++)
                {
                    for (lon = 0; lon < scan->dimLon; lon++)
                    {
                        RDDataType value = scan->data[lat * scan->dimLon + lon];

                        if (threshold == NULL || (threshold != NULL && value >= *threshold))
                        {
                            RDCartesianPoint p = rcs.cartesianCoordinate(rdGridPoint(lon, lat));

                            f << p.x << "\t" << p.y << "\t0.0" << endl;
                        }
                    }
                }

                // use then LAST dimension to 'colour' the point

                f << endl;
                f << "POINT_DATA " << number_of_valid_points << endl;
                f << "SCALARS "
                  << "reflectivity"
                  << " FLOAT" << endl;
                f << "LOOKUP_TABLE default" << endl;

                for (size_t i = 0; i < scan->dimLat * scan->dimLon; i++)
                {
                    RDDataType value = scan->data[i];

                    if (threshold == NULL || (threshold != NULL && value >= *threshold))
                    {
                        f << value << endl;
                    }
                }

                f.close();
            }
        }

        template <typename T>
        void
        VisitUtils<T>::write_weights(const string &filename,
                                     const string &var_name,
                                     typename Point<T>::list *list,
                                     const vector<T> &weights,
                                     bool restrict_to_2D)
        {
            ofstream f(filename.c_str());
            f << fixed << setprecision(4);

            // Write Header

            f << "# vtk DataFile Version 3.0" << endl;
            f << "Weights" << endl;
            f << "ASCII" << endl;
            f << "DATASET UNSTRUCTURED_GRID" << endl;
            f << "POINTS " << list->size() << " FLOAT" << endl;

            size_t dim = 0;
            if (list->size() > 0)
            {
                dim = list->front()->values.size();
            }

            // Write point coordinates out as unstructured grid
            for (size_t pi = 0; pi < list->size(); pi++)
            {
                Point<T> *p = list->at(pi);
                // NOTE: modified to be put on top of 2D plots
                for (size_t ri = 0; ri < (restrict_to_2D ? 2 : dim); ri++)
                {
                    f << p->values[get_vtk_index(ri)] << "\t";
                }
                f << "0.0";
                f << endl;
            }

            // Write weights
            f << endl;
            f << "POINT_DATA " << list->size() << endl;
            f << "SCALARS " << var_name << " FLOAT" << endl;
            f << "LOOKUP_TABLE default" << endl;
            for (size_t pi = 0; pi < list->size(); pi++)
            {
                f << weights[pi] << endl;
            }
        }

        template <typename T>
        void
        VisitUtils<T>::write_shift_vectors(const string &filename,
                                           FeatureSpace<T> *fs,
                                           bool spatial_only)
        {
            vector<vector<T>> origins, vectors;

            // Write point coordinates out as unstructured grid

            for (size_t pi = 0; pi < fs->points.size(); pi++)
            {
                Point<T> *p = fs->points[pi];

                if (spatial_only)
                {
                    origins.push_back(p->coordinate);

                    vectors.push_back(fs->spatial_component(p->shift));
                }
                else
                {
                    origins.push_back(p->values);

                    vectors.push_back(p->shift);
                }
            }

            write_vectors_vtk(filename, origins, vectors);
        }

        template <typename T>
        void
        VisitUtils<T>::write_weight_function_response(const string &filename,
                                                      FeatureSpace<T> *fs,
                                                      WeightFunction<T> *weight_function,
                                                      bool write_legacy)
        {
            const CoordinateSystem<T> *cs = fs->coordinate_system;
            int nx, ny, nz;
            VisitUtils<T>::get_vtk_image_dimensions(cs, nx, ny, nz);
            vector<vtkDoubleArray *> coords;
            vtkRectilinearGrid *rgrid = allocate_vtk_rectilinear_grid(cs, coords);

            vtkDoubleArray *variable = vtkDoubleArray::New();
            variable->SetName("weight");
            variable->SetNumberOfComponents(1); // scalar
            variable->SetNumberOfValues(nx * ny * nz);
            variable->FillComponent(0, 0);

            for (size_t i = 0; i < fs->points.size(); ++i)
            {
                typename Point<T>::ptr p = fs->points.at(i);
                double value = (double)weight_function->operator()(p);
                int gx, gy, gz;
                VisitUtils<T>::get_vtk_gridpoint(p->gridpoint, gx, gy, gz);
                int gridIndex = to_single_index(cs, nx, ny, nz, gx, gy, gz);
                variable->SetValue(gridIndex, value);
            }
            rgrid->GetPointData()->AddArray(variable);

            // Write out
            if (write_legacy)
            {
                // ASCII
                vtkSmartPointer<vtkRectilinearGridWriter>
                    writer = vtkSmartPointer<vtkRectilinearGridWriter>::New();

                std::string fn = filename + ".vtk";
                writer->SetFileName(fn.c_str());
#if VTK_MAJOR_VERSION <= 5
                writer->SetInput(rgrid);
#else
                writer->SetInputData(rgrid);
#endif
                writer->Write();
            }
            else
            {
                // XML
                vtkSmartPointer<vtkXMLRectilinearGridWriter>
                    xmlWriter = vtkSmartPointer<vtkXMLRectilinearGridWriter>::New();
                std::string xml_fn = filename + "." + xmlWriter->GetDefaultFileExtension();
                xmlWriter->SetFileName(xml_fn.c_str());
#if VTK_MAJOR_VERSION <= 5
                xmlWriter->SetInput(rgrid);
#else
                xmlWriter->SetInputData(rgrid);
#endif
                xmlWriter->Write();
            }

            // Clean up
            for (size_t ci = 0; ci < coords.size(); ci++)
            {
                coords.at(ci)->Delete();
            }
            rgrid->Delete();
        }

        template <typename T>
        void
        VisitUtils<T>::write_multiarray_vtk(const std::string &filename,
                                            const std::string &variable_name,
                                            const CoordinateSystem<T> *cs,
                                            MultiArray<T> *array,
                                            bool write_legacy)
        {

            class Functor : public MultiArray<T>::ForEachFunctor
            {
            private:
                vtkDoubleArray *m_array;
                const CoordinateSystem<T> *m_cs;
                int nx, ny, nz;

            public:
                Functor(vtkDoubleArray *array, const CoordinateSystem<T> *cs) : m_array(array), m_cs(cs)
                {
                    VisitUtils<T>::get_vtk_image_dimensions(cs, nx, ny, nz);
                };

                void
                operator()(const vector<int> &index, const T value)
                {
                    int gx, gy, gz;
                    VisitUtils<T>::get_vtk_gridpoint(index, gx, gy, gz);
                    int gridIndex = VisitUtils<T>::to_single_index(m_cs, nx, ny, nz, gx, gy, gz);
                    m_array->SetValue(gridIndex, value);
                }
            };

            int nx, ny, nz;
            VisitUtils<T>::get_vtk_image_dimensions(cs, nx, ny, nz);

            vector<vtkDoubleArray *> coords;
            vtkRectilinearGrid *rgrid = allocate_vtk_rectilinear_grid(cs, coords);

            vtkDoubleArray *variable = vtkDoubleArray::New();
            variable->SetName(variable_name.c_str());
            variable->SetNumberOfComponents(1); // scalar
            variable->SetNumberOfValues(array->size());

            Functor f(variable, cs);
            array->for_each(&f);

            rgrid->GetPointData()->AddArray(variable);

            // Write out

            boost::filesystem::path path(filename);
            std::string basename = path.filename().stem().generic_string();

            if (write_legacy)
            {
                // ASCII

                vtkSmartPointer<vtkRectilinearGridWriter>
                    writer = vtkSmartPointer<vtkRectilinearGridWriter>::New();

                std::string fn = basename + ".vtk";
                writer->SetFileName(fn.c_str());

#if VTK_MAJOR_VERSION <= 5
                writer->SetInput(rgrid);
#else
                writer->SetInputData(rgrid);
#endif
                writer->Write();
            }
            else
            {
                // XML

                vtkSmartPointer<vtkXMLRectilinearGridWriter>
                    xmlWriter = vtkSmartPointer<vtkXMLRectilinearGridWriter>::New();
                std::string xml_fn = basename + "." + xmlWriter->GetDefaultFileExtension();

                xmlWriter->SetFileName(xml_fn.c_str());

#if VTK_MAJOR_VERSION <= 5
                xmlWriter->SetInput(rgrid);
#else
                xmlWriter->SetInputData(rgrid);
#endif
                xmlWriter->Write();
            }

            // Clean up

            for (size_t ci = 0; ci < coords.size(); ci++)
            {
                coords.at(ci)->Delete();
            }

            rgrid->Delete();
        };

        template <typename T>
        void
        VisitUtils<T>::write_multiarray_vtk(const std::string &filename,
                                            const std::string &variable_name,
                                            const CoordinateSystem<T> *cs,
                                            const MultiArray<bool> *array)
        {

            class Functor : public MultiArray<bool>::ForEachFunctor
            {
                std::ofstream &m_stream;
                const CoordinateSystem<T> *m_coord_system;
                bool m_write_coords;

            public:
                Functor(std::ofstream &stream,
                        const CoordinateSystem<T> *cs,
                        bool write_coords)
                    : m_stream(stream), m_coord_system(cs), m_write_coords(write_coords)
                {
                }

                void
                operator()(const vector<int> &index, const bool value)
                {
                    if (m_write_coords)
                    {
                        for (size_t ri = 0; ri < 3; ri++)
                        {
                            if (ri < index.size())
                            {
                                vector<T> coord(index.size());
                                m_coord_system->lookup(index, coord);
                                m_stream << coord[get_vtk_index(ri)] << "\t";
                            }
                            else
                            {
                                m_stream << 0.0 << "\t";
                            }
                        }
                        m_stream << endl;
                    }
                    else
                    {
                        m_stream << (value ? 1.0 : 0.0) << endl;
                    }
                }
            };

            ofstream f(filename.c_str());
            f << fixed << setprecision(4);

            // Write Header

            f << "# vtk DataFile Version 3.0" << endl;
            f << variable_name << endl;
            f << "ASCII" << endl;
            f << "DATASET UNSTRUCTURED_GRID" << endl;
            f << "POINTS " << array->size() << " FLOAT" << endl;

            // Write point coordinates out as unstructured grid

            Functor f1(f, cs, true);
            array->for_each(&f1);

            // Write weights

            f << endl;
            f << "POINT_DATA " << array->size() << endl;
            f << "SCALARS " << variable_name << " FLOAT" << endl;
            f << "LOOKUP_TABLE default" << endl;

            Functor f2(f, cs, false);
            array->for_each(&f2);
        };
    } // namespace utils
} // namespace m3D

#endif
#endif