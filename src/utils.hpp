#pragma once

// Cli
#include <cstring>
#include <stdexcept>
#include <string>
#include <vector>
#include <optional>
#include <sstream>
#include <fmt/format.h>

// toImVec2, fromImVec2
#include <imgui.h>
#include <glm/glm.hpp>

// checkGLShader, checkGLProgram
#include <GL/gl3w.h>


//
// Delete move/copy constructure/assignment
//
#define TOY_CLASS_DELETE_MOVE(CLASS)  \
  CLASS(CLASS&&) = delete;            \
  CLASS &operator=(CLASS&&) = delete;

#define TOY_CLASS_DELETE_COPY(CLASS)      \
  CLASS(const CLASS&) = delete;           \
  CLASS& operator=(const CLASS&) = delete;

#define TOY_CLASS_DELETE_MOVE_COPY(CLASS) \
  TOY_CLASS_DELETE_MOVE(CLASS)            \
  TOY_CLASS_DELETE_COPY(CLASS)


//
// format glm::fmat4x4
//
std::ostream& operator<<(std::ostream& os, const glm::fmat4& A) {
  auto _A = reinterpret_cast<const float*>(&A[0][0]);
  os << "{ ";
  for (auto i = 0; i < 16; i++) {
    if (i == 15) {
      os << _A[i];
    } else if (i % 4 == 3) {
      os << _A[i] << ",\n  ";
    } else {
      os << _A[i] << ", ";
    }
  }
  os << " }";
  return os;
}


