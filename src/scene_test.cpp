#include <gtest/gtest.h>
#include <fmt/format.h>

#include "scene.hpp"

using namespace toy;

TEST(SceneTest, gltf_load) {
  auto assets = scene::gltf::load(GLTF_MODEL_PATH("Suzanne"));

  auto& texture = assets.textures_[0];
  EXPECT_EQ(texture->name_, "Suzanne_BaseColor.png");
  EXPECT_EQ(texture->filename_, GLTF_MODEL_DIR "/2.0/Suzanne/glTF/Suzanne_BaseColor.png");

  auto& material = assets.materials_[0];
  EXPECT_EQ(material->base_color_texture_, texture);

  auto& mesh = assets.meshes_[0];
  EXPECT_EQ(mesh->name_, "Suzanne");
  EXPECT_EQ(mesh->vertices_.size(), 11808);
  EXPECT_EQ(mesh->indices_.size(),11808);

  auto& node = assets.nodes_[0];
  EXPECT_EQ(node->material_, material);
  EXPECT_EQ(node->mesh_, mesh);
}

TEST(SceneTest, gltf_load_unsupported) {
  std::map<const char*, const char*> cases{
    {"BrainStem",          "gmesh->primitives_count == 1"                },
    {"SciFiHelmet",        "component_type == cgltf_component_type_r_16u"},
    {"AlphaBlendModeTest", "component_type == cgltf_component_type_r_16u"},
    {"CesiumMan",          "component_type == cgltf_component_type_r_32f"},
  };

  for (auto& [model, assertion] : cases) {
    bool caught = false;
    try {
      scene::gltf::load(getGltfModelPath(model));
    } catch (std::runtime_error e) {
      EXPECT_TRUE(std::string{e.what()}.find(assertion) != std::string::npos);
      caught = true;
    }
    if (!caught) {
      GTEST_FAIL() << "Expected: throws exception";
    }
  }
}
