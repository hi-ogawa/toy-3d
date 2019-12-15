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
std::ostream &operator<<(std::ostream& os, const glm::fmat4x4& A) {
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
    GLuint handle_, vertex_shader_, fragment_shader_,  vertex_array_;
    Program(const char* vs_src, const char* fs_src) {
      glGenVertexArrays(1, &vertex_array_);
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
      glDeleteVertexArrays(1, &vertex_array_);
      glDetachShader(handle_, vertex_shader_);
      glDetachShader(handle_, fragment_shader_);
      glDeleteShader(vertex_shader_);
      glDeleteShader(fragment_shader_);
      glDeleteProgram(handle_);
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
