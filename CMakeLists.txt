cmake_minimum_required(VERSION 3.30)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPERIMENTAL_CXX_IMPORT_STD "451f2fe2-a8a2-47c3-bc32-94786d8fc91b")
set(CMAKE_CXX_MODULE_STD ON)

project(tomlcpp LANGUAGES CXX)

add_library(tomlcpp)
target_sources(tomlcpp
    PUBLIC FILE_SET CXX_MODULES FILES
    src/toml.cppm
)
