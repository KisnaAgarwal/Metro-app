cmake_minimum_required(VERSION 3.16)
project(MetroMitra LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

add_library(metromitra_graph
    backend/src/Graph.cpp
    backend/src/CsvLoader.cpp
    backend/src/Traversal.cpp
    backend/src/Dijkstra.cpp
)
target_include_directories(metromitra_graph PUBLIC backend/include)

add_executable(metromitra backend/src/main.cpp)
target_link_libraries(metromitra PRIVATE metromitra_graph)

include(CTest)
if(BUILD_TESTING)
    add_executable(metromitra_tests backend/tests/GraphTests.cpp)
    target_link_libraries(metromitra_tests PRIVATE metromitra_graph)
    target_compile_definitions(metromitra_tests PRIVATE METROMITRA_SOURCE_DIR="${CMAKE_CURRENT_SOURCE_DIR}")
    add_test(NAME metromitra_tests COMMAND metromitra_tests)
endif()
