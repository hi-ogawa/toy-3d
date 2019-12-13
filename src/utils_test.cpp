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
