#include <memory>

#include <fmt/format.h>
#include <imgui.h>
#include <glm/glm.hpp>

#include "window.hpp"
#include "panel_system.hpp"
#include "utils.hpp"


namespace toy {

using namespace utils;
using glm::ivec2, glm::fvec2;

// [@] drawing
//   - [@] draw target (texture, frame buffer)
//   - [@] clear color
// [ ] mesh
//   - [ ] alloc vertex array
//   - [ ] transf
//   - draw program
// [ ] camera
//   - [ ] params..
//   - [ ] transf
// [ ] render model
//   - [ ] simple shader
//   - [ ] uniform, vertex attrib

struct SimpleRenderer {
  struct Camera {
    // todo. parameters, transform
  };
  std::unique_ptr<Camera> camera_;

  struct Mesh {
    TOY_CLASS_DELETE_COPY(Mesh)
    std::vector<float> vertices_;
    std::vector<float> triangles_;
    GLuint array_buffer_, element_array_buffer_, vertex_array_;

    // bind()
    // draw()
    static Mesh create4Hedron() {
      return Mesh{};
    }
  };
  std::unique_ptr<Mesh> mesh_;

  struct Material {};
  std::unique_ptr<Material> material_;

  struct Framebuffer {
    TOY_CLASS_DELETE_COPY(Framebuffer)
    GLuint framebuffer_handle_, texture_handle_;
    ivec2 size_;

    Framebuffer(const ivec2& size) : size_{size} {
      glGenTextures(1, &texture_handle_);
      glBindTexture(GL_TEXTURE_2D, texture_handle_);
      glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
      glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, size_.x, size_.y, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);

      glGenFramebuffers(1, &framebuffer_handle_);
      glBindFramebuffer(GL_DRAW_FRAMEBUFFER, framebuffer_handle_);
      glFramebufferTexture2D(GL_DRAW_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, texture_handle_, 0);
      glDrawBuffer(GL_COLOR_ATTACHMENT0);
    }
    ~Framebuffer() {
      glDeleteTextures(1, &texture_handle_);
      glDeleteFramebuffers(1, &framebuffer_handle_);
    }
  };
  std::unique_ptr<Framebuffer> fb_;

  struct Uniforms {
    GLuint view_inv_xform_;
    GLuint model_xform_;
  } uniforms_;

  GLuint vertex_shader_, fragment_shader_,  program_;

  SimpleRenderer(const ivec2& size) {
    fb_.reset(new Framebuffer{size});
  }

  void draw() {
    glBindFramebuffer(GL_DRAW_FRAMEBUFFER, fb_->framebuffer_handle_);
    glViewport(0, 0, fb_->size_[0], fb_->size_[1]);
    auto c = utils::HSLtoRGB({std::fmod(glfwGetTime() / 6, 1), 1, 0.5, 1});
    glClearColor(c.x, c.y, c.z, c.w);
    glClear(GL_COLOR_BUFFER_BIT);
  }
};


struct RenderPanel : Panel {
  constexpr static const char* type = "Render Panel";
  const SimpleRenderer& renderer_;

  RenderPanel(SimpleRenderer& renderer) : renderer_{renderer} {}

  void processUI() override {
    ImGui::Text("Rendered Image");
    ImGui::Image(
        reinterpret_cast<ImTextureID>(renderer_.fb_->texture_handle_),
        toImVec2(renderer_.fb_->size_));
  }

  void processMenu() override {
  }
};


struct App {
  std::unique_ptr<toy::Window> window_;
  std::unique_ptr<PanelManager> panel_manager_;
  std::unique_ptr<SimpleRenderer> renderer_;
  bool done_ = false;

  App() {
    window_.reset(new Window{"My Window", {800, 600}});
    renderer_.reset(new SimpleRenderer{ivec2{512, 512}});

    panel_manager_.reset(new PanelManager{*window_});
    panel_manager_->registerPanelType(RenderPanel::type, [&]() { return new RenderPanel{*renderer_}; });
    panel_manager_->addPanelToRoot(kdtree::SplitType::HORIZONTAL, RenderPanel::type);
  }

  void processMainMenuBar() {
    if (auto _ = ImScoped::MainMenuBar()) {
      if (auto _ = ImScoped::Menu("Menu")) {
        panel_manager_->processPanelManagerMenuItems();
        if (ImGui::MenuItem("Quit")) {
          done_ = true;
        }
      }
    }
  }

  void processUI() {
    processMainMenuBar();
    panel_manager_->processUI();
  }

  int exec() {
    while(!done_) {
      window_->newFrame();
      panel_manager_->newFrame();
      processUI();
      renderer_->draw();
      panel_manager_->processPostUI();
      window_->render();
      done_ = done_ || window_->shouldClose();
    }
    return 0;
  }
};

} // namespace toy


int main(const int argc, const char* argv[]) {
  toy::App app{};
  return app.exec();
}
