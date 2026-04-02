add_library(tomlcpp)
target_sources(tomlcpp
    PUBLIC FILE_SET CXX_MODULES FILES
    toml.cppm
)
