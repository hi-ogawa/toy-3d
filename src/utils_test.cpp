#include <gtest/gtest.h>
#include <fmt/format.h>

#include "utils.hpp"

using namespace toy;

TEST(UtilsTest, Cli1) {
  int argc = 6;
  const char* argv[] = {"some_program", "--force", "-n", "2", "xxx", "yyy"};

  utils::Cli cli{argc, argv};
  auto names   = cli.getArgs<std::string>(); // vector<string>
  auto n       = cli.getArg<int>("-n");      // optional<int>
  auto force   = cli.checkArg("--force");    // bool
  auto m       = cli.getArg<float>("-m");    // optional<float>
  auto verbose = cli.checkArg("--verbose");  // bool

  EXPECT_EQ(names.size(), 2); EXPECT_EQ(names[0], "xxx"); EXPECT_EQ(names[1], "yyy");
  EXPECT_EQ(n.has_value(), true); EXPECT_EQ(n.value(), 2);
  EXPECT_EQ(force, true);
  EXPECT_EQ(m.has_value(), false);
  EXPECT_EQ(verbose, false);
  EXPECT_EQ(cli.help(), "Usage: <program> --force --verbose -n <arg> -m <arg> <arg-0> <arg-1> ...\n");
}

TEST(UtilsTest, composeTransform) {
  {
    glm::fvec3 s = {1, 1, 1};
    glm::fvec3 t = {0, 0, 0};

    for (auto i : utils::range(90)) {
      glm::fvec3 degs_in = {0, i, 0};
      glm::fmat4 xform = utils::composeTransform(s, glm::radians(degs_in), t);
      auto [_s, _r, _t] = utils::decomposeTransform(xform);
      glm::fvec3 degs_out = glm::degrees(_r);
      EXPECT_LT(glm::distance(degs_in, degs_out), 1e-4) << "NOTE [ i = " << i << " ]";
    }
  }
}

TEST(UtilsTest, decomposeTransform) {
  // identity
  {
    glm::fmat4 xform_id{1};
    auto [s, r, t] = utils::decomposeTransform(xform_id);
    EXPECT_TRUE((s == glm::fvec3{1, 1, 1}));
    EXPECT_TRUE((r == glm::fvec3{0, 0, 0}));
    EXPECT_TRUE((t == glm::fvec3{0, 0, 0}));
  }

  // scale
  {
    glm::fmat4 xform{
      2, 0, 0, 0,
      0, 3, 0, 0,
      0, 0, 5, 0,
      0, 0, 0, 1,
    };
    auto [s, r, t] = utils::decomposeTransform(xform);
    EXPECT_TRUE((s == glm::fvec3{2, 3, 5}));
    EXPECT_TRUE((r == glm::fvec3{0, 0, 0}));
    EXPECT_TRUE((t == glm::fvec3{0, 0, 0}));
  }

  // translation
  {
    glm::fmat4 xform{
      1, 0, 0, 0,
      0, 1, 0, 0,
      0, 0, 1, 0,
      2, 3, 5, 1,
    };
    auto [s, r, t] = utils::decomposeTransform(xform);
    EXPECT_TRUE((s == glm::fvec3{1, 1, 1}));
    EXPECT_TRUE((r == glm::fvec3{0, 0, 0}));
    EXPECT_TRUE((t == glm::fvec3{2, 3, 5}));
  }

  // NOTE: again, don't forget matrix is column measure...
  // rotation z
  {
    glm::fmat4 xform{
      0, 1, 0, 0,
     -1, 0, 0, 0,
      0, 0, 1, 0,
      7,11,13, 1,
    };
    auto [s, r, t] = utils::decomposeTransform(xform);

    EXPECT_FLOAT_EQ(glm::distance(s, {1, 1, 1}), 0);
    EXPECT_FLOAT_EQ(r.x, 0);
    EXPECT_FLOAT_EQ(r.y, 0);
    EXPECT_FLOAT_EQ(r.z, glm::pi<float>() / 2);
    EXPECT_TRUE((t == glm::fvec3{7,11,13}));
  }

  // rotation x
  {
    glm::fmat4 xform{
      1, 0, 0, 0,
      0, 0, 1, 0,
      0,-1, 0, 0,
      7,11,13, 1,
    };
    auto [s, r, t] = utils::decomposeTransform(xform);

    EXPECT_FLOAT_EQ(glm::distance(s, {1, 1, 1}), 0);
    EXPECT_FLOAT_EQ(r.x, glm::pi<float>() / 2);
    EXPECT_FLOAT_EQ(r.y, 0);
    EXPECT_FLOAT_EQ(r.z, 0);
    EXPECT_TRUE((t == glm::fvec3{7,11,13}));
  }

  // rotation y
  {
    glm::fmat4 xform{
      0, 0,-1, 0,
      0, 1, 0, 0,
      1, 0, 0, 0,
      7,11,13, 1,
    };
    auto [s, r, t] = utils::decomposeTransform(xform);

    EXPECT_FLOAT_EQ(glm::distance(s, {1, 1, 1}), 0);
    EXPECT_FLOAT_EQ(r.x, 0);
    EXPECT_FLOAT_EQ(r.y, glm::pi<float>() / 2);
    EXPECT_FLOAT_EQ(r.z, 0);
    EXPECT_TRUE((t == glm::fvec3{7,11,13}));
  }
}


