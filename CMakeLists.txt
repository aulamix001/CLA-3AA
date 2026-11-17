cmake_minimum_required(VERSION 3.25)
project(CLA3AStudio VERSION 0.1.1 LANGUAGES CXX)
if(NOT WIN32 OR NOT CMAKE_SIZEOF_VOID_P EQUAL 8)
    message(FATAL_ERROR "This release target requires Windows x64. Native DSP tests can also be built directly with g++.")
endif()
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(SMTG_ENABLE_VSTGUI_SUPPORT OFF CACHE BOOL "" FORCE)
set(SMTG_ENABLE_VST3_PLUGIN_EXAMPLES OFF CACHE BOOL "" FORCE)
set(SMTG_ENABLE_VST3_HOSTING_EXAMPLES OFF CACHE BOOL "" FORCE)
set(SMTG_CREATE_PLUGIN_LINK OFF CACHE BOOL "" FORCE)
set(SMTG_RUN_VST_VALIDATOR ON CACHE BOOL "" FORCE)
set(SMTG_CREATE_BUNDLE_FOR_WINDOWS ON CACHE BOOL "" FORCE)
include(FetchContent)
FetchContent_Declare(vst3sdk
    GIT_REPOSITORY https://github.com/steinbergmedia/vst3sdk.git
    GIT_TAG 3cdf9ca5d1f5b1b21e0a86832aa4abe55607bd96
    GIT_SUBMODULES base cmake pluginterfaces public.sdk
    GIT_SUBMODULES_RECURSE OFF
)
FetchContent_MakeAvailable(vst3sdk)
# The SDK's plug-in packaging functions need these in the caller's scope.
set(SDK_ROOT "${vst3sdk_SOURCE_DIR}")
set(public_sdk_SOURCE_DIR "${SDK_ROOT}/public.sdk")
set(pluginterfaces_SOURCE_DIR "${SDK_ROOT}/pluginterfaces")
FetchContent_Declare(webview2
    URL https://api.nuget.org/v3-flatcontainer/microsoft.web.webview2/1.0.4314.35/microsoft.web.webview2.1.0.4314.35.nupkg
    URL_HASH SHA256=90778deb1b77e36292828b4b5833c9255c27533f40e0ddd55891ae718944edc4
    DOWNLOAD_EXTRACT_TIMESTAMP TRUE
)
FetchContent_MakeAvailable(webview2)
smtg_add_vst3plugin(CLA3AStudio PACKAGE_NAME "CLA-3A Studio" SOURCES_LIST
    native/PluginEntry.cpp native/PluginProcessor.cpp native/PluginProcessor.h
    native/PluginEditor.cpp native/PluginEditor.h native/Dsp.h native/Parameters.h
)
target_link_libraries(CLA3AStudio PRIVATE sdk
    "${webview2_SOURCE_DIR}/build/native/x64/WebView2LoaderStatic.lib"
    ole32 oleaut32 user32 shlwapi version)
target_include_directories(CLA3AStudio PRIVATE "${webview2_SOURCE_DIR}/build/native/include")
target_compile_definitions(CLA3AStudio PRIVATE NOMINMAX WIN32_LEAN_AND_MEAN UNICODE _UNICODE)
smtg_target_set_bundle(CLA3AStudio BUNDLE_IDENTIFIER "com.aulamix.cla3astudio" COMPANY_NAME "Aulamix")
find_program(NPM_EXECUTABLE NAMES npm.cmd npm REQUIRED)
execute_process(COMMAND "${NPM_EXECUTABLE}" run build -- --outDir "${CMAKE_BINARY_DIR}/web"
    WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}" COMMAND_ERROR_IS_FATAL ANY)
get_target_property(PLUGIN_PACKAGE CLA3AStudio SMTG_PLUGIN_PACKAGE_PATH)
add_custom_command(TARGET CLA3AStudio POST_BUILD
    COMMAND "${CMAKE_COMMAND}" -E copy_directory "${CMAKE_BINARY_DIR}/web" "${PLUGIN_PACKAGE}/Contents/Resources/web"
)
add_executable(native_dsp_tests tests/native-dsp.test.cpp)
target_compile_features(native_dsp_tests PRIVATE cxx_std_20)
enable_testing()
add_test(NAME native_dsp COMMAND native_dsp_tests)
