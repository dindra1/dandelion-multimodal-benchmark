# Phase 3 will populate individual node targets here.
# For now: a single stub target that verifies the common library links correctly.
add_executable(dandelion_stub dandelion_stub.cpp)
target_link_libraries(dandelion_stub PRIVATE bench_common)
