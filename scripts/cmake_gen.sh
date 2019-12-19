#
# NOTE:
# - out-of-source tree build (as in `-B ../toy-3d-build`) is necessary to get absolute path used by Ninja
#   which helps __FILE__ macro and compile error output for IDE, etc...
#   (cf. https://gitlab.kitware.com/cmake/cmake/issues/13894)
#
CC=clang CXX=clang++ LDFLAGS=-fuse-ld=lld cmake -B ../toy-3d-build -G Ninja -DCMAKE_EXPORT_COMPILE_COMMANDS=ON -DCMAKE_BUILD_TYPE=Debug