namespace toy { namespace utils {

using namespace glm;
using std::vector, std::string;

//
// inverse of group SO(3) x R^3 (aka "transform")
//

fmat4 inverse(const fmat4& F) {
  fmat3 A{F};
  fvec3 b{F[3]};
  auto AT = transpose(A); // i.e. inverse
  auto c = - AT * b;
  fmat4 G{AT};
  G[3] = fvec4{c, 1};
  return G;
}

fmat3 ExtrinsicEulerXYZ_to_SO3(fvec3 degrees_xyz) {
  auto x = degrees_xyz[0] * 3.14 / 180., cx = std::cos(x), sx = std::sin(x);
  auto y = degrees_xyz[1] * 3.14 / 180., cy = std::cos(y), sy = std::sin(y);
  auto z = degrees_xyz[2] * 3.14 / 180., cz = std::cos(z), sz = std::sin(z);
  auto Rx = glm::fmat3{
      1,   0,   0,
      0,  cx,  sx,
      0, -sx,  cx,
  };
  auto Ry = glm::fmat3{
      cy,   0, -sy,
      0,   1,   0,
      sy,   0,  cy,
  };
  auto Rz = glm::fmat3{
      cz,  sz,   0,
    -sz,  cz,   0,
      0,   0,   1,
  };
  return Rz * Ry * Rx;
}

//
// Mesh examples
//

template<typename TOut, typename T, typename... Ts>
static vector<TOut> interleave(const vector<T>& v, const vector<Ts>&... vs) {
  vector<TOut> result;
  result.resize(v.size());
  for (auto i = 0; i < v.size(); i++) {
    result[i] = {v[i], vs[i]...};
  }
  return result;
}

template<typename T>
vector<T> Quads_to_Triangles(const vector<T>& quad_indices) {
  auto n = quad_indices.size();
  if (n % 4 != 0) {
    throw std::runtime_error{"Invalid argument: quad_indices.size() % 4 != 0"};
  }
  vector<T> result;
  result.resize(n / 4 * 6);
  for (auto i = 0; i < n / 4; i++) {
    auto A = quad_indices[4 * i + 0];
    auto B = quad_indices[4 * i + 1];
    auto C = quad_indices[4 * i + 2];
    auto D = quad_indices[4 * i + 3];
    result[6 * i + 0] = A;
    result[6 * i + 1] = B;
    result[6 * i + 2] = D;
    result[6 * i + 3] = C;
    result[6 * i + 4] = D;
    result[6 * i + 5] = B;
  }
  return result;
};

auto createCube() {
  std::tuple<vector<fvec3>, vector<fvec4>, vector<uint8_t>> result;
  auto& [positions, colors, indices] = result;
  positions = {
    { 0, 0, 0 },
    { 1, 0, 0 },
    { 1, 1, 0 },
    { 0, 1, 0 },
    { 0, 0, 1 },
    { 1, 0, 1 },
    { 1, 1, 1 },
    { 0, 1, 1 },
  };
  colors = {
    { 0, 0, 0, 1 },
    { 1, 0, 0, 1 },
    { 1, 1, 0, 1 },
    { 0, 1, 0, 1 },
    { 0, 0, 1, 1 },
    { 1, 0, 1, 1 },
    { 1, 1, 1, 1 },
    { 0, 1, 1, 1 },
  };
  indices = Quads_to_Triangles(vector<uint8_t>{
    0, 3, 2, 1, // z = 0 plane
    4, 5, 6, 7, // z = 1
    0, 1, 5, 4, // y = 0
    1, 2, 6, 5, // x = 1
    2, 3, 7, 6, // y = 1
    3, 0, 4, 7, // x = 0
  });
  return result;
}

auto create4Hedron() {
  std::tuple<vector<fvec3>, vector<fvec4>, vector<uint8_t>> result;
  auto& [positions, colors, indices] = result;
  positions = {
    { 0, 0, 0 },
    { 1, 1, 0 },
    { 0, 1, 1 },
    { 1, 0, 1 },
  };
  colors = {
    { 0, 0, 0, 1 },
    { 1, 0, 0, 1 },
    { 0, 1, 0, 1 },
    { 0, 0, 1, 1 },
  };
  indices = {
    0, 2, 1,
    0, 3, 2,
    0, 1, 3,
    1, 2, 3,
  };
  return result;
}

auto createPlane() {
  std::tuple<vector<fvec3>, vector<fvec4>, vector<uint8_t>> result;
  auto& [positions, colors, indices] = result;
  positions = {
    { 0, 0, 0 },
    { 1, 0, 0 },
    { 1, 1, 0 },
    { 0, 1, 0 },
  };
  colors = {
    { 1, 1, 1, 1 },
    { 1, 0, 0, 1 },
    { 0, 1, 0, 1 },
    { 0, 0, 1, 1 },
  };
  indices = Quads_to_Triangles(vector<uint8_t>{
    0, 1, 2, 3,
    4, 5, 6, 7,
  });
  return result;
}


//
// hsl <--> rgb \in [0, 1]^3
//

glm::fvec4 inline RGBtoHSL(glm::fvec4 c) {
  int sort[3];
  if (c.x >= c.y) {
    if (c.y >= c.z) {
      sort[0] = 0; sort[1] = 1; sort[2] = 2;
    } else {
      sort[2] = 1;
      if (c.x >= c.z) { sort[0] = 0; sort[1] = 2; }
      else            { sort[0] = 2; sort[1] = 0; }
    }
  } else {
    if (c.y >= c.z) {
      sort[0] = 1;
      if (c.x >= c.z) { sort[1] = 0; sort[2] = 2; }
      else            { sort[1] = 2; sort[2] = 0; }
    } else {
      sort[0] = 2; sort[1] = 1; sort[2] = 0;
    }
  }
  float h_base = 2 * sort[0];
  float h_dir = (sort[1] - sort[0]) % 3 == 1 ? 1 : -1;
  float dh = c[sort[1]] - c[sort[2]];
  auto h = (h_base + h_dir * dh) / 6;
  auto s = c[sort[0]] - c[sort[2]];
  auto l = (c.x + c.y + c.z) / 3;
  return glm::fvec4{ h, s, l, c.w };
};

//
// NOTE:
// Linear transf. (invertible)
// - c[sort[0]], c[sort[1]], c[sort[2]] |-> s, dh, l
// [   1    0   -1 ]
// [   0    1   -1 ]
// [ 1/3  1/3  1/3 ]
// and its inverse
// [ 2/3 -1/2  1 ]
// [-1/3  2/3  1 ]
// [ 1/3  1/3  1 ]
//
glm::fvec4 inline HSLtoRGB(glm::fvec4 d) {
  float h6 = d.x * 6, s = d.y, l = d.z;
  float dh;
  float csort[3];
  int inv[3];
  if (h6 <= 1) {
    inv[0] = 0; inv[1] = 1; inv[2] = 2; dh = h6;
  } else if (h6 <= 2) {
    inv[0] = 1; inv[1] = 0; inv[2] = 2; dh = 2 - h6;
  } else if (h6 <= 3) {
    inv[0] = 2; inv[1] = 0; inv[2] = 1; dh = h6 - 2;
  } else if (h6 <= 4) {
    inv[0] = 2; inv[1] = 1; inv[2] = 0; dh = 4 - h6;
  } else if (h6 <= 5) {
    inv[0] = 1; inv[1] = 2; inv[2] = 0; dh = h6 - 4;
  } else {
    inv[0] = 0; inv[1] = 2; inv[2] = 1; dh = 6 - h6;
  }
  csort[0] = + 2./3. * s - 1./3. * dh + l;
  csort[1] = - 1./3. * s + 2./3. * dh + l;
  csort[2] = - 1./3. * s - 1./3. * dh + l;
  return glm::fvec4{ csort[inv[0]], csort[inv[1]], csort[inv[2]], d.w };
};


//
// glm::ivec2, fvec2  <-->  ImVec2
//

template<typename T>
ImVec2 inline toImVec2(glm::vec<2, T> v) {
  return ImVec2{static_cast<float>(v[0]), static_cast<float>(v[1])};
};

template<typename T>
glm::vec<2, T> inline fromImVec2(ImVec2 v) {
  return glm::vec<2, T>{static_cast<T>(v[0]), static_cast<T>(v[1])};
};

//
// checkGLShader, checkGLProgram
//

namespace gl {
  void enableDebugMessage() {
    static auto callback = [](
        GLenum source, GLenum type, GLuint id,
        GLenum severity, GLsizei length,
        const GLchar* message, void* userParam) {
      fprintf(
          stderr, "OpenGL Debug Message: %s type = 0x%x, severity = 0x%x, message = %s\n",
          type == GL_DEBUG_TYPE_ERROR ? "[ERROR]" : "",
          type, severity, message);
    };
    glEnable(GL_DEBUG_OUTPUT);
    glDebugMessageCallback(callback, 0);
  }

