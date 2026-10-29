cmake_minimum_required(VERSION 3.28...3.30)

if(NOT TARGET libobs)
  message(FATAL_ERROR "OBS Core modules require 'libobs' target.")
endif()

option(ENABLE_CORE_MODULES "Enable building OBS Core Modules" ON)
set_property(GLOBAL PROPERTY ENABLE_PLUGINS ${ENABLE_CORE_MODULES})

# FIXME: Remove with OBS 34.0.0
if(DEFINED ENABLE_PLUGINS)
  message(DEPRECATION "The ENABLE_PLUGINS cache variable is deprecated, use 'ENABLE_CORE_MODULES' instead")
  set(ENABLE_CORE_MODULES ${ENABLE_PLUGINS})
endif()

if(NOT ENABLE_CORE_MODULES)
  set_property(GLOBAL APPEND PROPERTY OBS_FEATURES_DISABLED "Core Modules")
  set_obs_core_modules()
  return()
endif()

set_property(GLOBAL APPEND PROPERTY OBS_FEATURES_ENABLED "Core Modules")

macro(check_obs_browser)
  if((OS_WINDOWS AND CMAKE_VS_PLATFORM_NAME MATCHES "(ARM64|x64)") OR OS_MACOS OR OS_LINUX)
    if(NOT EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/obs-browser/CMakeLists.txt")
      message(FATAL_ERROR "Required submodule 'obs-browser' not available.")
    else()
      add_core_module(obs-browser)
    endif()
  else()
    add_custom_target(obs-browser)
    target_disable(obs-browser)
  endif()
endmacro()

macro(check_obs_websocket)
  if(NOT EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/obs-websocket/CMakeLists.txt")
    message(FATAL_ERROR "Required submodule 'obs-websocket' not available.")
  else()
    add_core_module(obs-websocket)
  endif()
endmacro()

# Add plugins in alphabetical order to retain order in IDE projects
add_core_module(
    aja
    PLATFORMS WINDOWS MACOS LINUX
    WITH_MESSAGE
)
add_core_module(aja-output-ui PLATFORMS WINDOWS MACOS LINUX WITH_MESSAGE)
add_core_module(coreaudio-encoder PLATFORMS WINDOWS MACOS)
add_core_module(
    decklink
    PLATFORMS WINDOWS MACOS LINUX
    WITH_MESSAGE
)
add_core_module(
    decklink-captions
    PLATFORMS WINDOWS MACOS LINUX
    WITH_MESSAGE
)
add_core_module(
    decklink-output-ui
    PLATFORMS WINDOWS MACOS LINUX
    WITH_MESSAGE
)
add_core_module(
    frontend-tools
    PLATFORMS WINDOWS MACOS LINUX
    WITH_MESSAGE
)
add_core_module(image-source)
add_core_module(linux-alsa PLATFORMS LINUX FREEBSD OPENBSD)
add_core_module(linux-capture PLATFORMS LINUX FREEBSD OPENBSD)
add_core_module(linux-jack PLATFORMS LINUX FREEBSD OPENBSD)
add_core_module(linux-pipewire PLATFORMS LINUX FREEBSD OPENBSD)
add_core_module(linux-pulseaudio PLATFORMS LINUX FREEBSD OPENBSD)
add_core_module(linux-v4l2 PLATFORMS LINUX FREEBSD OPENBSD)
add_core_module(mac-avcapture PLATFORMS MACOS)
add_core_module(mac-capture PLATFORMS MACOS)
add_core_module(mac-syphon PLATFORMS MACOS)
add_core_module(mac-videotoolbox PLATFORMS MACOS)
add_core_module(mac-virtualcam PLATFORMS MACOS)
add_core_module(nv-filters PLATFORMS WINDOWS)

check_obs_browser()

add_core_module(obs-ffmpeg)
add_core_module(obs-filters)
add_core_module(obs-libfdk)
add_core_module(obs-nvenc PLATFORMS WINDOWS LINUX)
add_core_module(obs-outputs)
add_core_module(
    obs-qsv11
    PLATFORMS WINDOWS LINUX
    ARCHITECTURES x64 x86_64
)
add_core_module(obs-text PLATFORMS WINDOWS)
add_core_module(obs-transitions)
add_core_module(
    obs-vst
    PLATFORMS WINDOWS MACOS LINUX
    WITH_MESSAGE
)
add_core_module(obs-webrtc)

check_obs_websocket()

add_core_module(obs-x264)
add_core_module(oss-audio PLATFORMS FREEBSD OPENBSD)
add_core_module(rtmp-services)
add_core_module(sndio PLATFORMS LINUX FREEBSD OPENBSD)
add_core_module(text-freetype2)
add_core_module(vlc-video WITH_MESSAGE)
add_core_module(win-capture PLATFORMS WINDOWS)
add_core_module(win-dshow PLATFORMS WINDOWS)
add_core_module(win-wasapi PLATFORMS WINDOWS)

set_obs_core_modules()

add_custom_module(3d-effect)
add_custom_module(obs-advanced-masks)
add_custom_module(obs-composite-blur)
add_custom_module(dir-watch-media)
add_custom_module(browser-transition)
add_custom_module(face-mask)
add_custom_module(freeze-filter)
add_custom_module(gradient-source)
add_custom_module(html-source)
add_custom_module(markdown)
add_custom_module(move-transition)
add_custom_module(obs-noise)
add_custom_module(obs-shaderfilter)
add_custom_module(playout-source)
add_custom_module(recursion-effect)
add_custom_module(replay-source)
add_custom_module(source-clone)
add_custom_module(source-switcher)
add_custom_module(source-record)
add_custom_module(obs-stroke-glow-shadow)
add_custom_module(svg-source)
add_custom_module(time-warp-scan)
#add_custom_module(obs-streamelements-core)
#add_custom_module(live-obs)

# add_custom_module(SceneSwitcher)
add_custom_module(media-controls)
add_custom_module(transition-table)
# add_custom_module(streamfx)
add_custom_module(audio-monitor)
add_custom_module(downstream-keyer)
add_custom_module(source-copy)
add_custom_module(source-dock)
add_custom_module(streamup)
# add_custom_module(setting-docks)
add_custom_module(scene-notes-dock)
add_custom_module(scene-collection-manager)
# add_custom_module(device-switcher)
# add_custom_module(multi-canvas)
add_custom_module(vertical-canvas)
add_custom_module(stream-delay)
# add_custom_module(obs_scene_tree_view)
# add_custom_module(StreamBeats)
add_custom_module(streamdeck)
add_custom_module(source-profiler)
add_custom_module(distroav)
add_custom_module(aitum-multistream)
add_custom_module(aitum-stream-suite)
add_custom_module(draw-dock)
add_custom_module(record-chapter-manager)
add_custom_module(obs-quick-access-utiltiy)
# add_custom_module(obs-urlsource)
add_custom_module(controls-dock)
add_custom_module(record-rename)
add_custom_module(streamup-bbcode)
add_custom_module(studio-player)
# add_custom_module(spark-plug)
