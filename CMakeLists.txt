cmake_minimum_required(VERSION 3.22)

project(MIMIC VERSION 1.0.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>") # no VC++ redist needed

include(FetchContent)

# ---------------------------------------------------------------- JUCE 9
# Downloaded automatically (no git needed). Point JUCE_DIR at a local copy to skip the download.
set(JUCE_DIR "" CACHE PATH "Optional path to a local JUCE checkout")
if(JUCE_DIR)
    add_subdirectory(${JUCE_DIR} ${CMAKE_BINARY_DIR}/JUCE)
    set(juce_SOURCE_DIR ${JUCE_DIR})
else()
    FetchContent_Declare(juce
        URL https://github.com/juce-framework/JUCE/archive/refs/tags/9.0.4.zip
        DOWNLOAD_EXTRACT_TIMESTAMP TRUE)
    FetchContent_MakeAvailable(juce)
endif()

# ---------------------------------------------------------------- WebView2 SDK (Windows)
# JUCE links WebView2 statically so users need nothing but the Edge WebView2 runtime
# (built into Windows 10/11). The NuGet package is fetched here so no PowerShell setup is needed.
if(WIN32)
    set(MIMIC_WEBVIEW2_VERSION 1.0.3485.44)
    set(MIMIC_NUGET_DIR ${CMAKE_BINARY_DIR}/nuget)
    set(MIMIC_WEBVIEW2_DIR ${MIMIC_NUGET_DIR}/Microsoft.Web.WebView2.${MIMIC_WEBVIEW2_VERSION})
    if(NOT EXISTS ${MIMIC_WEBVIEW2_DIR}/build/native/include/WebView2.h)
        message(STATUS "Downloading WebView2 SDK ${MIMIC_WEBVIEW2_VERSION}...")
        file(DOWNLOAD
            https://www.nuget.org/api/v2/package/Microsoft.Web.WebView2/${MIMIC_WEBVIEW2_VERSION}
            ${MIMIC_NUGET_DIR}/webview2.nupkg
            STATUS dl_status SHOW_PROGRESS)
        list(GET dl_status 0 dl_code)
        if(NOT dl_code EQUAL 0)
            message(FATAL_ERROR "WebView2 SDK download failed: ${dl_status}")
        endif()
        file(ARCHIVE_EXTRACT INPUT ${MIMIC_NUGET_DIR}/webview2.nupkg DESTINATION ${MIMIC_WEBVIEW2_DIR})
    endif()
    set(JUCE_WEBVIEW2_PACKAGE_LOCATION ${MIMIC_NUGET_DIR} CACHE PATH "" FORCE)
endif()

# ---------------------------------------------------------------- Plugin
juce_add_plugin(MIMIC
    COMPANY_NAME "DSKY"
    PLUGIN_MANUFACTURER_CODE Dsky
    PLUGIN_CODE Mimc
    PRODUCT_NAME "MIMIC"
    DESCRIPTION "Artist vocal chains"
    FORMATS VST3 Standalone
    IS_SYNTH FALSE
    NEEDS_MIDI_INPUT FALSE
    NEEDS_MIDI_OUTPUT FALSE
    IS_MIDI_EFFECT FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS TRUE
    VST3_CATEGORIES Fx Vocals
    NEEDS_WEB_BROWSER TRUE
    NEEDS_WEBVIEW2 TRUE
    COPY_PLUGIN_AFTER_BUILD FALSE)


target_sources(MIMIC PRIVATE
    Source/PluginProcessor.cpp
    Source/PluginEditor.cpp)

target_include_directories(MIMIC PRIVATE Source)

# UI files are embedded in the binary and served to the WebView from memory.
juce_add_binary_data(MIMIC_UI
    NAMESPACE MimicUI
    SOURCES
        ui/index.html
        ui/style.css
        ui/app.js
        ${juce_SOURCE_DIR}/modules/juce_gui_extra/native/typescript/webview-interop/dist/index.js)

target_compile_definitions(MIMIC PUBLIC
    JUCE_WEB_BROWSER=1
    JUCE_USE_WIN_WEBVIEW2_WITH_STATIC_LINKING=1
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0
    JUCE_DISPLAY_SPLASH_SCREEN=0)

target_link_libraries(MIMIC
    PRIVATE
        MIMIC_UI
        juce::juce_audio_utils
        juce::juce_dsp
        juce::juce_gui_extra
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags)

if(MSVC)
    target_compile_options(MIMIC PRIVATE /fp:fast /O2)
endif()
