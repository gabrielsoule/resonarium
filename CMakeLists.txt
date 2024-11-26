cmake_minimum_required(VERSION 3.24.0 FATAL_ERROR)

#
# Set for each plugin
#
set(PLUGIN_NAME Resonarium)
set(DEV_CODE Soul)
set(PLUGIN_VERSION 0.0.4)
set(BUNDLE_ID com.gabrielsoule.resonarium)
set(AU_ID ResonariumAU)
set(LV2_URI "https://gabrielsoule.com/resonarium")
set(WEBSITE "https://gabrielsoule.com")
set(PLUGIN_CODE Res0)

set(CMAKE_POLICY_DEFAULT_CMP0077 NEW)
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
set(CMAKE_SUPPRESS_REGENERATION true)
set(CMAKE_SKIP_INSTALL_RULES YES)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)
set_property(GLOBAL PROPERTY DEBUG_CONFIGURATIONS "Debug")

set(CMAKE_C_FLAGS_DEVELOPMENT ${CMAKE_C_FLAGS_RELEASE})
set(CMAKE_CXX_FLAGS_DEVELOPMENT ${CMAKE_CXX_FLAGS_RELEASE})

project(${PLUGIN_NAME} VERSION ${PLUGIN_VERSION} LANGUAGES CXX C HOMEPAGE_URL "https://gabrielsoule.com/")

if(CMAKE_BUILD_TYPE)
    message(STATUS "Current build type: ${CMAKE_BUILD_TYPE}")
    message(STATUS "Current C++ flags: ${CMAKE_CXX_FLAGS} ${CMAKE_CXX_FLAGS_RELEASE}")
else()
    message(STATUS "No build type selected")
endif()

include(CMakeDependentOption)

set_property(DIRECTORY APPEND PROPERTY LABELS ${PLUGIN_NAME})

set_property(DIRECTORY APPEND PROPERTY LABELS gabrielsoule)
set_property(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR} PROPERTY VS_STARTUP_PROJECT ${PLUGIN_NAME}_Standalone)

set(CMAKE_OSX_DEPLOYMENT_TARGET 10.13)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(CMAKE_ERROR_ON_ABSOLUTE_INSTALL_DESTINATION ON)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_OBJCXX_STANDARD 20)
set(CMAKE_OBJCXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_VISIBILITY_PRESET hidden)
set(CMAKE_VISIBILITY_INLINES_HIDDEN ON)
set(CMAKE_MINSIZEREL_POSTFIX -rm)
set(CMAKE_RELWITHDEBINFO_POSTFIX -rd)
set(CMAKE_OPTIMIZE_DEPENDENCIES OFF)

set(BUILD_SHARED_LIBS OFF)

if (APPLE)
    set(CMAKE_OSX_ARCHITECTURES arm64 x86_64)
    set(FORMATS Standalone VST3 AU LV2)
else ()
    set(FORMATS Standalone VST3 LV2)
endif ()

set_property(GLOBAL PROPERTY USE_FOLDERS YES)
set_property(GLOBAL PROPERTY PREDEFINED_TARGETS_FOLDER utility)
set_property(GLOBAL PROPERTY REPORT_UNDEFINED_PROPERTIES "${CMAKE_BINARY_DIR}/undefined_properties.log")
set_property(GLOBAL PROPERTY JUCE_COPY_PLUGIN_AFTER_BUILD YES)

set_property(DIRECTORY APPEND PROPERTY LABELS External)

# JUCE

set(JUCE_MODULES_ONLY OFF)
set(JUCE_ENABLE_MODULE_SOURCE_GROUPS ON)
set(JUCE_BUILD_EXTRAS OFF)
set(JUCE_BUILD_EXAMPLES OFF)

add_subdirectory(modules/juce)
add_subdirectory(modules/melatonin_inspector)
add_subdirectory(modules/chowdsp_utils)
add_subdirectory(modules/melatonin_perfetto)
juce_add_module(modules/melatonin_blur)

set_property(DIRECTORY "${CMAKE_CURRENT_LIST_DIR}/modules/juce" APPEND PROPERTY LABELS JUCE)

foreach (module_name IN ITEMS gin gin_dsp gin_graphics gin_gui gin_metadata gin_network gin_plugin gin_webp gin_simd)
    juce_add_module(
            "${CMAKE_CURRENT_LIST_DIR}/modules/gin/modules/${module_name}"
    )

    set_property(TARGET "${module_name}" APPEND PROPERTY LABELS Gin)
endforeach ()

set_property(DIRECTORY APPEND PROPERTY LABELS Assets)

file(GLOB_RECURSE binary_files CONFIGURE_DEPENDS
        "./plugin/Resources/resonarium_logo.png"
        "./plugin/Resources/resonarium_logo_svg.svg"
        "./plugin/Resources/*.svg"
        "./plugin/Resources/tooltips.json"
        "./plugin/Resources/Presets/*.xml"
        "./plugin/Resources/Fonts/*.otf")

juce_add_binary_data(${PLUGIN_NAME}_Assets SOURCES ${binary_files})

set_target_properties(${PLUGIN_NAME}_Assets PROPERTIES UNITY_BUILD ON UNITY_BUILD_MODE BATCH UNITY_BUILD_BATCH_SIZE 10)

