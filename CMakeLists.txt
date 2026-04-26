cmake_minimum_required(VERSION 3.22)
project(dandelion_multimodal_benchmark LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_subdirectory(src/monolith)
add_subdirectory(src/dandelion)
add_subdirectory(src/lambda_baseline)
