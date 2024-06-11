cmake_minimum_required(VERSION 3.10)

project(clox VERSION 1.0 LANGUAGES C)

set(CMAKE_BUILD_TYPE Debug)
set(CMAKE_INSTALL_PREFIX "/home/adrian/local")

add_subdirectory(CloxCore)
