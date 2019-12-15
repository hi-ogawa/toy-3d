#include <iostream>

#include <fmt/format.h>
#include <imgui.h>
#include <glm/glm.hpp>

#include "window.hpp"
#include "utils.hpp"


namespace toy {

using glm::ivec2;
using utils::toImVec2;

struct Framebuffer {
  TOY_CLASS_DELETE_COPY(Framebuffer)
  GLuint handle_, texture_;
  ivec2 size_ = {512, 512};

  Framebuffer() {
    glGenTextures(1, &texture_);
    glBindTexture(GL_TEXTURE_2D, texture_);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST); // default is GL_NEAREST_MIPMAP_LINEAR
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, size_.x, size_.y, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);

    glGenFramebuffers(1, &handle_);
    glBindFramebuffer(GL_DRAW_FRAMEBUFFER, handle_);
    glFramebufferTexture2D(GL_DRAW_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, texture_, 0);
    glDrawBuffer(GL_COLOR_ATTACHMENT0);
  }

  ~Framebuffer() {
    glDeleteTextures(1, &texture_);
    glDeleteFramebuffers(1, &handle_);
  }

  void draw() {
    glBindFramebuffer(GL_DRAW_FRAMEBUFFER, handle_);
    glViewport(0, 0, size_.x, size_.y);
    auto c = utils::HSLtoRGB({std::fmod(glfwGetTime() / 6, 1), 1, 0.5, 1});
    glClearColor(c.x, c.y, c.z, c.w);
    glClear(GL_COLOR_BUFFER_BIT);
  }
};

struct App {
  bool done_ = false;
  std::unique_ptr<Window> window_;
  std::unique_ptr<Framebuffer> framebuffer_;

  App() {
    window_.reset(new Window{"My Window", {800, 600}, { .gl_debug = true }});
    framebuffer_.reset(new Framebuffer{});
  }
  ~App() {
  }

  void processUI() {
    ImGui::SetNextWindowSize({400, 400}, ImGuiCond_FirstUseEver);
    ImGui::Begin("Framebuffer color attachment", nullptr);
    ImGui::Image(reinterpret_cast<ImTextureID>(framebuffer_->texture_), toImVec2(framebuffer_->size_));
    ImGui::End();
  }

  int exec() {
    while(!done_) {
      window_->newFrame();
      processUI();
      framebuffer_->draw();
      window_->render();
      done_ = done_ || window_->shouldClose();
    }
    return 0;
  };
};

} // namespace toy


int main(const int argc, const char* argv[]) {
  toy::App app{};
  return app.exec();
}
