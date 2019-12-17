#include <gtest/gtest.h>
#include <fmt/format.h>

#include "utils.hpp"

TEST(UtilsTest, Cli1) {
  int argc = 6;
  const char* argv[] = {"some_program", "--force", "-n", "2", "xxx", "yyy"};

  toy::utils::Cli cli{argc, argv};
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

TEST(UtilsTest, interleave) {
  struct VertexData {
    int x;
    float y;
    std::string z;
  };

  std::vector<int> v1 = {0, 1, 2, 3};
  std::vector<float> v2 = {.1, .2, .3, .4};
  std::vector<std::string> v3 = {"p", "q", "r", "s"};

  auto data = toy::utils::interleave<VertexData>(v1, v2, v3);

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