  std::pair<bool, std::string> checkShader(GLuint handle) {
    std::string log;
    GLint status = 0, log_length = 0;
    glGetShaderiv(handle, GL_COMPILE_STATUS, &status);
    glGetShaderiv(handle, GL_INFO_LOG_LENGTH, &log_length);
    if (log_length > 0) {
      log.resize(log_length);
      glGetShaderInfoLog(handle, log_length, NULL, (GLchar*)log.data());
    }
    return make_pair(status == GL_TRUE, log);
  }

  std::pair<bool, std::string> checkProgram(GLuint handle) {
    std::string log;
    GLint status = 0, log_length = 0;
    glGetProgramiv(handle, GL_LINK_STATUS, &status);
    glGetProgramiv(handle, GL_INFO_LOG_LENGTH, &log_length);
    if (log_length > 0) {
      log.resize(log_length);
      glGetProgramInfoLog(handle, log_length, NULL, (GLchar*)log.data());
    }
    return make_pair(status == GL_TRUE, log);
  }

  struct Program {
    GLuint handle_, vertex_shader_, fragment_shader_;
    Program(const char* vs_src, const char* fs_src) {
      vertex_shader_ = glCreateShader(GL_VERTEX_SHADER);
      fragment_shader_ = glCreateShader(GL_FRAGMENT_SHADER);
      handle_ = glCreateProgram();

      glShaderSource(vertex_shader_, 1, &vs_src, nullptr);
      glCompileShader(vertex_shader_);
      if (auto result = checkShader(vertex_shader_); !result.first) {
        throw std::runtime_error{"== glCompileShader(vertex_shader_) faild. ==\n" + result.second};
      }

      glShaderSource(fragment_shader_, 1, &fs_src, nullptr);
      glCompileShader(fragment_shader_);
      if (auto result = checkShader(fragment_shader_); !result.first) {
        throw std::runtime_error{"== glCompileShader(fragment_shader_) faild. ==\n" + result.second};
      }

      glAttachShader(handle_, vertex_shader_);
      glAttachShader(handle_, fragment_shader_);
      glLinkProgram(handle_);
      if (auto result = checkProgram(handle_); !result.first) {
        throw std::runtime_error{"== glLinkProgram(handle_) faild. ==\n" + result.second};
      }
    }
    ~Program() {
      glDetachShader(handle_, vertex_shader_);
      glDetachShader(handle_, fragment_shader_);
      glDeleteShader(vertex_shader_);
      glDeleteShader(fragment_shader_);
      glDeleteProgram(handle_);
    }

