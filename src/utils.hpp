#pragma once

// Cli
#include <cstring>
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
    glGetProgramiv(handle, GL_COMPILE_STATUS, &status);
    glGetProgramiv(handle, GL_INFO_LOG_LENGTH, &log_length);
    if (log_length > 0) {
      log.resize(log_length);
      glGetProgramInfoLog(handle, log_length, NULL, (GLchar*)log.data());
    }
    return make_pair(status == GL_TRUE, log);
  }
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
    std::string _template = "Usage: <program> {} {} {}\n";
    auto join = [](const std::vector<std::string>& v) {
      std::string result;
      for (auto i : range(v.size())) {
        if (i >= 1) result += " ";
        result += v[i];
      }
      return result;
    };
    return fmt::format(
        _template,
        join(boolean_flags_), join(value_flags_),
        has_positional_arg_ ? "<arg-0> <arg-1> ..." : "");
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
