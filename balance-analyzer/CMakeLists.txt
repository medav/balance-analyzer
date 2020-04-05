cmake_minimum_required(VERSION 3.7)
project(balance-analyzer)

find_package(LLVM REQUIRED CONFIG)

message(STATUS "Found LLVM ${LLVM_PACKAGE_VERSION}")
message(STATUS "Using LLVMConfig.cmake in: ${LLVM_DIR}")

include_directories(${LLVM_INCLUDE_DIRS})
add_definitions(${LLVM_DEFINITIONS})

llvm_map_components_to_libnames(llvm_libs support core irreader)

set(SOURCE_FILES main.cpp)

add_executable(balance-analyzer ${SOURCE_FILES})
target_link_libraries(balance-analyzer ${llvm_libs})