juce_add_plugin(${PLUGIN_NAME}
        PRODUCT_NAME ${PLUGIN_NAME}
        VERSION ${PLUGIN_VERSION}
        COMPANY_NAME "Soule DSP"
        COMPANY_WEBSITE ${WEBSITE}
        BUNDLE_ID ${BUNDLE_ID}
        FORMATS ${FORMATS}
        PLUGIN_MANUFACTURER_CODE ${DEV_CODE}
        PLUGIN_CODE ${PLUGIN_CODE}
        IS_SYNTH ON
        NEEDS_MIDI_INPUT ON
        NEEDS_MIDI_OUTPUT OFF
        IS_MIDI_EFFECT OFF
        EDITOR_WANTS_KEYBOARD_FOCUS ON
        VST2_CATEGORY "kPlugCategUnknown"
        VST3_CATEGORIES Instrument Fx Synth
        AU_MAIN_TYPE "kAudioUnitType_Effect"
        AU_EXPORT_PREFIX ${AU_ID}
        AU_SANDBOX_SAFE FALSE
        LV2URI ${LV2_URI}
        COPY_PLUGIN_AFTER_BUILD TRUE)

file(GLOB_RECURSE source_files CONFIGURE_DEPENDS
        ${CMAKE_CURRENT_SOURCE_DIR}/plugin/*.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/plugin/*.h)

target_sources(${PLUGIN_NAME} PRIVATE ${source_files})
source_group(TREE ${CMAKE_CURRENT_SOURCE_DIR}/plugin PREFIX Source FILES ${source_files})

target_link_libraries(${PLUGIN_NAME} PRIVATE
        ${PLUGIN_NAME}_Assets
        gin
        gin_dsp
        gin_graphics
        gin_gui
        gin_plugin
        gin_simd

        Melatonin::Perfetto
        melatonin_perfetto

        melatonin_inspector

        melatonin_blur


        chowdsp::chowdsp_dsp_utils
        chowdsp::chowdsp_gui
        chowdsp::chowdsp_plugin_base
        chowdsp::chowdsp_filters

        juce::juce_audio_basics
        juce::juce_audio_devices
        juce::juce_audio_formats
        juce::juce_audio_plugin_client
        juce::juce_audio_processors
        juce::juce_audio_utils
        juce::juce_core
        juce::juce_cryptography
        juce::juce_data_structures
        juce::juce_events
        juce::juce_graphics
        juce::juce_gui_basics
        juce::juce_gui_extra
        juce::juce_recommended_config_flags
)

juce_generate_juce_header(${PLUGIN_NAME})

target_compile_definitions(${PLUGIN_NAME} PRIVATE
        JUCE_DISPLAY_SPLASH_SCREEN=0
        JUCE_MODAL_LOOPS_PERMITTED=0
        JUCE_COREGRAPHICS_DRAW_ASYNC=1
        JUCE_VST3_CAN_REPLACE_VST2=0
        JUCE_WEB_BROWSER=0
        DONT_SET_USING_JUCE_NAMESPACE=1
        JUCE_SILENCE_XCODE_15_LINKER_WARNING=1
)

if (APPLE)
    set_target_properties("juce_vst3_helper" PROPERTIES XCODE_ATTRIBUTE_CLANG_LINK_OBJC_RUNTIME NO)

    foreach (t ${FORMATS} "All" "")
        set(tgt ${CMAKE_PROJECT_NAME})
        if (NOT t STREQUAL "")
            set(tgt ${tgt}_${t})
        endif ()
        if (TARGET ${tgt})
            set_target_properties(${tgt} PROPERTIES
                    XCODE_ATTRIBUTE_CLANG_LINK_OBJC_RUNTIME NO
                    #XCODE_ATTRIBUTE_DEPLOYMENT_POSTPROCESSING[variant=Release] YES
                    XCODE_ATTRIBUTE_ONLY_ACTIVE_ARCH[variant=Debug] "YES"
            )
            if (NOT t STREQUAL "All")
                target_compile_options(${tgt} PRIVATE
                        -Wall -Wstrict-aliasing -Wunused-parameter -Wconditional-uninitialized -Woverloaded-virtual -Wreorder -Wconstant-conversion -Wbool-conversion -Wextra-semi
                        -Wunreachable-code -Winconsistent-missing-destructor-override -Wshift-sign-overflow -Wnullable-to-nonnull-conversion -Wuninitialized -Wno-missing-field-initializers
                        -Wno-ignored-qualifiers -Wno-missing-braces -Wno-char-subscripts -Wno-unused-private-field -fno-aligned-allocation -Wunused-private-field -Wunreachable-code
                        -Wenum-compare -Wshadow -Wfloat-conversion -Wshadow-uncaptured-local -Wshadow-field -Wsign-compare -Wdeprecated-this-capture -Wimplicit-float-conversion -Wno-deprecated-declarations
                        -ffast-math -fno-finite-math-only -Ofast)
            endif ()
        endif ()
    endforeach ()
endif ()

if (WIN32)
    foreach (t ${FORMATS} "All" "")
        set(tgt ${CMAKE_PROJECT_NAME})
        if (NOT t STREQUAL "")
            set(tgt ${tgt}_${t})
        endif ()
        if (TARGET ${tgt})
            set_property(TARGET ${tgt} APPEND_STRING PROPERTY LINK_FLAGS_DEBUG " /INCREMENTAL:NO")
            set_target_properties(${tgt} PROPERTIES LINK_FLAGS "/ignore:4099")
        endif ()
    endforeach ()
endif ()

if (UNIX AND NOT APPLE)
    target_link_libraries(${PLUGIN_NAME} PRIVATE curl)
endif ()


if (WIN32)
    set(dest "Program Files")
else ()
    set(dest "Applications")
endif ()

install(TARGETS ${PLUGIN_NAME} DESTINATION "${dest}")