    void setUniform(const char* name, const glm::fmat4& value) {
      auto location = glGetUniformLocation(handle_, name);
      if (location == -1)
        throw std::runtime_error{fmt::format("== Uniform ({}) not found ==", name)};
      glUniformMatrix4fv(location, 1, GL_FALSE, (GLfloat*)&value);
    }

    void setUniform(const char* name, GLint value) {
      auto location = glGetUniformLocation(handle_, name);
      if (location == -1)
        throw std::runtime_error{fmt::format("== Uniform ({}) not found ==", name)};
      glUniform1i(location, value);
    }
  };

  struct Framebuffer {
    TOY_CLASS_DELETE_COPY(Framebuffer)
    GLuint framebuffer_handle_, texture_handle_, depth_texture_handle_;
    glm::ivec2 size_ = {1, 1};

    Framebuffer() {
      glGenFramebuffers(1, &framebuffer_handle_);
      glBindFramebuffer(GL_DRAW_FRAMEBUFFER, framebuffer_handle_);

      // color attachment
      glGenTextures(1, &texture_handle_);
      glBindTexture(GL_TEXTURE_2D, texture_handle_);
      glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
      glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, size_.x, size_.y, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
      glFramebufferTexture2D(GL_DRAW_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, texture_handle_, 0);
      glDrawBuffer(GL_COLOR_ATTACHMENT0);

      // depth attachment
      glGenTextures(1, &depth_texture_handle_);
      glBindTexture(GL_TEXTURE_2D, depth_texture_handle_);
      glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
      glTexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH_COMPONENT32F, size_.x, size_.y, 0, GL_DEPTH_COMPONENT, GL_FLOAT, nullptr);
      glFramebufferTexture2D(GL_DRAW_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, depth_texture_handle_, 0);
    }

    ~Framebuffer() {
      glDeleteTextures(1, &depth_texture_handle_);
      glDeleteTextures(1, &texture_handle_);
      glDeleteFramebuffers(1, &framebuffer_handle_);
    }

    void setSize(const glm::ivec2& size) {
      if (size[0] == 0 || size[1] == 0) {
        throw std::runtime_error{"Invalid argument: size[0] == 0 || size[1] == 0"};
      }
      size_ = size;
      glBindTexture(GL_TEXTURE_2D, texture_handle_);
      glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, size_.x, size_.y, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
      glBindTexture(GL_TEXTURE_2D, depth_texture_handle_);
      glTexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH_COMPONENT32F, size_.x, size_.y, 0, GL_DEPTH_COMPONENT, GL_FLOAT, nullptr);
    }
  };

  // Usable only for interleaved vertex buffer
  struct VertexRenderer {
    TOY_CLASS_DELETE_COPY(VertexRenderer)
    GLuint vertex_array_, array_buffer_, element_array_buffer_;
    GLenum primitive_mode_ = GL_TRIANGLES;
    GLenum index_type_;
    GLsizei num_indices_;

    VertexRenderer() {
      glGenBuffers(1, &array_buffer_);
      glGenBuffers(1, &element_array_buffer_);
      glGenVertexArrays(1, &vertex_array_);
    }

    ~VertexRenderer() {
      glDeleteBuffers(1, &array_buffer_);
      glDeleteBuffers(1, &element_array_buffer_);
      glDeleteVertexArrays(1, &vertex_array_);
    }

    template<typename T> void setIndexType();
    template<> void setIndexType< uint8_t>() { index_type_ = GL_UNSIGNED_BYTE;  };
    template<> void setIndexType<uint16_t>() { index_type_ = GL_UNSIGNED_SHORT; };
    template<> void setIndexType<uint32_t>() { index_type_ = GL_UNSIGNED_INT;   };

