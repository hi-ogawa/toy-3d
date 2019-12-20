#include <memory>
#include <stdexcept>

#include <cgltf.h>
#include <fmt/format.h>

#include "utils.hpp"

namespace toy {

struct App {
  std::unique_ptr<cgltf_data, decltype(&cgltf_free)> data_ = {nullptr, &cgltf_free};

  App(const std::string& filename) {
    cgltf_options params = {};
    cgltf_data* data;
    if (cgltf_parse_file(&params, filename.data(), &data) != cgltf_result_success) {
      throw std::runtime_error{fmt::format("cgltf_parse_file failed: {}", filename)};
    }
    data_.reset(data);

    if (cgltf_load_buffers(&params, data_.get(), filename.data()) != cgltf_result_success) {
      throw std::runtime_error{fmt::format("cgltf_load_buffers failed: {}", filename)};
    }
  }
};

} // namespace toy


int main(const int argc, const char* argv[]) {
  toy::utils::Cli cli{argc, argv};
  auto filename = cli.getArg<std::string>("--gltf");
  if (!filename) {
    fmt::print(cli.help());
    return 1;
  }

  toy::App app{*filename};
  return 0;
}
