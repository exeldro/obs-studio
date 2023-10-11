project(simulcast)

set(PROJECT_NAME simulcast)

find_qt(COMPONENTS Core Widgets Svg Network)

find_package(CURL REQUIRED)

add_library(${PROJECT_NAME} MODULE)
add_library(OBS::${PROJECT_NAME} ALIAS ${PROJECT_NAME})

target_sources(
  ${PROJECT_NAME}
  PRIVATE # cmake-format: sortable
          src/berryessa-every-minute.cpp
          src/berryessa-every-minute.hpp
          src/berryessa-submitter.cpp
          src/berryessa-submitter.hpp
          src/common.h
          src/copy-from-obs/remote-text.cpp
          src/copy-from-obs/remote-text.hpp
          src/global-service.cpp
          src/global-service.h
          src/goliveapi-network.cpp
          src/goliveapi-network.hpp
          src/goliveapi-postdata.cpp
          src/goliveapi-postdata.hpp
          src/immutable-date-time.cpp
          src/immutable-date-time.h
          src/ivs-events.cpp
          src/ivs-events.h
          src/presentmon-csv-capture.cpp
          src/presentmon-csv-capture.hpp
          src/presentmon-csv-parser.cpp
          src/presentmon-csv-parser.hpp
          src/qt-helpers.cpp
          src/qt-helpers.h
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

if(OS_WINDOWS)
  target_sources(simulcast PRIVATE # cmake-format: sortable
                                   src/system-info-windows.cpp src/wmi-data-provider.cpp src/wmi-data-provider.h)
elseif(OS_MACOS)
  target_sources(simulcast PRIVATE # cmake-format: sortable
                                   src/system-info-macos.cpp)
elseif(OS_LINUX)
  target_sources(simulcast PRIVATE # cmake-format: sortable
                                   src/system-info-posix.cpp)
endif()

configure_file(src/plugin-macros.h.in plugin-macros.generated.h)

target_sources(${PROJECT_NAME} PRIVATE plugin-macros.generated.h)

target_link_libraries(${PROJECT_NAME} PRIVATE OBS::libobs OBS::frontend-api Qt::Widgets Qt::Svg Qt::Network
                                              CURL::libcurl)

target_include_directories(${PROJECT_NAME} PRIVATE ${CMAKE_CURRENT_BINARY_DIR})

option(ENABLE_CUSTOM_TWITCH_CONFIG "Enable custom Twitch simulcast config" ON)
if(ENABLE_CUSTOM_TWITCH_CONFIG)
  add_compile_definitions(ENABLE_CUSTOM_TWITCH_CONFIG)
endif()

if(NOT DEFINED SIMULCAST_CUSTOMER OR SIMULCAST_CUSTOMER STREQUAL "twitch")
  add_compile_definitions(
    # cmake-format: sortable
    SIMULCAST_DOCK_ID="twitch-go-live" SIMULCAST_DOCK_STYLE_BACKGROUND_COLOR_HEX="644186"
    SIMULCAST_DOCK_STYLE_COLOR="white" SIMULCAST_DOCK_TITLE="Twitch"
    SIMULCAST_GET_STREAM_KEY_URL="https://dashboard.twitch.tv/settings/stream")
elseif(SIMULCAST_CUSTOMER STREQUAL "ivs")
  add_compile_definitions(
    # cmake-format: sortable
    SIMULCAST_DOCK_ID="amazon-ivs-go-live" SIMULCAST_DOCK_TITLE="Amazon IVS" SIMULCAST_OVERRIDE_RTMP_URL=true)
endif()

set_target_properties(
  ${PROJECT_NAME}
  PROPERTIES FOLDER plugins
             PREFIX ""
             AUTOMOC ON
             AUTOUIC ON
             AUTORCC ON)

if(OS_WINDOWS)
  set_property(
    TARGET ${PROJECT_NAME}
    APPEND
    PROPERTY AUTORCC_OPTIONS --format-version 1)
endif()

setup_plugin_target(${PROJECT_NAME})
