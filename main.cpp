////////////////////////////////////////////////////////////////////////////////
// C++ include
#include <iostream>
#include <string>
#include <vector>
#include <limits>
#include <fstream>
#include <algorithm>
#include <numeric>
#include <stack>

// Utilities for the Assignment
#include "utils.h"

// Image writing library
#define STB_IMAGE_WRITE_IMPLEMENTATION // Do not include this line twice in your project!
#include "stb_image_write.h"

// Shortcut to avoid Eigen:: everywhere, DO NOT USE IN .h
using namespace Eigen;

////////////////////////////////////////////////////////////////////////////////
// Class to store tree
////////////////////////////////////////////////////////////////////////////////
class AABBTree
{
public:
    class Node
    {
    public:
        AlignedBox3d bbox;
        int parent;   // Index of the parent node (-1 for root)
        int left;     // Index of the left child (-1 for a leaf)
        int right;    // Index of the right child (-1 for a leaf)
        int triangle; // Index of the node triangle (-1 for internal nodes)
    };

    std::vector<Node> nodes;
    int root;

    AABBTree() = default;                           // Default empty constructor
    AABBTree(const MatrixXd& V, const MatrixXi& F); // Build a BVH from an existing mesh

private:
    // builds the bvh recursively
    int build_recursive(const MatrixXd& V, const MatrixXi& F, const MatrixXd& centroids, int from, int to, int parent, std::vector<int>& triangles);
};

////////////////////////////////////////////////////////////////////////////////
// Scene setup, global variables
////////////////////////////////////////////////////////////////////////////////
const std::string data_dir = DATA_DIR;
const std::string filename("raytrace.png");
const std::string mesh_filename(data_dir + "dragon.off");

//Camera settings
const double focal_length = 2;
const double field_of_view = 0.7854; //45 degrees
const bool is_perspective = true;
const Vector3d camera_position(0, 0, 2);

// Triangle Mesh
MatrixXd vertices; // n x 3 matrix (n points)
MatrixXi facets;   // m x 3 matrix (m triangles)
AABBTree bvh;

//Material for the object, same material for all objects
const Vector4d obj_ambient_color(0.0, 0.5, 0.0, 0);
const Vector4d obj_diffuse_color(0.5, 0.5, 0.5, 0);
const Vector4d obj_specular_color(0.2, 0.2, 0.2, 0);
const double obj_specular_exponent = 256.0;
const Vector4d obj_reflection_color(0.7, 0.7, 0.7, 0);

// Precomputed (or otherwise) gradient vectors at each grid node
const int grid_size = 20;
std::vector<std::vector<Vector2d>> grid;

//Lights
std::vector<Vector3d> light_positions;
std::vector<Vector4d> light_colors;
//Ambient light
const Vector4d ambient_light(0.2, 0.2, 0.2, 0);

//Fills the different arrays
void setup_scene()
{
    //Loads file
    std::ifstream in(mesh_filename);
    std::string token;
    in >> token;
    int nv, nf, ne;
    in >> nv >> nf >> ne;
    vertices.resize(nv, 3);
    facets.resize(nf, 3);
    for (int i = 0; i < nv; ++i)
    {
        in >> vertices(i, 0) >> vertices(i, 1) >> vertices(i, 2);
    }
    for (int i = 0; i < nf; ++i)
    {
        int s;
        in >> s >> facets(i, 0) >> facets(i, 1) >> facets(i, 2);
        assert(s == 3);
    }

    //setup tree
    bvh = AABBTree(vertices, facets);

    //Lights
    light_positions.emplace_back(8, 8, 0);
    light_colors.emplace_back(16, 16, 16, 0);

    light_positions.emplace_back(6, -8, 0);
    light_colors.emplace_back(16, 16, 16, 0);

    light_positions.emplace_back(4, 8, 0);
    light_colors.emplace_back(16, 16, 16, 0);

    light_positions.emplace_back(2, -8, 0);
    light_colors.emplace_back(16, 16, 16, 0);

    light_positions.emplace_back(0, 8, 0);
    light_colors.emplace_back(16, 16, 16, 0);

    light_positions.emplace_back(-2, -8, 0);
    light_colors.emplace_back(16, 16, 16, 0);

    light_positions.emplace_back(-4, 8, 0);
    light_colors.emplace_back(16, 16, 16, 0);
}

////////////////////////////////////////////////////////////////////////////////
// BVH Code
////////////////////////////////////////////////////////////////////////////////

AlignedBox3d bbox_from_triangle(const Vector3d& a, const Vector3d& b, const Vector3d& c)
{
    AlignedBox3d box;
    box.extend(a);
    box.extend(b);
    box.extend(c);
    return box;
}

