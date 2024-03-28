# C++ Ray Tracer with AABBTree

C++ program implementing a simple ray tracer with an AABBTree for efficient collision detection. The ray tracer generates an image by tracing rays through a scene and computing intersections with objects.

## Dependencies

- **Eigen:** A C++ template library for linear algebra.
- **STB Image Write:** A lightweight image file writer library.

## Classes and Structures

### `AABBTree`

- Represents an axis-aligned bounding box (AABB) tree used for spatial partitioning.
- Contains nested class `Node` representing a node in the AABB tree.

### `Node`

- Represents a node in the AABB tree.
- Contains information such as bounding box (`bbox`), parent index (`parent`), left child index (`left`), right child index (`right`), and triangle index (`triangle`).

## Scene Setup and Global Variables

- Defines global variables such as file paths, camera settings, mesh data, material properties, light positions, and grid size.
- Provides a function `setup_scene()` to initialize the scene.

## BVH Code

- Implements methods for building the AABB tree from a triangle mesh.
- Defines functions for computing bounding boxes and centroids of triangles.

## Intersection Code

- Contains functions for ray-triangle intersection and ray-box intersection.
- Implements a function `find_nearest_object()` to find the closest intersecting object in the scene.

## Raytracer Code

- Implements the main ray tracing algorithm in the function `shoot_ray()`.
- Computes lighting contributions (ambient and diffuse) and applies the rendering equation.
- Provides a function `raytrace_scene()` to generate the final image.

## Compilation and Execution

To compile and run the program:

1. Ensure Eigen and STB Image Write libraries are installed.
2. Compile the source code using a C++ compiler (e.g., g++).
3. Execute the compiled binary to generate the ray-traced image.