    template<typename T1, typename T2>
    void setData(const std::vector<T1>& vertices, const std::vector<T2>& indices) {
      glBindBuffer(GL_ARRAY_BUFFER, array_buffer_);
      glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, element_array_buffer_);
      glBufferData(GL_ARRAY_BUFFER, vertices.size() * sizeof(T1), vertices.data(), GL_STREAM_DRAW);
      glBufferData(GL_ELEMENT_ARRAY_BUFFER, indices.size() * sizeof(T2), indices.data(), GL_STREAM_DRAW);
      num_indices_ = indices.size();
      setIndexType<T2>();
    }

    void setFormat(
        GLint location, GLint size, GLenum type,
        GLboolean normalized, GLsizei stride, const void* pointer) {
      glBindVertexArray(vertex_array_);
      glBindBuffer(GL_ARRAY_BUFFER, array_buffer_);
      glEnableVertexAttribArray(location);
      glVertexAttribPointer(location, size, type, normalized, stride, pointer);
    }

    void draw() {
      glBindVertexArray(vertex_array_);
      glBindBuffer(GL_ARRAY_BUFFER, array_buffer_);
      glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, element_array_buffer_);
      glDrawElements(primitive_mode_, num_indices_, index_type_, 0);
    }
  };
}


//
// Usage:
// for (auto i : range(10)) { ... }
//
// cf.
// - https://en.cppreference.com/w/cpp/language/range-for
// - https://en.cppreference.com/w/cpp/named_req/Iterator
// - https://github.com/xelatihy/yocto-gl/blob/master/yocto/yocto_common.h
//

struct RangeHelper {
  int start_, end_;

  struct Iterator {
    int i_;

    int operator*() { return i_; }
    Iterator& operator++() {
      i_++;
      return *this;
    }
    bool operator!=(const Iterator& other) {
      return i_ != other.i_;
    }
  };

  Iterator begin() { return Iterator{start_}; };
  Iterator end() { return Iterator{end_}; };
};

RangeHelper inline range(int stop) {
  return RangeHelper{0, stop};
}

RangeHelper inline range(int start, int stop) {
  return RangeHelper{start, stop};
}


//
// Example:
//
// Cli cli{argc, argv};
// auto names = cli.getArgs<string>();   // vector<string>
// auto n     = cli.getArg<int>("-n");   // optional<int>
// auto force = cli.checkArg("--force"); // bool
// if (names.empty())
//   cout << cli.help();
//

struct Cli {
  const int argc_; const char** argv_;
  bool has_positional_arg_ = false;
  std::vector<std::string> value_flags_;
  std::vector<std::string> boolean_flags_;

  std::string help() {
    auto join = [](const std::vector<std::string>& v) {
      std::string result;
      for (auto i : range(v.size())) {
        result += " " + v[i];
      }
      return result;
    };
    return fmt::format(
        "Usage: <program>{}{}{}\n",
        join(boolean_flags_), join(value_flags_),
        has_positional_arg_ ? " <arg-0> <arg-1> ..." : "");
  }

  template<typename T>
  T _convert(const char* s) {
    std::istringstream stream{s};
    T result;
    stream >> result;
    return result;
  }

  template<>
  std::string _convert(const char* s) {
    return std::string{s};
  }

  template<typename T>
  std::optional<T> getArg(const char* flag) {
    value_flags_.push_back(std::string(flag) + " <arg>");
    for (auto i = 1; i < argc_; i++) {
      if (strcmp(argv_[i], flag) == 0 && i + 1 < argc_) {
        return _convert<T>(argv_[i + 1]);
      }
    }
    return {};
  }

  template<typename T>
  std::vector<T> getArgs() {
    has_positional_arg_ = true;
    std::vector<T> results;
    for (auto i = 1; i < argc_; i++) {
      if (argv_[i][0] == '-') {
        i += 2;
        continue;
      }
      results.push_back(_convert<T>(argv_[i]));
    }
    return results;
  }

  bool checkArg(const char* flag) {
    boolean_flags_.push_back(flag);
    for (auto i = 1; i < argc_; i++) {
      if (strcmp(argv_[i], flag) == 0) {
        return true;
      }
    }
    return false;
  }
};


} } // namespace utils // namespace toy
