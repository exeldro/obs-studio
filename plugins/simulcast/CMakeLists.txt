cmake_minimum_required(VERSION 3.16...3.25)

legacy_check()

# find_package(MbedTLS REQUIRED) find_package(ZLIB REQUIRED)

find_package(CURL REQUIRED)

add_library(simulcast MODULE)

target_sources(
  simulcast
  PRIVATE # cmake-format: sortable
          src/common.h
          src/copy-from-obs/remote-text.cpp
          src/copy-from-obs/remote-text.hpp
          src/global-service.cpp
          src/global-service.h
          src/goliveapi-network.cpp
          src/goliveapi-network.hpp
          src/goliveapi-postdata.cpp
          src/goliveapi-postdata.hpp
          src/simulcast-dock-widget.cpp
          src/simulcast-dock-widget.h
          src/simulcast-output.cpp
          src/simulcast-output.h
          src/simulcast-plugin.cpp
          src/simulcast-plugin.h
          src/simulcast-service.cpp
          src/simulcast-settings-window.cpp
          src/simulcast-settings-window.h
          src/system-info.h)

target_link_libraries(
  simulcast
  PRIVATE OBS::libobs
          OBS::frontend-api
          Qt::Core
          Qt::Widgets
          Qt::Svg
          Qt::Network
          CURL::libcurl)

# target_compile_definitions(simulcast PRIVATE USE_MBEDTLS CRYPTO)

if(OS_WINDOWS)
  target_sources(simulcast PRIVATE # cmake-format: sortable
                                   src/system-info-windows.cpp)
elseif(OS_MACOS)
  target_sources(simulcast PRIVATE # cmake-format: sortable
                                   src/system-info-macos.cpp)
elseif(OS_LINUX)
  target_sources(simulcast PRIVATE # cmake-format: sortable
                                   src/system-info-posix.cpp)
endif()

configure_file(src/plugin-macros.h.in plugin-macros.generated.h)
target_sources(simulcast PRIVATE plugin-macros.generated.h)

target_include_directories(simulcast PRIVATE ${CMAKE_CURRENT_BINARY_DIR})

set_target_properties(
  simulcast
  PROPERTIES FOLDER plugins
             PREFIX ""
             AUTOMOC ON
             AUTOUIC ON
             AUTORCC ON)

if(OS_WINDOWS)
  set_property(
    TARGET simulcast
    APPEND
    PROPERTY AUTORCC_OPTIONS --format-version 1)
endif()

# just setting this same property up above doesn't seem to be sufficient?
set_target_properties_obs(simulcast PROPERTIES FOLDER plugins PREFIX "")