TEST(UtilsTest, GltfData) {
  auto data = utils::GltfData::load(GLTF_MODEL_PATH("Suzanne"));

  auto texture = data.textures[0];
  EXPECT_EQ(texture.name, "Suzanne_BaseColor.png");
  EXPECT_EQ(texture.filename, GLTF_MODEL_DIR "/2.0/Suzanne/glTF/Suzanne_BaseColor.png");

  auto mesh = data.meshes[0];
  EXPECT_EQ(mesh.name, "Suzanne (1)");
  EXPECT_EQ(mesh.vertices.size(), 11808);
  EXPECT_EQ(mesh.indices.size(),11808);
}


TEST(UtilsTest, range) {
  std::string result = "\n";
  std::string expected = R"(
1:
  - 0
  - 1
2:
  - 0
  - 1
  - 2
3:
  - 0
  - 1
  - 2
  - 3
)";

  for (auto i : utils::range(1, 4)) {
    result += fmt::format("{}:\n", i);
    for (auto j : utils::range(i + 1)) {
      result += fmt::format("  - {}\n", j);
    }
  }

  EXPECT_EQ(result, expected);
}

TEST(UtilsTest, enumerate) {
  std::string argv1[] =            {"some_program", "--force", "-n", "2", "xxx", "yyy"};
  std::vector<std::string> argv2 = {"some_program", "--force", "-n", "2", "xxx", "yyy"};

  std::string result1 = "\n";
  std::string result2 = "\n";
  std::string expected = R"(
0: some_program
1: --force
2: -n
3: 2
4: xxx
5: yyy
)";
  for (auto [i, x_ptr] : utils::enumerate(argv1, 6)) {
    result1 += fmt::format("{}: {}\n", i, *x_ptr);
  }
  for (auto [i, x_ptr] : utils::enumerate(argv2)) {
    result2 += fmt::format("{}: {}\n", i, *x_ptr);
  }
  EXPECT_EQ(result1, expected);
  EXPECT_EQ(result2, expected);
}


TEST(UtilsTest, interleave) {
  struct VertexData {
    int x;
    float y;
    std::string z;
  };

  std::vector<int> v1 = {0, 1, 2, 3};
  std::vector<float> v2 = {.1, .2, .3, .4};
  std::vector<std::string> v3 = {"p", "q", "r", "s"};

  auto data = utils::interleave<VertexData>(v1, v2, v3);

  std::string result = "\n";
  std::string expected = R"(
0 - 0.1 - p
1 - 0.2 - q
2 - 0.3 - r
3 - 0.4 - s
)";

  for (auto& e : data) {
    result += fmt::format("{} - {} - {}\n", e.x, e.y, e.z);
  }

  EXPECT_EQ(result, expected);
}
