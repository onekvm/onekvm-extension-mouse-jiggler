cmake_minimum_required(VERSION 3.16)
project(onekvm_mouse_jiggler LANGUAGES CXX)

set(ONEKVM_EXTENSION_VERSION "0.0.0-dev" CACHE STRING "Extension version")
set(ONEKVM_EXTENSION_ARCH "unknown" CACHE STRING "Target architecture")

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

find_package(PkgConfig REQUIRED)
pkg_check_modules(JSON_C REQUIRED json-c)
find_file(JSON_C_STATIC_LIBRARY
  NAMES libjson-c.a
  HINTS ${JSON_C_LIBRARY_DIRS}
  NO_DEFAULT_PATH)
if(NOT JSON_C_STATIC_LIBRARY)
  message(FATAL_ERROR "Static json-c library not found")
endif()

add_executable(onekvm-mouse-jiggler src/main.cpp)
target_compile_options(onekvm-mouse-jiggler PRIVATE -Wall -Wextra -Wpedantic)
target_include_directories(onekvm-mouse-jiggler PRIVATE ${JSON_C_INCLUDE_DIRS})
target_link_libraries(onekvm-mouse-jiggler PRIVATE ${JSON_C_STATIC_LIBRARY} m)

set(VERSION "${ONEKVM_EXTENSION_VERSION}")
set(ARCH "${ONEKVM_EXTENSION_ARCH}")
configure_file(manifest.json.in manifest.json @ONLY)

install(TARGETS onekvm-mouse-jiggler RUNTIME DESTINATION bin)
install(FILES web/page.json DESTINATION web)
install(FILES "${CMAKE_CURRENT_BINARY_DIR}/manifest.json" DESTINATION .)
