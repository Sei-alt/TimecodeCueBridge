cmake_minimum_required(VERSION 3.24)
project(TimecodeCueBridge VERSION 0.5.1 LANGUAGES C CXX)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
add_compile_options($<$<COMPILE_LANG_AND_ID:CXX,MSVC>:/utf-8>)
# All Windows binaries use the static CRT; no separate VC redistributable installer.
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
if(MINGW)
  add_link_options(-static -static-libgcc -static-libstdc++)
endif()

option(TCB_BUILD_GUI "Build the JUCE GUI" ON)
option(TCB_BUILD_INTEGRATION_TESTS "Test generated LTC waveform through OSC/UDP" ON)
include(CTest)
add_library(tcb_core STATIC src/CueEngine.cpp src/CueText.cpp src/MidiAction.cpp)
target_include_directories(tcb_core PUBLIC src)
if(BUILD_TESTING)
  add_executable(tcb_core_tests tests/test_cue_engine.cpp)
  target_link_libraries(tcb_core_tests PRIVATE tcb_core)
  add_test(NAME cue_engine COMMAND tcb_core_tests)
endif()
if(TCB_BUILD_GUI OR TCB_BUILD_INTEGRATION_TESTS)
  include(FetchContent)
  # Prefer bundled sources: delivered source ZIP builds without dependency downloads.
  if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/third_party/JUCE/CMakeLists.txt" AND NOT FETCHCONTENT_SOURCE_DIR_JUCE)
    set(FETCHCONTENT_SOURCE_DIR_JUCE "${CMAKE_CURRENT_SOURCE_DIR}/third_party/JUCE")
  endif()
  if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/third_party/libltc/src/ltc.h" AND NOT FETCHCONTENT_SOURCE_DIR_LIBLTC)
    set(FETCHCONTENT_SOURCE_DIR_LIBLTC "${CMAKE_CURRENT_SOURCE_DIR}/third_party/libltc")
  endif()
  FetchContent_Declare(JUCE GIT_REPOSITORY https://github.com/juce-framework/JUCE.git GIT_TAG 8.0.12 GIT_SHALLOW TRUE)
  FetchContent_MakeAvailable(JUCE)
  if(CMAKE_CROSSCOMPILING AND TCB_WINDOWS_SDK)
    execute_process(COMMAND "${TCB_PYTHON}" "${CMAKE_CURRENT_SOURCE_DIR}/cmake/make-sdk-overlay.py"
      "${TCB_WINDOWS_SDK}" "${TCB_WINDOWS_SDK}/tcb-overlay.json" "${juce_SOURCE_DIR}"
      COMMAND_ERROR_IS_FATAL ANY)
  endif()
  FetchContent_Declare(libltc GIT_REPOSITORY https://github.com/x42/libltc.git GIT_TAG v1.3.2 GIT_SHALLOW TRUE)
  FetchContent_GetProperties(libltc)
  if(NOT libltc_POPULATED)
    FetchContent_Populate(libltc)
  endif()
  add_library(tcb_ltc STATIC "${libltc_SOURCE_DIR}/src/ltc.c" "${libltc_SOURCE_DIR}/src/decoder.c"
    "${libltc_SOURCE_DIR}/src/encoder.c" "${libltc_SOURCE_DIR}/src/timecode.c")
  target_include_directories(tcb_ltc PUBLIC "${libltc_SOURCE_DIR}/src")
  if(NOT WIN32)
    target_link_libraries(tcb_ltc PUBLIC m)
  endif()
  add_library(tcb_decode STATIC src/LtcDecoder.cpp src/LtcStream.cpp)
  target_link_libraries(tcb_decode PUBLIC tcb_ltc)
  target_include_directories(tcb_decode PUBLIC src)
  add_library(tcb_midi INTERFACE)
  target_sources(tcb_midi INTERFACE "${CMAKE_CURRENT_SOURCE_DIR}/src/MidiOutput.cpp" "${CMAKE_CURRENT_SOURCE_DIR}/src/SceneSettings.cpp")
  target_link_libraries(tcb_midi INTERFACE tcb_core juce::juce_audio_devices)

  # JUCE module implementation belongs in final executables (avoid duplicate compilation/linkage).
  add_library(tcb_osc INTERFACE)
  target_sources(tcb_osc INTERFACE "${CMAKE_CURRENT_SOURCE_DIR}/src/OscOutput.cpp")
  target_link_libraries(tcb_osc INTERFACE tcb_core juce::juce_osc)
  target_compile_definitions(tcb_osc INTERFACE JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0 JUCE_MODAL_LOOPS_PERMITTED=1)
  if(BUILD_TESTING AND TCB_BUILD_INTEGRATION_TESTS)
    juce_add_console_app(tcb_integration PRODUCT_NAME "TCB Integration Tests")
    target_sources(tcb_integration PRIVATE tests/test_integration.cpp src/LtcCapture.cpp)
    target_link_libraries(tcb_integration PRIVATE tcb_decode tcb_osc tcb_midi)
    add_test(NAME ltc_osc_integration COMMAND tcb_integration)
    set_tests_properties(ltc_osc_integration PROPERTIES TIMEOUT 60)
    juce_add_console_app(tcb_scene_tests PRODUCT_NAME "TCB Scene UI Tests")
    target_sources(tcb_scene_tests PRIVATE tests/test_scene_ui.cpp src/SceneEditor.cpp)
    target_link_libraries(tcb_scene_tests PRIVATE tcb_core juce::juce_gui_extra)
    target_compile_definitions(tcb_scene_tests PRIVATE JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0 JUCE_MODAL_LOOPS_PERMITTED=1)
    if(WIN32 OR APPLE)
      add_test(NAME scene_editor COMMAND tcb_scene_tests)
      set_tests_properties(scene_editor PROPERTIES TIMEOUT 30)
    endif()
  endif()
endif()
if(TCB_BUILD_GUI)
  juce_add_binary_data(TcbIcon NAMESPACE TcbAssets HEADER_NAME AppIconData.h SOURCES assets/app.png)
  juce_add_gui_app(TimecodeCueBridge ICON_BIG "${CMAKE_CURRENT_SOURCE_DIR}/assets/app.png" ICON_SMALL "${CMAKE_CURRENT_SOURCE_DIR}/assets/app.png" PRODUCT_NAME "Timecode Cue Bridge"
    COMPANY_NAME "Timecode Cue Bridge Project" BUNDLE_ID "org.timecodecuebridge.app"
    MICROPHONE_PERMISSION_ENABLED TRUE
    MICROPHONE_PERMISSION_TEXT "Timecode Cue Bridge needs audio input access to decode LTC."
    NEEDS_CURL FALSE NEEDS_WEB_BROWSER FALSE)
  target_sources(TimecodeCueBridge PRIVATE src/Main.cpp src/MainComponent.cpp src/LtcCapture.cpp src/SceneEditor.cpp)
  target_link_libraries(TimecodeCueBridge PRIVATE TcbIcon tcb_decode tcb_osc tcb_midi juce::juce_audio_utils juce::juce_gui_extra
    juce::juce_recommended_config_flags juce::juce_recommended_warning_flags)
  target_compile_definitions(TimecodeCueBridge PRIVATE
    JUCE_APPLICATION_NAME_STRING="$<TARGET_PROPERTY:TimecodeCueBridge,JUCE_PRODUCT_NAME>"
    JUCE_APPLICATION_VERSION_STRING="$<TARGET_PROPERTY:TimecodeCueBridge,JUCE_VERSION>")
  # macOS architecture remains controlled by CMAKE_OSX_ARCHITECTURES (arm64 supported).
endif()

# JUCE 8.0.12's resource rule depends on the icon but omits its Info.txt input.
# Track version metadata too, so incremental Windows builds refresh PE versions.
if(WIN32)
  foreach(_tcb_target IN ITEMS TimecodeCueBridge tcb_integration tcb_scene_tests)
    if(TARGET ${_tcb_target}_rc_lib)
      get_target_property(_tcb_resource_dir ${_tcb_target} JUCE_GENERATED_SOURCES_DIRECTORY)
      get_target_property(_tcb_info ${_tcb_target} JUCE_INFO_FILE)
      add_custom_command(OUTPUT "${_tcb_resource_dir}/${_tcb_target}_resources.rc"
        APPEND DEPENDS "${_tcb_info}" "${CMAKE_CURRENT_SOURCE_DIR}/CMakeLists.txt")
    endif()
  endforeach()
endif()
