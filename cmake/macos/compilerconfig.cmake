# OBS CMake macOS compiler configuration module

include_guard(GLOBAL)

include(ccache)
include(compiler_common)

add_compile_options("$<$<NOT:$<COMPILE_LANGUAGE:Swift>>:-fopenmp-simd>")

# Enable selection between arm64 and x86_64 targets
if(NOT CMAKE_OSX_ARCHITECTURES)
  set(CMAKE_OSX_ARCHITECTURES
      arm64
      CACHE STRING "Build architectures for macOS" FORCE)
endif()
set_property(CACHE CMAKE_OSX_ARCHITECTURES PROPERTY STRINGS arm64 x86_64)

# Ensure recent enough Xcode and platform SDK
function(check_sdk_requirements)
  set(obs_macos_minimum_sdk 13.1) # Keep in sync with Xcode
  set(obs_macos_minimum_xcode 14.2) # Keep in sync with SDK
  execute_process(
    COMMAND xcrun --sdk macosx --show-sdk-platform-version
    OUTPUT_VARIABLE obs_macos_current_sdk
    RESULT_VARIABLE result
    OUTPUT_STRIP_TRAILING_WHITESPACE
  )
  if(NOT result EQUAL 0)
    message(
      FATAL_ERROR
      "Failed to fetch macOS SDK version. "
      "Ensure that the macOS SDK is installed and that xcode-select points at the Xcode developer directory."
    )
  endif()
  message(DEBUG "macOS SDK version: ${obs_macos_current_sdk}")
  if(obs_macos_current_sdk VERSION_LESS obs_macos_minimum_sdk)
    message(
      FATAL_ERROR
      "Your macOS SDK version (${obs_macos_current_sdk}) is too low. "
      "The macOS ${obs_macos_minimum_sdk} SDK (Xcode ${obs_macos_minimum_xcode}) is required to build OBS."
    )
  endif()
  execute_process(COMMAND xcrun --find xcodebuild OUTPUT_VARIABLE obs_macos_xcodebuild RESULT_VARIABLE result)
  if(NOT result EQUAL 0)
    message(
      FATAL_ERROR
      "Xcode was not found. "
      "Ensure you have installed Xcode and that xcode-select points at the Xcode developer directory."
    )
  endif()
  message(DEBUG "Path to xcodebuild binary: ${obs_macos_xcodebuild}")
  if(XCODE_VERSION VERSION_LESS obs_macos_minimum_xcode)
    message(
      FATAL_ERROR
      "Your Xcode version (${XCODE_VERSION}) is too low. Xcode ${obs_macos_minimum_xcode} is required to build OBS."
    )
  endif()
endfunction()

check_sdk_requirements()

if(XCODE)
  # Enable dSYM generator for release builds
  string(APPEND CMAKE_C_FLAGS_RELEASE " -g")
  string(APPEND CMAKE_CXX_FLAGS_RELEASE " -g")
else()
  option(ENABLE_COMPILER_TRACE "Enable clang time-trace (requires Ninja)" OFF)
  mark_as_advanced(ENABLE_COMPILER_TRACE)

  # clang options for ObjC
  set(_obs_clang_objc_options
      ${_obs_clang_common_options}
      -Wno-implicit-atomic-properties
      -Wno-objc-interface-ivars
      -Warc-repeated-use-of-weak
      -Wno-arc-maybe-repeated-use-of-weak
      -Wimplicit-retain-self
      -Wduplicate-method-match
      -Wshadow
      -Wfloat-conversion
      -Wobjc-literal-conversion
      -Wno-selector
      -Wno-strict-selector-match
      -Wundeclared-selector
      -Wdeprecated-implementations
      -Wprotocol
      -Werror=block-capture-autoreleasing
      -Wrange-loop-analysis)

  # clang options for ObjC++
  set(_obs_clang_objcxx_options ${_obs_clang_objc_options} -Wno-non-virtual-dtor)

  # cmake-format: off
  add_compile_options(
    "$<$<COMPILE_LANGUAGE:C>:${_obs_clang_c_options}>"
    "$<$<COMPILE_LANGUAGE:CXX>:${_obs_clang_cxx_options}>"
    "$<$<COMPILE_LANGUAGE:OBJC>:${_obs_clang_objc_options}>"
    "$<$<COMPILE_LANGUAGE:OBJCXX>:${_obs_clang_objcxx_options}>")
  # cmake-format: on

  # Enable stripping of dead symbols when not building for Debug configuration
  set(_release_configs RelWithDebInfo Release MinSizeRel)
  if(CMAKE_BUILD_TYPE IN_LIST _release_configs)
    add_link_options(LINKER:-dead_strip)
  endif()

  # Enable color diagnostics for AppleClang
  set(CMAKE_COLOR_DIAGNOSTICS ON)

  # Add time trace option to compiler, if enabled.
  if(ENABLE_COMPILER_TRACE AND CMAKE_GENERATOR STREQUAL "Ninja")
    add_compile_options($<$<NOT:$<COMPILE_LANGUAGE:Swift>>:-ftime-trace>)
  else()
    set(ENABLE_COMPILER_TRACE
        OFF
        CACHE BOOL "Enable clang time-trace (requires Ninja)" FORCE)
  endif()
endif()

add_compile_definitions(
  "$<$<NOT:$<COMPILE_LANGUAGE:Swift>>:$<$<CONFIG:DEBUG>:DEBUG>;$<$<CONFIG:DEBUG>:_DEBUG>;SIMDE_ENABLE_OPENMP>")
