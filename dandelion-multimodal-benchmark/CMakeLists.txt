cmake_minimum_required(VERSION 3.22)
project(dandelion_benchmark CXX C)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE RelWithDebInfo)
endif()

# Compiler flags — MSVC vs GCC/Clang
if(MSVC)
  add_compile_options(/O2 /W3 /WX-)
else()
  add_compile_options(-O2 -march=native -Wall -Wextra)
endif()

# --- Optional dependencies (OFF by default; enabled in Phase 2-4) ---
option(WITH_ONNXRUNTIME "Enable ONNX Runtime (required for Phase 2+)" OFF)
option(WITH_TESSERACT   "Enable Tesseract OCR (required for Phase 2+)" OFF)
option(WITH_WHISPER     "Enable whisper.cpp ASR (required for Phase 2+)" ON)

if(WITH_ONNXRUNTIME)
  find_library(ONNXRUNTIME_LIB onnxruntime REQUIRED PATHS ${ONNXRUNTIME_DIR}/lib)
  find_path(ONNXRUNTIME_INCLUDE onnxruntime_cxx_api.h REQUIRED PATHS ${ONNXRUNTIME_DIR}/include)
  message(STATUS "ONNX Runtime: ${ONNXRUNTIME_LIB}")
else()
  message(STATUS "ONNX Runtime: disabled (Phase 3+)")
endif()

if(WITH_TESSERACT)
  find_library(TESSERACT_LIB tesseract REQUIRED)
  find_library(LEPTONICA_LIB leptonica REQUIRED)
  message(STATUS "Tesseract: ${TESSERACT_LIB}")
else()
  message(STATUS "Tesseract: via import lib (third_party/tesseract-import)")
endif()

if(WITH_WHISPER)
  if(EXISTS "${CMAKE_SOURCE_DIR}/third_party/whisper.cpp/CMakeLists.txt")
    # Suppress whisper's own tests and examples to keep the build fast
    set(WHISPER_BUILD_TESTS    OFF CACHE BOOL "" FORCE)
    set(WHISPER_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
    set(GGML_OPENMP            OFF CACHE BOOL "" FORCE)
    add_subdirectory(third_party/whisper.cpp EXCLUDE_FROM_ALL)
    message(STATUS "whisper.cpp: enabled (submodule)")
  else()
    message(FATAL_ERROR "WITH_WHISPER=ON but submodule missing. Run: git submodule update --init --recursive")
  endif()
else()
  message(STATUS "whisper.cpp: disabled")
endif()

# --- Shared metrics/utility library (no external deps) ---
add_library(bench_common STATIC
  src/dandelion/common/metrics.cpp
  src/dandelion/common/json_writer.cpp
)
target_include_directories(bench_common PUBLIC src/dandelion/common)

if(WITH_ONNXRUNTIME)
  target_include_directories(bench_common PUBLIC ${ONNXRUNTIME_INCLUDE})
endif()

# --- Three build targets ---
add_subdirectory(src/monolith)
add_subdirectory(src/dandelion)
add_subdirectory(src/lambda_baseline)
