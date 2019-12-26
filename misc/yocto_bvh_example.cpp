#include <fmt/format.h>
#include <yocto_bvh.h>

// Summary
// - make 8hedron
// - make_triangles_bvh
// - intersect_triangles_bvh with various rays

int main(int argc, char** argv) {
  using std::vector;
  using yocto::vec3f, yocto::vec3i;

  yocto::bvh_tree tree;
  vector<vec3f> positions;
  vector<vec3i> triangles;
  {
    // make 8hedron
    vec3f x = {1, 0, 0}, y = {0, 1, 0}, z = {0, 0, 1};
    positions = {
       z,   x,   y,
       z,   y,  -x,
       z,  -x,  -y,
       z,  -y,   x,
       x,   z,   y,
       y,   z,  -x,
      -x,   z,  -y,
      -y,   z,   x,
    };
    for (auto i = 0; i < 8; i++) {
      int k = 3 * i;
      triangles.push_back({k, k + 1, k + 2});
    }
  }

  // construct bvh tree
  yocto::make_triangles_bvh(tree, triangles, positions, {});

  // ray test
  {
    yocto::ray3f ray;
    ray.o = {.3, .3, 2}; ray.d = {0, 0, -1};
    yocto::bvh_intersection result = intersect_triangles_bvh(tree, triangles, positions, ray);
    fmt::print("hit: {}\nelement: {}\nuv: [{}, {}]\n",
              result.hit, result.element, result.uv.x, result.uv.y);
  }
  {
    yocto::ray3f ray;
    ray.o = {-.3, -.3, 2}; ray.d = {0, 0, -1};
    yocto::bvh_intersection result = intersect_triangles_bvh(tree, triangles, positions, ray);
    fmt::print("hit: {}\nelement: {}\nuv: [{}, {}]\n",
              result.hit, result.element, result.uv.x, result.uv.y);
  }
  return 0;
}
