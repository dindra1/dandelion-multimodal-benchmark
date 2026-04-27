# Dandelion compute functions — cross-compiled with DandelionSDK.
#
# Built by wsl_build.sh inside WSL2. Not part of the Windows CMake build.
#
# Invoke via wsl_build.sh which sets:
#   -DDANDELION_SDK_INSTALL=<path>   (the installed SDK directory)
#
# The installed SDK directory contains:
#   CMakeLists.txt  — defines dandelion_runtime, dlibc, dlibcxx interface targets
#   *.cmake         — toolchain + compiler config (used by wsl_build.sh)
#   lib/            — prebuilt runtime libraries
#   include/        — dandelion/runtime.h, dandelion/io_buffer.h, dandelion/crt.h

cmake_minimum_required(VERSION 3.22)
project(dandelion_nodes C)

if(NOT DEFINED DANDELION_SDK_INSTALL)
    message(FATAL_ERROR "DANDELION_SDK_INSTALL must point to the installed DandelionSDK directory.")
endif()

# Pull in the SDK's CMakeLists which defines dandelion_runtime target.
include(${DANDELION_SDK_INSTALL}/CMakeLists.txt)

# Output directory for ELFs (wsl_build.sh's ELF_DIR).
set(ELF_DIR "${CMAKE_BINARY_DIR}/../dandelion_elfs")

# Helper macro: add one Dandelion compute function target.
macro(dn_node TARGET SRC)
    add_executable(${TARGET} ${SRC})
    target_include_directories(${TARGET} PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
    target_link_libraries(${TARGET} PRIVATE dandelion_runtime)
    set_target_properties(${TARGET} PROPERTIES OUTPUT_NAME "${TARGET}" SUFFIX ".elf")
    add_custom_command(TARGET ${TARGET} POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E make_directory "${ELF_DIR}"
        COMMAND ${CMAKE_COMMAND} -E copy "$<TARGET_FILE:${TARGET}>" "${ELF_DIR}/${TARGET}.elf"
    )
endmacro()

# --- Fine-grained DAG (7 nodes) ---
dn_node(ingress        ingress.c)
dn_node(normalize      normalize.c)
dn_node(classify       classify.c)
dn_node(audio_asr      audio_asr.c)
dn_node(image_ocr      image_ocr.c)
dn_node(format_output  format_output.c)

# --- Coarse-grained DAG (3 nodes + ingress) ---
dn_node(coarse_text    coarse_text.c)
dn_node(coarse_audio   coarse_audio.c)
dn_node(coarse_image   coarse_image.c)

# --- Monolithic DAG (1 node) ---
dn_node(mono_all       mono_all.c)
