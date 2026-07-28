set(CMAKE_C_COMPILER "/usr/bin/clang")
set(CMAKE_C_FLAGS "-O3")

# Manually set DPU library paths (pkg-config path is hardcoded to /usr in UPMEM SDK)
set(DPU_INCLUDE_DIRS "${UPMEM_HOME}/include/dpu")
set(DPU_LIBRARIES "-L${UPMEM_HOME}/lib -ldpu")

file(GLOB_RECURSE SRCS *.c "${CMAKE_SOURCE_DIR}/Sources/Microbenchmark/Host/*.c")

add_executable(Microbenchmark_test_host ${SRCS})

target_include_directories(Microbenchmark_test_host PUBLIC "${CMAKE_SOURCE_DIR}/Sources")
target_include_directories(Microbenchmark_test_host PUBLIC "${CMAKE_SOURCE_DIR}/Tests")
target_include_directories(Microbenchmark_test_host PUBLIC "${DPU_INCLUDE_DIRS}")

target_compile_definitions(Microbenchmark_test_host PRIVATE
    DPU_BINARY="${CMAKE_BINARY_DIR}/Tests/MicrobenchmarkTest/Device/Microbenchmark_test_device")

target_link_libraries(Microbenchmark_test_host "${DPU_LIBRARIES}")
target_link_directories(Microbenchmark_test_host PUBLIC "${UPMEM_HOME}/lib")
