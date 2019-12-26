#pragma once

#include <cgltf.h>
#include <glm/ext/matrix_clip_space.hpp>
#include <stb_image.h>

#include "utils.hpp"

//
// Initial Strategy
// - no hierarchy (simply Scene <--1-many--> Node)
// - shared_ptr for automatic reference counting
// - GPU resource is owned by "XxxRR" counterpart ("RR" stands for "Render Resource")
//   which is not allocated until rendering it (cf. `SceneRenderer` in scene_example.cpp).
//

namespace toy {
namespace scene {

namespace {
using std::string, std::vector, std::shared_ptr, std::unique_ptr;
using glm::ivec2, glm::fvec2, glm::fvec3, glm::fvec4, glm::fmat3, glm::fmat4;
}

struct MeshRR; struct TextureRR; struct MeshBVH;
struct Node; struct Mesh; struct Texture; struct Material;

struct VertexAttrs {
  fvec3 position;
  fvec3 normal;
  fvec4 tangent;
  fvec2 texcoord;
  fvec4 color = {1, 1, 1, 1};
};

struct Mesh {
  unique_ptr<MeshRR> rr_;
  unique_ptr<MeshBVH> bvh_;
  string name_;
  vector<uint16_t> indices_; // 2**16 = 65536
  vector<VertexAttrs> vertices_;
};

struct Texture {
  unique_ptr<TextureRR> rr_;
  string name_;
  string filename_;
  ivec2 size_;
};

struct Material {
  string name_;
  fvec4 base_color_factor_ = {1, 1, 1, 1};
  shared_ptr<Texture> base_color_texture_;
  bool use_base_color_texture_ = true;
};

struct Node {
  string name_;
  fmat4 transform_ = fmat4{1};
  shared_ptr<Mesh> mesh_;
  shared_ptr<Material> material_;
};

struct Camera {
  fmat4 transform_ = fmat4{1};
  float yfov_ = std::acos(-1) / 3.; // 60deg
  float aspect_ratio_ = 16. / 9.;
  float znear_ = 0.001;
  float zfar_ = 1000;

  glm::fmat4 getPerspectiveProjection() const {
    return glm::perspectiveRH_NO(yfov_, aspect_ratio_, znear_, zfar_);
  }
};


struct Scene {
  Camera camera_;
  vector<shared_ptr<Node>> nodes_;
};

struct AssetRepository {
  string name_;
  string filename_;
  vector<shared_ptr<Node>> nodes_;
  vector<shared_ptr<Material>> materials_;
  vector<shared_ptr<Mesh>> meshes_;
  vector<shared_ptr<Texture>> textures_;
};


//
// RR counterparts
//

struct MeshRR {
  Mesh& owner_;
  utils::gl::VertexRenderer base_;

  MeshRR(Mesh& owner) : owner_{owner} {
    base_.setData(owner.vertices_, owner.indices_);
  }
};

struct TextureRR {
  Texture& owner_;
  utils::gl::Texture base_;

  TextureRR(Texture& owner) : owner_{owner} {
    auto data = stbi_load(owner_.filename_.data(), &owner_.size_.x, &owner_.size_.y, nullptr, 4);
    TOY_ASSERT_CUSTOM(data, fmt::format("stbi_load failed: {}", owner_.filename_));
    base_.setData(owner_.size_, data);
    stbi_image_free(data);
  }
};

// TODO: implement bvh (currently traverses all triangle...)
struct MeshBVH {
  Mesh& owner_;
  MeshBVH(Mesh& mesh) : owner_{mesh} {}

  struct RayTestResult {
    bool hit;
    std::array<fvec3, 3> face;
    fvec3 point;
    float t;
  };

