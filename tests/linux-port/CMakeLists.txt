cmake_minimum_required(VERSION 3.25)
project(t6_linux_port_smoke LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# This tiny standalone project deliberately tests only platform-independent
# repository tools and the GGPO POSIX port. It does not need SDK graphics
# dependencies or any copyrighted game input.
get_filename_component(REPO_ROOT "${CMAKE_CURRENT_LIST_DIR}/../.." ABSOLUTE)
set(GGPO_ROOT "${REPO_ROOT}/third_party/ggpo/src/lib/ggpo")

file(GLOB GGPO_SOURCES CONFIGURE_DEPENDS
    "${GGPO_ROOT}/*.cpp"
    "${GGPO_ROOT}/backends/*.cpp"
    "${GGPO_ROOT}/network/*.cpp")
list(FILTER GGPO_SOURCES EXCLUDE REGEX "/platform_windows\\.cpp$")
list(APPEND GGPO_SOURCES "${GGPO_ROOT}/platform_linux.cpp")

add_library(ggpo_linux_smoke STATIC ${GGPO_SOURCES})
target_include_directories(ggpo_linux_smoke PUBLIC
    "${REPO_ROOT}/third_party/ggpo/src/include"
    PRIVATE "${GGPO_ROOT}")
target_compile_options(ggpo_linux_smoke PRIVATE -w)

add_subdirectory("${REPO_ROOT}/tools/xdvdfs" "${CMAKE_CURRENT_BINARY_DIR}/xdvdfs")