AABBTree::AABBTree(const MatrixXd& V, const MatrixXi& F)
{
    // Compute the centroids of all the triangles in the input mesh
    MatrixXd centroids(F.rows(), V.cols());
    centroids.setZero();
    for (int i = 0; i < F.rows(); ++i)
    {
        for (int k = 0; k < F.cols(); ++k)
        {
            centroids.row(i) += V.row(F(i, k));
        }
        centroids.row(i) /= F.cols();
    }

    // Vector containing the list of triangle indices
    std::vector<int> triangles(F.rows());
    std::iota(triangles.begin(), triangles.end(), 0);

    root = build_recursive(V, F, centroids, 0, triangles.size(), -1, triangles);
}

int AABBTree::build_recursive(const MatrixXd& V, const MatrixXi& F, const MatrixXd& centroids, int from, int to, int parent, std::vector<int>& triangles)
{
    if (to - from == 0)
    {
        return -1;
    }

    if (to - from == 1)
    {
        AABBTree::Node newNode;
        newNode.triangle = triangles[from];
        newNode.parent = parent;
        newNode.right = -1;
        newNode.left = -1;
        
        
        Vector3i triangleVertexIndices = F.row(newNode.triangle);
        Vector3d a = V.row(triangleVertexIndices(0));
        Vector3d b = V.row(triangleVertexIndices(1));
        Vector3d c = V.row(triangleVertexIndices(2));
        newNode.bbox = bbox_from_triangle(a, b, c);

        nodes.push_back(newNode);

        return nodes.size() - 1; 
    }

    Vector3d range = centroids.colwise().maxCoeff() - centroids.colwise().minCoeff();
    int longest_dim = std::max_element(range.data(), range.data() + range.size()) - range.data();
    std::sort(triangles.begin() + from, triangles.begin() + to, [&](int f1, int f2) {
        return centroids(f1, longest_dim) < centroids(f2, longest_dim); 
        });

    
    AlignedBox3d centroid_box; 
    for (int i = from; i < to; ++i)
    {
        Vector3i triangleVertexIndices = F.row(triangles[i]);
        Vector3d a = V.row(triangleVertexIndices(0));
        Vector3d b = V.row(triangleVertexIndices(1));
        Vector3d c = V.row(triangleVertexIndices(2));
        centroid_box.extend(bbox_from_triangle(a, b, c));
    }

    AABBTree::Node node;
    node.bbox = centroid_box;
    node.parent = parent;

    int mid = (from + to) / 2;

    node.left = build_recursive(V, F, centroids, from, mid, nodes.size() - 1, triangles);
    node.right = build_recursive(V, F, centroids, mid, to, nodes.size() - 1, triangles);

    nodes.push_back(node);

    return nodes.size() - 1; 
}

////////////////////////////////////////////////////////////////////////////////
// Intersection code
////////////////////////////////////////////////////////////////////////////////

double ray_triangle_intersection(const Vector3d& ray_origin, const Vector3d& ray_direction, const Vector3d& a, const Vector3d& b, const Vector3d& c, Vector3d& p, Vector3d& N)
{
    const Vector3d u = b - a;
    const Vector3d v = c - a;

    Vector3d uvt;
    Matrix3d intersectionMatrix;
    intersectionMatrix.col(0) = -(u).transpose();
    intersectionMatrix.col(1) = -(v).transpose();
    intersectionMatrix.col(2) = ray_direction.transpose();

    uvt = intersectionMatrix.inverse() * (a - ray_origin);

    bool isInsideTriangle = uvt.x() >= 0 &&
                            uvt.y() >= 0 && 
                            uvt.z() >= 0 &&
                            uvt.x() + uvt.y() <= 1;

    if (isInsideTriangle) {
        p = ray_origin + uvt.z() * ray_direction;
        N = (u.cross(v)).normalized();
        return uvt.z();
    }

    return -1;
}

bool ray_box_intersection(const Vector3d& ray_origin, const Vector3d& ray_direction, const AlignedBox3d& box) {
    Vector3d invRay = ray_direction.cwiseInverse(); 
    Vector3d tMin = (box.min() - ray_origin).cwiseProduct(invRay);
    Vector3d tMax = (box.max() - ray_origin).cwiseProduct(invRay);

    if (invRay.x() < 0) std::swap(tMin.x(), tMax.x());
    if (invRay.y() < 0) std::swap(tMin.y(), tMax.y());
    if (invRay.z() < 0) std::swap(tMin.z(), tMax.z());

    double entryTime = std::max({ tMin.x(), tMin.y(), tMin.z() });
    double exitTime = std::min({ tMax.x(), tMax.y(), tMax.z() });

    return entryTime <= exitTime && exitTime >= 0;
}