  RayTestResult rayTest(const fvec3& src, const fvec3& dir) {
    auto& vs = owner_.vertices_;
    auto& is = owner_.indices_;
    RayTestResult result = { .hit = false, .t = FLT_MAX };

    for (auto k : utils::Range{is.size() / 3}) {
      size_t l = 3 * k;
      fvec3& p0 = vs[is[l + 0]].position;
      fvec3& p1 = vs[is[l + 1]].position;
      fvec3& p2 = vs[is[l + 2]].position;
      auto tmp_result = utils::hit::Ray_Triangle(src, dir, p0, p1, p2);
      if (!tmp_result.valid) { continue; }

      fvec2& uv = tmp_result.uv;
      bool hit = uv.x >= 0 && uv.y >= 0 && (uv.x + uv.y <= 1);
      if (!hit) { continue; }
      if (!(tmp_result.t < result.t)) { continue; }

      result.hit = true;
      result.t = tmp_result.t;
      result.face = {p0, p1, p2};
      result.point = tmp_result.p;
    }

    return result;
  }
};

//
// gltf importer with cgltf
// cf. https://github.com/KhronosGroup/glTF/blob/master/specification/2.0/README.md
//
namespace gltf {

  // This doesn't work for "integer-encoded" float.
  // cf. cgltf_accessor_read_index, cgltf_calc_size
  inline void* readAccessor(const cgltf_accessor* accessor, size_t index) {
    cgltf_size offset = accessor->offset + accessor->buffer_view->offset;
    uint8_t* element = (uint8_t*)accessor->buffer_view->buffer->data;
    return (void*)(element + offset + accessor->stride * index);
  }

  // "mydir/myfile" => "myfile"
  // "myfile" => "myfile"
  inline auto getBasename = [](const std::string& s) {
    auto npos = std::string::npos;
    auto pos = s.rfind("/");
    return pos == npos ? s : s.substr(pos + 1, npos);
  };

  // "mydir/myfile" => "mydir"
  inline auto getDirname = [](const std::string& s) {
    return std::string{s, 0, s.rfind('/')};
  };

  // Use non-interleaved vertex data as temporary
  struct Primitive {
    vector<fvec3> positions;
    vector<fvec3> normals;
    vector<fvec4> tangents;
    vector<fvec2> texcoords;
    vector<fvec4> colors;
    vector<uint16_t> indices;
  };


