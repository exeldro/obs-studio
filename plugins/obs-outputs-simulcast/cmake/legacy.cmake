project(obs-outputs-simulcast)

if(NOT DEFINED ENABLE_RTMPS)
  set(ENABLE_RTMPS
      AUTO
      CACHE STRING "Enable RTMPS support with mbedTLS" FORCE)
  set_property(CACHE ENABLE_RTMPS PROPERTY STRINGS AUTO ON OFF)
endif()

option(ENABLE_STATIC_MBEDTLS "Enable statically linking mbedTLS into binary" OFF)
mark_as_advanced(ENABLE_STATIC_MBEDTLS)

add_library(obs-outputs-simulcast MODULE)

target_sources(
  obs-outputs-simulcast
  PRIVATE obs-outputs.c
          obs-output-ver.h
          flv-mux.c
          flv-mux.h
          flv-output.c
          net-if.c
          net-if.h
          null-output.c
          rtmp-helpers.h
          rtmp-stream.c
          rtmp-stream.h
          rtmp-windows.c
          librtmp/amf.c
          librtmp/amf.h
          librtmp/bytes.h
          librtmp/cencode.c
          librtmp/cencode.h
          librtmp/handshake.h
          librtmp/hashswf.c
          librtmp/http.h
          librtmp/log.c
          librtmp/log.h
          librtmp/md5.c
          librtmp/md5.h
          librtmp/parseurl.c
          librtmp/rtmp.c
          librtmp/rtmp.h
          librtmp/rtmp_sys.h)

target_link_libraries(obs-outputs-simulcast PRIVATE OBS::libobs)

set_target_properties(obs-outputs-simulcast PROPERTIES FOLDER "plugins" PREFIX "")

if(OS_WINDOWS)
  set(MODULE_DESCRIPTION "OBS output module")
  configure_file(${CMAKE_SOURCE_DIR}/cmake/bundle/windows/obs-module.rc.in obs-outputs.rc)

  target_sources(obs-outputs-simulcast PRIVATE obs-outputs.rc)

  if(MSVC)
    target_link_libraries(obs-outputs-simulcast PRIVATE OBS::w32-pthreads)
    target_link_options(obs-outputs-simulcast PRIVATE "LINKER:/IGNORE:4098" "LINKER:/IGNORE:4099")
  endif()

  target_link_libraries(obs-outputs-simulcast PRIVATE ws2_32 winmm Iphlpapi)
endif()

if(ENABLE_RTMPS STREQUAL "AUTO" OR ENABLE_RTMPS STREQUAL "ON")
  find_package(MbedTLS)
  find_package(ZLIB)
  if(NOT MBEDTLS_FOUND OR NOT ZLIB_FOUND)
    if(ENABLE_RTMPS STREQUAL "ON")
      obs_status(FATAL_ERROR "mbedTLS or zlib not found, but required for RTMPS support.")
      return()
    else()
      obs_status(WARNING "mbedTLS or zlib was not found, RTMPS will be automatically disabled.")
      target_compile_definitions(obs-outputs-simulcast PRIVATE NO_CRYPTO)
    endif()
  else()
    target_compile_definitions(obs-outputs-simulcast PRIVATE USE_MBEDTLS CRYPTO)

    target_link_libraries(obs-outputs-simulcast PRIVATE Mbedtls::Mbedtls ZLIB::ZLIB)

    if(OS_WINDOWS)
      target_link_libraries(obs-outputs-simulcast PRIVATE crypt32)

    elseif(OS_MACOS)
      find_library(FOUNDATION_FRAMEWORK Foundation)
      find_library(SECURITY_FRAMEWORK Security)
      mark_as_advanced(FOUNDATION_FRAMEWORK SECURITY_FRAMEWORK)

      target_link_libraries(obs-outputs-simulcast PRIVATE ${FOUNDATION_FRAMEWORK} ${SECURITY_FRAMEWORK})
      set_target_properties(obs-outputs-simulcast PROPERTIES CXX_VISIBILITY_PRESET hidden)
      set_target_properties(obs-outputs-simulcast PROPERTIES C_VISIBILITY_PRESET hidden)

    elseif(OS_POSIX)
      set_target_properties(obs-outputs-simulcast PROPERTIES CXX_VISIBILITY_PRESET hidden)
      set_target_properties(obs-outputs-simulcast PROPERTIES C_VISIBILITY_PRESET hidden)
    endif()
  endif()
else()
  target_compile_definitions(obs-outputs-simulcast PRIVATE NO_CRYPTO)
endif()

setup_plugin_target(obs-outputs-simulcast)
