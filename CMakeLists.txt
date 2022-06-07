# Copyright 2022 Imperial College London
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#     http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

cmake_minimum_required(VERSION 3.13)

project(
  remove-parens
  VERSION 1.0
  DESCRIPTION "remove-parens"
  LANGUAGES CXX)

# Third party targets.

set(LLVM_DIR "${CMAKE_SOURCE_DIR}/third_party/clang+llvm-13.0.1/lib/cmake/llvm")

set(Clang_DIR
    "${CMAKE_SOURCE_DIR}/third_party/clang+llvm-13.0.1/lib/cmake/clang")

if(NOT EXISTS "${LLVM_DIR}/LLVMConfig.cmake")
  message(FATAL_ERROR "Could not find LLVMConfig.cmake under ${LLVM_DIR}. \
Make sure you have downloaded a release of Clang/LLVM.")
endif()

if(NOT EXISTS "${Clang_DIR}/ClangConfig.cmake")
  message(FATAL_ERROR "Could not find ClangConfig.cmake under ${Clang_DIR}. \
Make sure you have downloaded a release of Clang/LLVM.")
endif()

find_package(LLVM REQUIRED CONFIG)
find_package(Clang REQUIRED)

include_directories(SYSTEM ${LLVM_INCLUDE_DIRS})
include_directories(SYSTEM ${CLANG_INCLUDE_DIRS})
add_definitions(${LLVM_DEFINITIONS})

# Disable RTTI, as LLVM releases are not built with RTTI
if(MSVC)

else()
  add_compile_options(-fno-rtti)
endif()

add_executable(remove-parens src/main.cc)
target_link_libraries(remove-parens PRIVATE clangTooling)
