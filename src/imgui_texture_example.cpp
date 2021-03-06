#include <fmt/format.h>
#include <imgui.h>
#include <stb_image.h>

#include "window.hpp"
#include "utils.hpp"


namespace toy {

using glm::ivec2, glm::fvec2;
using utils::fromImVec2, utils::toImVec2;

struct Texture {
  TOY_CLASS_DELETE_COPY(Texture)
  GLuint handle_;

  Texture() {
    glGenTextures(1, &handle_);
  }
  ~Texture() {
    glDeleteTextures(1, &handle_);
  }
};

struct ImageTexture : Texture {
  uint8_t* pixels_;
  ivec2 size_;
  int original_color_component_;
  std::string name_;

  ImageTexture(const std::string& filename, bool is_srgb) : Texture(), name_{filename} {
    pixels_ = stbi_load(filename.data(), &size_.x, &size_.y, &original_color_component_, 4);
    if (!pixels_) fmt::print("failed to load a file: {}\n", filename);
    assert(pixels_);
    glBindTexture(GL_TEXTURE_2D, handle_);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST); // default is GL_NEAREST_MIPMAP_LINEAR
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, size_.x, size_.y, 0, GL_RGBA, GL_UNSIGNED_BYTE, pixels_);
  }

  ~ImageTexture() {
    stbi_image_free(pixels_);
  }

  //
  // Shared data for all textures to draw quad
  //
  constexpr static inline float array_buffer_data_[] = {
    0, 0,
    1, 0,
    1, 1,
    0, 1,
  };
  constexpr static inline uint8_t element_array_buffer_data_[] = {
    0, 1, 3,
    2, 3, 1,
  };
  constexpr static inline const char* vertex_shader_source = R"(
#version 330
uniform mat4 projection_;
layout (location = 0) in vec2 position_;
layout (location = 1) in vec2 uv_;
out vec2 frag_uv_;
void main() {
  frag_uv_ = vec2(uv_.x, uv_.y);
  gl_Position = projection_ * vec4(position_.xy, 0, 1);
}
)";
  constexpr static inline const char* fragment_shader_source = R"(
#version 330
uniform sampler2D texture_;
in vec2 frag_uv_;
layout (location = 0) out vec4 out_color_;
void main() {
  out_color_ = texture(texture_, frag_uv_);
}
)";
  static inline GLuint
      array_buffer_, element_array_buffer_, vertex_array_,
      uniform_location_projection_, uniform_location_texture_;

  static inline std::unique_ptr<utils::gl::Program> program_;

  static void createGLObjects() {
    glGenBuffers(1, &array_buffer_);
    glGenBuffers(1, &element_array_buffer_);
    glGenVertexArrays(1, &vertex_array_);
    program_.reset(new utils::gl::Program{vertex_shader_source, fragment_shader_source});
    uniform_location_projection_ = glGetUniformLocation(program_->handle_, "projection_");
    uniform_location_texture_ = glGetUniformLocation(program_->handle_, "texture_");

    // Configure vertex format once and for all
    glBindVertexArray(vertex_array_);
    glBindBuffer(GL_ARRAY_BUFFER, array_buffer_);
    auto position_index_ = glGetAttribLocation(program_->handle_, "position_");
    auto uv_index_ = glGetAttribLocation(program_->handle_, "uv_");
    glEnableVertexAttribArray(position_index_);
    glEnableVertexAttribArray(uv_index_);
    glVertexAttribPointer(position_index_, 2, GL_FLOAT, GL_FALSE, (sizeof array_buffer_data_[0]) * 2, 0);
    glVertexAttribPointer(uv_index_,       2, GL_FLOAT, GL_FALSE, (sizeof array_buffer_data_[0]) * 2, 0);

    // Initialize data once and for all
    glBindBuffer(GL_ARRAY_BUFFER, array_buffer_);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, element_array_buffer_);
    glBufferData(GL_ARRAY_BUFFER, (sizeof array_buffer_data_), array_buffer_data_, GL_STREAM_DRAW);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, (sizeof element_array_buffer_data_), element_array_buffer_data_, GL_STREAM_DRAW);
  }

  static void destroyGLObjects() {
    glDeleteBuffers(1, &array_buffer_);
    glDeleteBuffers(1, &element_array_buffer_);
    glDeleteVertexArrays(1, &vertex_array_);
    program_.reset(nullptr);
  }

  void draw(ivec2 viewport_size, ivec2 offset, ivec2 size) {
    fvec2 vp{viewport_size};
    glm::fmat4x4 viewport_projection = {
      2/vp.x,       0,  0, 0,
           0, -2/vp.y,  0, 0,
           0,       0, -1, 0,
          -1,       1,  0, 1,
    };
    glm::fmat4x4 quad_transform = {
        size.x,        0, 0, 0,
             0,   size.y, 0, 0,
             0,        0, 1, 0,
      offset.x, offset.y, 0, 1,
    };
    glm::fmat4x4 projection  = viewport_projection * quad_transform;

    glViewport(0, 0, vp.x, vp.y);
    glEnable(GL_BLEND);
    glBlendEquation(GL_FUNC_ADD);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glDisable(GL_CULL_FACE);
    glDisable(GL_DEPTH_TEST);
    glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);

    glUseProgram(program_->handle_);

    ////
    // Most confusing part of texture setup
    // (Cf. "Texture image units" https://www.khronos.org/opengl/wiki/Texture)
    glUniform1i(uniform_location_texture_, 0); // 1. Tell "sampler" to use "0th image unit"
    glActiveTexture(GL_TEXTURE0);              // 2. Activate "0th image unit" for "texture" to bind to
    glBindTexture(GL_TEXTURE_2D, handle_);     //    by this `glBindTexture` call.
    ////

    glUniformMatrix4fv(uniform_location_projection_, 1, GL_FALSE, (GLfloat*)&projection);

    glBindVertexArray(vertex_array_);
    glBindBuffer(GL_ARRAY_BUFFER, array_buffer_);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, element_array_buffer_);
    glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_BYTE, 0);
  }
};

