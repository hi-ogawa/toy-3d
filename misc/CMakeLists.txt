add_executable(stbtt_example stbtt_example.cpp)
target_link_libraries(stbtt_example PRIVATE stb)

add_executable(yocto_bvh_example yocto_bvh_example.cpp)
target_link_libraries(yocto_bvh_example PRIVATE yocto fmt)
