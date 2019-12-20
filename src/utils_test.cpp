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
