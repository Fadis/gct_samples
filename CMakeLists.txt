cmake_minimum_required(VERSION 3.10)
set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH}
  "${CMAKE_CURRENT_SOURCE_DIR}/cmake/")
PROJECT(
  gct_samples
  LANGUAGES CXX
  HOMEPAGE_URL https://github.com/Fadis/gct_samples
  DESCRIPTION "GPU Computing Toolkit Samples"
  VERSION 0.1.0
)
include(CheckIncludeFiles)
include(FindPkgConfig)
include(CheckCXXSourceCompiles)

# build type
set(CMAKE_BUILD_TYPE Debug)

set( ADDITIONAL_COMPILE_FLAGS )
option( ENABLE_ADDRSAN "build with address sanitizer" OFF )
if( ENABLE_ADDRSAN )
  list(APPEND ADDITIONAL_COMPILE_FLAGS "-fsanitize=address" )
  if( CMAKE_CXX_COMPILER_ID STREQUAL "GNU" )
    list(APPEND ADDITIONAL_COMPILE_FLAGS "-static-libasan" )
  endif()
endif()
option( ENABLE_PCOMPSAN "build with pointer compare sanitizer" OFF )
if( ENABLE_PCOMPSAN )
  list(APPEND ADDITIONAL_COMPILE_FLAGS "-fsanitize=pointer-compare" )
endif()
option( ENABLE_PSUBSAN "build with pointer subtract sanitizer" OFF )
if( ENABLE_PSUBSAN )
  list(APPEND ADDITIONAL_COMPILE_FLAGS "-fsanitize=pointer-subtract" )
endif()
option( ENABLE_THREADSAN "build with thread sanitizer" OFF )
if( ENABLE_THREADSAN )
  list(APPEND ADDITIONAL_COMPILE_FLAGS "-fsanitize=thread" )
endif()
option( ENABLE_LEAKSAN "build with leak sanitizer" OFF )
if( ENABLE_LEAKSAN )
  list(APPEND ADDITIONAL_COMPILE_FLAGS "-fsanitize=leak" )
endif()
option( ENABLE_UBSAN "build with undefined behavior sanitizer" OFF )
if( ENABLE_UBSAN )
  list(APPEND ADDITIONAL_COMPILE_FLAGS "-fsanitize=undefined" )
endif()
option( ENABLE_WERROR "build with -Werror" ON )
list( APPEND ADDITIONAL_COMPILE_FLAGS "-Wall" )
if( ENABLE_WERROR )
  list( APPEND ADDITIONAL_COMPILE_FLAGS "-Werror" )
endif()
option( ENABLE_CXX20 "build with -std=c++20" OFF)
set( CMAKE_CXX_STANDARD "20" )

# compile option (Debug) 
set(CMAKE_CXX_FLAGS_DEBUG "-O0 -g ${ADDITIONAL_COMPILER_FLAGS}")
set(CMAKE_C_FLAGS_DEBUG "-O0 -g ${ADDITIONAL_COMPILER_FLAGS}")

# compile option (Release)
set(CMAKE_CXX_FLAGS_RELEASE "-march=native -O2 ${ADDITIONAL_COMPILER_FLAGS}")
set(CMAKE_C_FLAGS_RELEASE "-march=native -O2 ${ADDITIONAL_COMPILER_FLAGS}")

#if you don't want the full compiler output, remove the following line
set(CMAKE_VERBOSE_MAKEFILE OFF)

find_package(Threads REQUIRED)
set(Boost_USE_MULTITHREADED ON)
find_package(Boost 1.68.0 COMPONENTS program_options context system REQUIRED )
find_package(JSON REQUIRED)
find_package(LibXCB REQUIRED)
pkg_check_modules(Vulkan REQUIRED vulkan)
pkg_check_modules(VULKAN2JSON REQUIRED vulkan2json)
pkg_check_modules(GCT REQUIRED libgct)
pkg_check_modules(GCTGLFW REQUIRED libgct_glfw)
find_package(GLSLC REQUIRED)
find_package(ASTCEnc REQUIRED)
find_package(OpenImageIO REQUIRED)


INCLUDE_DIRECTORIES(
  ${CMAKE_CURRENT_SOURCE_DIR}/include
  ${CMAKE_CURRENT_BINARY_DIR}/include
  ${CMAKE_CURRENT_SOURCE_DIR}/external/VulkanMemoryAllocator/include
  ${Boost_INCLUDE_DIRS}
  ${JSON_INCLUDE_DIRS}
  ${Vulkan_INCLUDE_DIRS}
  ${VULKAN2JSON_INCLUDE_DIR}
  ${GCT_INCLUDE_DIR}
  ${GCT_GLFW_INCLUDE_DIR}
)
link_directories(
  ${Boost_LIBRARY_DIRS}
  ${Vulkan_LIBRARY_DIRS}
  ${VULKAN2JSON_LIBRARY_DIR}
  ${GCT_LIBRARY_DIR}
  ${GCT_GLFW_LIBRARY_DIR}
)
link_libraries(
  Threads::Threads
  Boost::program_options
  Boost::system
  ${Vulkan_LIBRARIES}
  ${GCT_LIBRARIES}
  ${GCTGLFW_LIBRARIES}
  ${pkgcfg_lib_Vulkan_vulkan}
  ${OIIO_LIBRARIES}
  ${pkgcfg_lib_GLFW_glfw}
  ${pkgcfg_lib_GLFW_glfw3}
)


add_definitions( -DVK_ENABLE_BETA_EXTENSIONS -DVULKAN_HPP_DISPATCH_LOADER_DYNAMIC=1 )

set( EASYPACK_VENDOR "fadis" )
set( EASYPACK_RELEASE "1" )
set( EASYPACK_MAINTAINER "Naomasa Matsubayashi <foo@exmple.com>" )
set( EASYPACK_COMPRESSION_TYPE "xz" )
set( EASYPACK_LICENSE_NAME "MIT" )
include(EasyPack)

include(AddShader)
include(AddASTC)

subdirs( src )

