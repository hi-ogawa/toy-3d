constexpr static const char* vertex_shader_source = R"(
#version 330
uniform mat4 view_projection_;
uniform mat4 view_inv_xform_;
uniform mat4 model_xform_;

layout (location = 0) in vec3 vert_position_;
layout (location = 1) in vec4 vert_color_;
layout (location = 2) in vec2 vert_texcoord_;

out vec4 interp_color_;
out vec2 interp_texcoord_;

void main() {
  interp_color_ = vert_color_;
  interp_texcoord_ = vert_texcoord_;
  gl_Position = view_projection_ * view_inv_xform_ * model_xform_ * vec4(vert_position_, 1);
}
)";

constexpr static const char* fragment_shader_source = R"(
#version 330
uniform sampler2D base_color_texture_;
uniform bool use_base_color_texture_;
uniform vec4 base_color_factor_;

in vec4 interp_color_;
in vec2 interp_texcoord_;

layout (location = 0) out vec4 frag_color_;

void main() {
  vec4 uniform_base_color = (use_base_color_texture_) ? texture(base_color_texture_, interp_texcoord_) : base_color_factor_;
  vec4 base_color = interp_color_ * uniform_base_color;
  frag_color_ = base_color;
}
)";