//Finds the closest intersecting object returns its index
//In case of intersection it writes into p and N (intersection point and normals)
bool find_nearest_object(const Vector3d& ray_origin, const Vector3d& ray_direction, Vector3d& p, Vector3d& N) {
    double closest_t = std::numeric_limits<double>::max();
    bool res = false;

    std::stack<int> stack;
    stack.push(bvh.root);

    while (!stack.empty()) {
        int curr_node = stack.top();
        stack.pop();
        AABBTree::Node node = bvh.nodes[curr_node];

        if (ray_box_intersection(ray_origin, ray_direction, node.bbox)) {
            if (node.right == -1 && node.left == -1) {
                Vector3d a = vertices.row(facets(node.triangle, 0));
                Vector3d b = vertices.row(facets(node.triangle, 1));
                Vector3d c = vertices.row(facets(node.triangle, 2));

                Vector3d tmp_p, tmp_N;
                double t = ray_triangle_intersection(ray_origin, ray_direction, a, b, c, tmp_p, tmp_N);

                if (t >= 0 && t < closest_t) {
                    closest_t = t;
                    p = tmp_p;
                    N = tmp_N;
                    res = true;
                }
            }
            else {
                stack.push(node.right);
                stack.push(node.left);
            }
        }
    }
    return res;
}

////////////////////////////////////////////////////////////////////////////////
// Raytracer code
////////////////////////////////////////////////////////////////////////////////

Vector4d shoot_ray(const Vector3d& ray_origin, const Vector3d& ray_direction)
{
    //Intersection point and normal, these are output of find_nearest_object
    Vector3d p, N;

    const bool nearest_object = find_nearest_object(ray_origin, ray_direction, p, N);

    if (!nearest_object)
    {
        // Return a transparent color
        return Vector4d(0, 0, 0, 0);
    }

    // Ambient light contribution
    const Vector4d ambient_color = obj_ambient_color.array() * ambient_light.array();

    // Punctual lights contribution (direct lighting)
    Vector4d lights_color(0, 0, 0, 0);
    for (int i = 0; i < light_positions.size(); ++i)
    {
        const Vector3d& light_position = light_positions[i];
        const Vector4d& light_color = light_colors[i];

        Vector4d diff_color = obj_diffuse_color;

        // Diffuse contribution
        const Vector3d Li = (light_position - p).normalized();
        const Vector4d diffuse = diff_color * std::max(Li.dot(N), 0.0);

        // Specular contribution
        const Vector3d Hi = (Li - ray_direction).normalized();
        const Vector4d specular = obj_specular_color * std::pow(std::max(N.dot(Hi), 0.0), obj_specular_exponent);
        // Vector3d specular(0, 0, 0);

        // Attenuate lights according to the squared distance to the lights
        const Vector3d D = light_position - p;
        lights_color += (diffuse + specular).cwiseProduct(light_color) / D.squaredNorm();
    }

    // Rendering equation
    Vector4d C = ambient_color + lights_color;

    //Set alpha to 1
    C(3) = 1;

    return C;
}

////////////////////////////////////////////////////////////////////////////////

void raytrace_scene()
{
    std::cout << "Simple ray tracer." << std::endl;

    int w = 640;
    int h = 480;
    MatrixXd R = MatrixXd::Zero(w, h);
    MatrixXd G = MatrixXd::Zero(w, h);
    MatrixXd B = MatrixXd::Zero(w, h);
    MatrixXd A = MatrixXd::Zero(w, h); // Store the alpha mask

    // The camera always points in the direction -z
    // The sensor grid is at a distance 'focal_length' from the camera center,
    // and covers an viewing angle given by 'field_of_view'.
    double aspect_ratio = double(w) / double(h);

    const double halfView = field_of_view / 2;
    double image_y = (sin(halfView) * (focal_length / (sin((3.145 / 2) - halfView))));
    double image_x = aspect_ratio * image_y;

    // The pixel grid through which we shoot rays is at a distance 'focal_length'
    const Vector3d image_origin(-image_x, image_y, camera_position[2] - focal_length);
    const Vector3d x_displacement(2.0 / w * image_x, 0, 0);
    const Vector3d y_displacement(0, -2.0 / h * image_y, 0);

    for (unsigned i = 0; i < w; ++i)
    {
        for (unsigned j = 0; j < h; ++j)
        {
            const Vector3d pixel_center = image_origin + (i + 0.5) * x_displacement + (j + 0.5) * y_displacement;

            // Prepare the ray
            Vector3d ray_origin;
            Vector3d ray_direction;

            if (is_perspective)
            {
                // Perspective camera
                ray_origin = camera_position;
                ray_direction = (pixel_center - camera_position).normalized();
            }
            else
            {
                // Orthographic camera
                ray_origin = pixel_center;
                ray_direction = Vector3d(0, 0, -1);
            }

            const Vector4d C = shoot_ray(ray_origin, ray_direction);
            R(i, j) = C(0);
            G(i, j) = C(1);
            B(i, j) = C(2);
            A(i, j) = C(3);
        }
    }

    // Save to png
    write_matrix_to_png(R, G, B, A, filename);
}

////////////////////////////////////////////////////////////////////////////////

int main(int argc, char* argv[])
{
    setup_scene();

    raytrace_scene();
    return 0;
}