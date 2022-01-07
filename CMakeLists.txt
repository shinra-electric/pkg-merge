cmake_minimum_required(VERSION 3.21)
project(pkg_merge)

set(CMAKE_CXX_STANDARD 23)

add_subdirectory(libs/nativefiledialog-extended)

add_executable(pkg_merge main.cpp)
target_link_libraries(pkg_merge PRIVATE nfd)