  //
  // - Strategy
  //   - each cgltf_primitive becomes Node
  //     - so cgltf_primitive.material becomses Node.material_
  //   - only triangles
  //
  // - Assertions
  //   - indices type is uint16_t (this is the case all but SciFiHelmet (https://github.com/KhronosGroup/glTF-Sample-Models/blob/master/2.0/SciFiHelmet)
  //   - vertex attribute is already float (i.e. not "integer-encoded")
  //   - image source is file
  //   - single primitive per mesh
  //
  inline AssetRepository load(const string& filename) {
    using utils::Range, utils::Enumerate;

    AssetRepository result;
    result.name_ = getBasename(filename);
    result.filename_ = filename;
    std::string dirname = getDirname(filename);

    // temporary map to resolve reference
    std::map<cgltf_texture*, std::shared_ptr<Texture>> ref_map_texture;
    std::map<cgltf_material*, std::shared_ptr<Material>> ref_map_material;
    std::map<cgltf_mesh*, std::shared_ptr<Node>> ref_map_mesh_node;

    // 1. load gltf file
    cgltf_options gparams = {};
    cgltf_data* gdata;
    TOY_ASSERT(cgltf_parse_file(&gparams, filename.data(), &gdata) == cgltf_result_success);
    std::unique_ptr<cgltf_data, decltype(&cgltf_free)> _final_action{gdata, &cgltf_free};
    TOY_ASSERT(gdata->file_type == cgltf_file_type_gltf);
    TOY_ASSERT(cgltf_load_buffers(&gparams, gdata, filename.data()) == cgltf_result_success);

    // 2. load texture (only image name)
    for (auto [_, gtex] : Enumerate{gdata->textures, gdata->textures_count}) {
      TOY_ASSERT(gtex->image->uri);
      auto& texture = result.textures_.emplace_back(new Texture);
      ref_map_texture[gtex] = texture;
      texture->name_ = gtex->image->uri;
      texture->filename_ = dirname + "/" + gtex->image->uri;
    }

    // 3. load material
    for (auto [_, gmat] : Enumerate{gdata->materials, gdata->materials_count}) {
      auto& mat = result.materials_.emplace_back(new Material);
      ref_map_material[gmat] = mat;
      mat->name_ = gmat->name;
      if (gmat->has_pbr_metallic_roughness) {
        auto& pbr = gmat->pbr_metallic_roughness;
        mat->base_color_factor_ = *(fvec4*)(pbr.base_color_factor);
        if (pbr.base_color_texture.texture) {
          mat->base_color_texture_ = ref_map_texture[pbr.base_color_texture.texture];
          TOY_ASSERT(mat->base_color_texture_);
        }
      }
    }

    // 4. load mesh
    for (auto [i, gmesh] : Enumerate{gdata->meshes, gdata->meshes_count}) {
      TOY_ASSERT(gmesh->primitives_count == 1);
      for (auto [_, gprim] : Enumerate{gmesh->primitives, gmesh->primitives_count}) {
        auto& node = result.nodes_.emplace_back(new Node);
        ref_map_mesh_node[gmesh] = node;

        auto& mesh = result.meshes_.emplace_back(new Mesh);
        node->mesh_ = mesh;
        mesh->name_ = gmesh->name;
        mesh->name_ = gmesh->name ? gmesh->name : fmt::format("Mesh ({})", i);
        if (gprim->material) {
          node->material_ = ref_map_material[gprim->material];
          TOY_ASSERT(node->material_);
        }

        // Temporarily load data in a non-interleaved mannger
        Primitive prim;

        // 4.1. load index
        {
          auto accessor = gprim->indices;
          TOY_ASSERT(accessor->component_type == cgltf_component_type_r_16u);
          prim.indices.resize(accessor->count);
          for (auto k : Range{accessor->count}) {
            prim.indices[k] = *(uint16_t*)readAccessor(accessor, k);
          }
        }

        // 4.2. load vertex attributes
        for (auto [_, gattr] : Enumerate{gprim->attributes, gprim->attributes_count}) {
          // reject TEXCOORD_1 etc..
          TOY_ASSERT(gattr->index == 0);

          auto accessor = gattr->data;
          // reject integer-encoded float
          TOY_ASSERT(accessor->component_type == cgltf_component_type_r_32f);

          #define CASE_MACRO(TYPE)                                        \
            case cgltf_attribute_type_##TYPE: {                           \
              prim.TYPE##s.resize(accessor->count);                       \
              for (auto k : Range{accessor->count}) {                     \
                using cast_type = decltype(prim.TYPE##s)::value_type;     \
                prim.TYPE##s[k] = *(cast_type*)readAccessor(accessor, k); \
              }                                                           \
              break;                                                      \
            }
          switch (gattr->type) {
            CASE_MACRO(position)
            CASE_MACRO(normal)
            CASE_MACRO(tangent)
            CASE_MACRO(texcoord)
            CASE_MACRO(color)
            default:;
          }
          #undef CASE_MACRO
        }

        // 4.3. validate vertex data
        auto num = prim.positions.size();
        auto is_zero_or_num = [&](size_t k) { return k == 0 || k == num; };
        TOY_ASSERT(num > 0);
        TOY_ASSERT(is_zero_or_num(prim.normals.size()));
        TOY_ASSERT(is_zero_or_num(prim.tangents.size()));
        TOY_ASSERT(is_zero_or_num(prim.texcoords.size()));
        TOY_ASSERT(is_zero_or_num(prim.colors.size()));
        if (prim.colors.size() == 0) {
          prim.colors = {num, fvec4{1, 1, 1, 1}};
        }

        // 4.4. make our mesh data
        mesh->indices_ = prim.indices;
        mesh->vertices_.resize(num);
        prim.positions.empty();
        for (auto k : utils::Range{num}) {
          #define MACRO(NAME) prim.NAME.empty() ? decltype(prim.NAME)::value_type{} : prim.NAME[k]
          mesh->vertices_[k] = {
            MACRO(positions),
            MACRO(normals),
            MACRO(tangents),
            MACRO(texcoords),
            MACRO(colors),
          };
          #undef MACRO
        }
      }
    }

    // 5. load node
    for (auto [i, gnode] : Enumerate{gdata->nodes, gdata->nodes_count}) {
      if (!gnode->mesh) { continue; }

      auto& node = ref_map_mesh_node[gnode->mesh];
      TOY_ASSERT(node);
      node->name_ = gnode->name ? gnode->name : fmt::format("Node ({})", i);
      cgltf_node_transform_local(gnode, (float*)&node->transform_);
    }

    return result;
  }

} // gltf

} // toy
} // scene