struct App {
  bool done_ = false;
  std::unique_ptr<Window> window_;
  std::vector<std::unique_ptr<ImageTexture>> textures_;
  int seleted_texture_index_ = -1;

  using Command = std::function<void()>;
  std::vector<Command> command_queue_;

  App(const std::vector<std::string>& filenames) {
    window_.reset(new Window{"My Window", {800, 600}, { .gl_debug = true }});
    window_->drop_callback_ = [&](const std::vector<std::string>& paths) {
      command_queue_.emplace_back([this, paths]() {
        for (auto& path : paths) {
          textures_.emplace_back(new ImageTexture{path, true});
        }
      });
    };

    for (auto& filename : filenames) {
      textures_.emplace_back(new ImageTexture{filename, true});
    }
    if (!filenames.empty()) {
      seleted_texture_index_ = 0;
    }

    ImageTexture::createGLObjects();
  }
  ~App() {
    ImageTexture::destroyGLObjects();
  }

  void processCommands() {
    for (auto& command : command_queue_) command();
    command_queue_ = {};
  }

  void processUI() {
    ImGui::SetNextWindowSize({300, 300}, ImGuiCond_FirstUseEver);
    ImGui::Begin("Texture Viewer", nullptr);
    static auto combo_text_getter = [](void* data, int index, const char* out[]) {
      *out = reinterpret_cast<App*>(data)->textures_[index]->name_.data();
      return true;
    };
    ImGui::Combo("Image file", &seleted_texture_index_, combo_text_getter, this, textures_.size());
    auto index = seleted_texture_index_;
    if (0 <= index && index < textures_.size()) {
      auto& tex = textures_[index];
      ImGui::Text("Texture size = (%d, %d)", tex->size_[0], tex->size_[1]);
      ImGui::Image(reinterpret_cast<ImTextureID>(tex->handle_), toImVec2(tex->size_));

      // Draw by ourselves
      tex->draw(fromImVec2<int>(window_->io_->DisplaySize), {0, 0}, fromImVec2<int>(window_->io_->DisplaySize));
    }
    ImGui::End();
  }

  int exec() {
    while(!done_) {
      window_->newFrame();
      processUI();
      window_->render();
      processCommands();
      done_ = done_ || window_->shouldClose();
    }
    return 0;
  };
};

} // namespace toy


int main(const int argc, const char* argv[]) {
  toy::utils::Cli cli{argc, argv};
  auto filenames = cli.getArgs<std::string>();
  toy::App app{filenames};
  return app.exec();
}
