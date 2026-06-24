cmake_minimum_required(VERSION 3.20)
project(bourse LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Release)
endif()
add_compile_options(-Wall -Wextra -Wpedantic)

# --- core engine, built as a library so tests, bench, and bindings share it ---
add_library(bourse_core src/order_book.cpp)
target_include_directories(bourse_core PUBLIC include)

# --- correctness suite ---
add_executable(bourse_tests tests/test_main.cpp tests/test_order_book.cpp)
target_link_libraries(bourse_tests PRIVATE bourse_core)
target_include_directories(bourse_tests PRIVATE tests)
enable_testing()
add_test(NAME correctness COMMAND bourse_tests)

# --- throughput benchmark ---
add_executable(bourse_bench tests/bench.cpp)
target_link_libraries(bourse_bench PRIVATE bourse_core)

# --- Phase 2: Python bindings. Uncomment once pybind11 is installed. ---
# find_package(pybind11 CONFIG REQUIRED)
# pybind11_add_module(_bourse bindings/pybind_module.cpp)
# target_link_libraries(_bourse PRIVATE bourse_core)