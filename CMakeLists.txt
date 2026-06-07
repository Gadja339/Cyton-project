cmake_minimum_required(VERSION 3.30)

project(Cyton
    VERSION 0.1
    LANGUAGES CXX
)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

add_executable(cyton
    src/main.cpp
    src/lexer.cpp
    src/parser.cpp
    src/semantic.cpp
    src/interpreter.cpp
)

target_compile_features(cyton PRIVATE
    cxx_std_23
)

target_compile_options(cyton PRIVATE
    -Wall -Wextra -Wpedantic
)

target_sources(cyton
    PRIVATE FILE_SET CXX_MODULES FILES
        src/token.cppm
        src/ast.cppm
        src/lexer.cppm
        src/parser.cppm
        src/semantic.cppm
        src/interpreter.cppm
)
