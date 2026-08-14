cmake_minimum_required(VERSION 3.24.0 FATAL_ERROR)

set(PLUGIN_NAME Resonarium)
set(DEV_CODE Soul)
set(PLUGIN_VERSION 0.1.0)
set(BUNDLE_ID com.gabrielsoule.resonarium)
set(AU_ID ResonariumAU)
set(LV2_URI "https://gabrielsoule.com/resonarium")
set(WEBSITE "https://gabrielsoule.com")
set(PLUGIN_CODE Res)

set(CMAKE_POLICY_DEFAULT_CMP0077 NEW)
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
set(CMAKE_SUPPRESS_REGENERATION true)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)
set_property(GLOBAL PROPERTY DEBUG_CONFIGURATIONS "Debug")

set(CMAKE_C_FLAGS_DEVELOPMENT ${CMAKE_C_FLAGS_RELEASE})
set(CMAKE_CXX_FLAGS_DEVELOPMENT ${CMAKE_CXX_FLAGS_RELEASE})

project(${PLUGIN_NAME} VERSION ${PLUGIN_VERSION} LANGUAGES CXX C HOMEPAGE_URL "https://gabrielsoule.com/")

# Resonarium must build against the exact, unmodified JUCE submodule recorded
# by this repository. This emits machine-readable evidence and rejects local
# vendor patches or an unrecorded JUCE checkout at configure time.
include("${CMAKE_CURRENT_SOURCE_DIR}/tests/cmake/RequirePristineJuce.cmake")

# pybind11's target helpers require the development components to be available
# before the subdirectory and any pybind11_add_module() call are evaluated.
# Discovering Python later only worked with a pre-populated CMake cache.
find_package(Python3 COMPONENTS Interpreter Development REQUIRED)

option(RESONARIUM_BUILD_TESTS "Build native unit tests" OFF)

find_package(CURL REQUIRED)

if (CMAKE_BUILD_TYPE)
    message(STATUS "Current build type: ${CMAKE_BUILD_TYPE}")
    message(STATUS "Current C++ flags: ${CMAKE_CXX_FLAGS} ${CMAKE_CXX_FLAGS_RELEASE}")
else ()
    message(STATUS "No build type selected")
endif ()

include(CMakeDependentOption)

set_property(DIRECTORY APPEND PROPERTY LABELS ${PLUGIN_NAME})
set_property(DIRECTORY APPEND PROPERTY LABELS gabrielsoule)
set_property(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR} PROPERTY VS_STARTUP_PROJECT ${PLUGIN_NAME}_Standalone)

set(CMAKE_OSX_DEPLOYMENT_TARGET 11.0)

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

#set(JUCE_MODULES_ONLY OFF)
set(JUCE_ENABLE_MODULE_SOURCE_GROUPS ON)
#set(JUCE_BUILD_EXTRAS OFF)
#set(JUCE_BUILD_EXAMPLES OFF)

add_subdirectory(modules/juce)
add_subdirectory(modules/melatonin_inspector)
add_subdirectory(modules/melatonin_perfetto)
add_subdirectory(modules/pybind11)
juce_add_module(modules/melatonin_blur)

set_property(DIRECTORY "${CMAKE_CURRENT_LIST_DIR}/modules/juce" APPEND PROPERTY LABELS JUCE)

foreach (module_name IN ITEMS gin gin_dsp gin_graphics gin_gui gin_metadata gin_network gin_plugin gin_webp gin_simd)
    juce_add_module("${CMAKE_CURRENT_LIST_DIR}/modules/gin/modules/${module_name}")
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

file(GLOB_RECURSE source_files CONFIGURE_DEPENDS
        ${CMAKE_CURRENT_SOURCE_DIR}/plugin/*.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/plugin/*.h)
list(REMOVE_ITEM source_files "${CMAKE_CURRENT_SOURCE_DIR}/plugin/Source/ResonariumPy.cpp")
source_group(TREE ${CMAKE_CURRENT_SOURCE_DIR}/plugin PREFIX Source FILES ${source_files})

juce_add_plugin(${PLUGIN_NAME}_Instrument
        PRODUCT_NAME ${PLUGIN_NAME}
        VERSION ${PLUGIN_VERSION}
        COMPANY_NAME "Soule DSP"
        COMPANY_WEBSITE ${WEBSITE}
        BUNDLE_ID ${BUNDLE_ID}.instrument
        FORMATS ${FORMATS}
        PLUGIN_MANUFACTURER_CODE ${DEV_CODE}
        PLUGIN_CODE ${PLUGIN_CODE}I
        IS_SYNTH ON
        MICROPHONE_PERMISSION_ENABLED ON
        NEEDS_MIDI_INPUT ON
        NEEDS_MIDI_OUTPUT OFF
        IS_MIDI_EFFECT OFF
        EDITOR_WANTS_KEYBOARD_FOCUS ON
        VST2_CATEGORY "kPlugCategUnknown"
        VST3_CATEGORIES Instrument Fx Synth
        AU_MAIN_TYPE "kAudioUnitType_MusicDevice"
        AU_EXPORT_PREFIX ${AU_ID}Ins
        AU_SANDBOX_SAFE FALSE
        LV2URI ${LV2_URI}
        COPY_PLUGIN_AFTER_BUILD TRUE
)

target_sources(${PLUGIN_NAME}_Instrument PRIVATE ${source_files})

target_link_libraries(${PLUGIN_NAME}_Instrument PRIVATE
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
        juce::juce_audio_basics
        juce::juce_audio_devices
        juce::juce_audio_formats
        juce::juce_audio_plugin_client
        juce::juce_audio_processors
        juce::juce_audio_utils
        juce::juce_core
        juce::juce_cryptography
        juce::juce_data_structures
        juce::juce_dsp
        juce::juce_events
        juce::juce_graphics
        juce::juce_gui_basics
        juce::juce_gui_extra
        juce::juce_recommended_config_flags
)

target_compile_definitions(${PLUGIN_NAME}_Instrument PRIVATE
        JUCE_DISPLAY_SPLASH_SCREEN=0
        JUCE_MODAL_LOOPS_PERMITTED=0
        JUCE_COREGRAPHICS_DRAW_ASYNC=1
        JUCE_VST3_CAN_REPLACE_VST2=0
        JUCE_WEB_BROWSER=0
        DONT_SET_USING_JUCE_NAMESPACE=1
        JUCE_SILENCE_XCODE_15_LINKER_WARNING=1
        RESONARIUM_IS_INSTRUMENT=1
)

juce_add_plugin(${PLUGIN_NAME}_Effect
        PRODUCT_NAME "${PLUGIN_NAME} Effect"
        VERSION ${PLUGIN_VERSION}
        COMPANY_NAME "Soule DSP"
        COMPANY_WEBSITE ${WEBSITE}
        BUNDLE_ID ${BUNDLE_ID}.effect
        FORMATS ${FORMATS}
        PLUGIN_MANUFACTURER_CODE ${DEV_CODE}
        PLUGIN_CODE ${PLUGIN_CODE}E
        IS_SYNTH OFF
        MICROPHONE_PERMISSION_ENABLED ON
        NEEDS_MIDI_INPUT ON
        NEEDS_MIDI_OUTPUT OFF
        IS_MIDI_EFFECT OFF
        EDITOR_WANTS_KEYBOARD_FOCUS ON
        VST2_CATEGORY "kPlugCategEffect"
        VST3_CATEGORIES Fx
        AU_MAIN_TYPE "kAudioUnitType_Effect"
        AU_EXPORT_PREFIX ${AU_ID}Fx
        AU_SANDBOX_SAFE FALSE
        LV2URI ${LV2_URI}/effect
        COPY_PLUGIN_AFTER_BUILD TRUE
)

target_sources(${PLUGIN_NAME}_Effect PRIVATE ${source_files})

target_link_libraries(${PLUGIN_NAME}_Effect PRIVATE
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
        juce::juce_audio_basics
        juce::juce_audio_devices
        juce::juce_audio_formats
        juce::juce_audio_plugin_client
        juce::juce_audio_processors
        juce::juce_audio_utils
        juce::juce_core
        juce::juce_cryptography
        juce::juce_data_structures
        juce::juce_dsp
        juce::juce_events
        juce::juce_graphics
        juce::juce_gui_basics
        juce::juce_gui_extra
        juce::juce_recommended_config_flags
)

target_compile_definitions(${PLUGIN_NAME}_Effect PRIVATE
        JUCE_DISPLAY_SPLASH_SCREEN=0
        JUCE_MODAL_LOOPS_PERMITTED=0
        JUCE_COREGRAPHICS_DRAW_ASYNC=1
        JUCE_VST3_CAN_REPLACE_VST2=0
        JUCE_WEB_BROWSER=0
        DONT_SET_USING_JUCE_NAMESPACE=1
        JUCE_SILENCE_XCODE_15_LINKER_WARNING=1
        RESONARIUM_IS_EFFECT=1
)

pybind11_add_module(${PLUGIN_NAME}_Python
        plugin/Source/ResonariumPy.cpp    # Your PyBind wrapper code
)

target_sources(${PLUGIN_NAME}_Python PRIVATE ${source_files})

target_link_libraries(${PLUGIN_NAME}_Python PRIVATE
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
        juce::juce_audio_basics
        juce::juce_audio_devices
        juce::juce_audio_formats
        juce::juce_audio_plugin_client
        juce::juce_audio_processors
        juce::juce_audio_utils
        juce::juce_core
        juce::juce_cryptography
        juce::juce_data_structures
        juce::juce_dsp
        juce::juce_events
        juce::juce_graphics
        juce::juce_gui_basics
        juce::juce_gui_extra
        juce::juce_recommended_config_flags
        CURL::libcurl
)

target_compile_definitions(${PLUGIN_NAME}_Python PRIVATE
        JUCE_DISPLAY_SPLASH_SCREEN=0
        JUCE_MODAL_LOOPS_PERMITTED=0
        JUCE_VST3_CAN_REPLACE_VST2=0
        JUCE_WEB_BROWSER=0
        DONT_SET_USING_JUCE_NAMESPACE=1
        RESONARIUM_IS_INSTRUMENT=1  # If you need this flag
)

target_include_directories(${PLUGIN_NAME}_Python PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/plugin
)

# Use a local directory in the build folder for the Python module
set(PYTHON_SITE_PACKAGES "${CMAKE_BINARY_DIR}/python_modules")
message(STATUS "Using local Python module directory: ${PYTHON_SITE_PACKAGES}")
# Create the directory if it doesn't exist
file(MAKE_DIRECTORY ${PYTHON_SITE_PACKAGES})

set_target_properties(${PLUGIN_NAME}_Python PROPERTIES
        OUTPUT_NAME "resonarium"  # This will be the import name in Python
        PREFIX ""                 # Remove lib prefix on Unix
        LIBRARY_OUTPUT_DIRECTORY "${PYTHON_SITE_PACKAGES}" # Copy to site-packages when built
        RUNTIME_OUTPUT_DIRECTORY "${PYTHON_SITE_PACKAGES}"
)

# Set the correct file extension based on the platform
if(WIN32)
    set_target_properties(${PLUGIN_NAME}_Python PROPERTIES SUFFIX ".pyd")
else()
    set_target_properties(${PLUGIN_NAME}_Python PROPERTIES SUFFIX ".so")
endif()

# Keep test-only modules below dependency configuration so enabling BUILD_TESTING
# does not opt vendored projects into their own test suites.
include(CTest)

if(BUILD_TESTING OR RESONARIUM_BUILD_TESTS)
    # Keep test-only DSP telemetry and control hooks out of the product-facing
    # Python extension. These modules remain opt-in build targets.
    function(resonarium_add_instrumented_python_module target_name output_name)
        pybind11_add_module(${target_name} EXCLUDE_FROM_ALL
                plugin/Source/ResonariumPy.cpp)
        target_sources(${target_name} PRIVATE ${source_files})
        target_link_libraries(${target_name} PRIVATE
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
                juce::juce_audio_basics
                juce::juce_audio_devices
                juce::juce_audio_formats
                juce::juce_audio_plugin_client
                juce::juce_audio_processors
                juce::juce_audio_utils
                juce::juce_core
                juce::juce_cryptography
                juce::juce_data_structures
                juce::juce_dsp
                juce::juce_events
                juce::juce_graphics
                juce::juce_gui_basics
                juce::juce_gui_extra
                juce::juce_recommended_config_flags
                CURL::libcurl)
        target_compile_definitions(${target_name} PRIVATE
                JUCE_DISPLAY_SPLASH_SCREEN=0
                JUCE_MODAL_LOOPS_PERMITTED=0
                JUCE_VST3_CAN_REPLACE_VST2=0
                JUCE_WEB_BROWSER=0
                DONT_SET_USING_JUCE_NAMESPACE=1
                RESONARIUM_TEST_INSTRUMENTATION=1
                RESONARIUM_IS_INSTRUMENT=1
                RESONARIUM_PYTHON_TEST_MODULE=1)
        target_include_directories(${target_name} PRIVATE
                ${CMAKE_CURRENT_SOURCE_DIR}/plugin)
        set_target_properties(${target_name} PROPERTIES
                OUTPUT_NAME "${output_name}"
                PREFIX ""
                LIBRARY_OUTPUT_DIRECTORY "${PYTHON_SITE_PACKAGES}"
                RUNTIME_OUTPUT_DIRECTORY "${PYTHON_SITE_PACKAGES}")
        if(WIN32)
            set_target_properties(${target_name} PROPERTIES SUFFIX ".pyd")
        else()
            set_target_properties(${target_name} PROPERTIES SUFFIX ".so")
        endif()
    endfunction()

    resonarium_add_instrumented_python_module(
            ${PLUGIN_NAME}_Python_Test resonarium_test)
endif()

if(BUILD_TESTING)
    set(RESONARIUM_PRESET_SWEEP_SCRIPT
            "${CMAKE_CURRENT_SOURCE_DIR}/tests/presets/preset_sweep.py")
    set(RESONARIUM_PRESET_SWEEP_OUTPUT
            "${CMAKE_CURRENT_BINARY_DIR}/preset-results/$<CONFIG>")

    # pybind11 records the interpreter used for the extension as Python_EXECUTABLE.
    # Fall back to the explicit Python3 discovery for older pybind11 configurations.
    if(Python_EXECUTABLE)
        set(RESONARIUM_TEST_PYTHON "${Python_EXECUTABLE}")
    else()
        set(RESONARIUM_TEST_PYTHON "${Python3_EXECUTABLE}")
    endif()

    add_test(NAME Resonarium.PresetSweepModel
            COMMAND "${RESONARIUM_TEST_PYTHON}"
                    -m unittest tests.presets.test_preset_sweep)
    set_tests_properties(Resonarium.PresetSweepModel PROPERTIES
            LABELS "resonarium;presets;python"
            TIMEOUT 60
            WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}")

    add_custom_target(${PLUGIN_NAME}_PresetSweep
            COMMAND "${RESONARIUM_TEST_PYTHON}"
                    "${RESONARIUM_PRESET_SWEEP_SCRIPT}"
                    --module "$<TARGET_FILE:${PLUGIN_NAME}_Python>"
                    --preset-dir "${CMAKE_CURRENT_SOURCE_DIR}/plugin/Resources/Presets"
                    --output "${RESONARIUM_PRESET_SWEEP_OUTPUT}"
            DEPENDS ${PLUGIN_NAME}_Python
            WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
            USES_TERMINAL
            VERBATIM)
endif()

if(RESONARIUM_BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()

if (APPLE)
    add_library(${PLUGIN_NAME}_AppleReleaseFlags INTERFACE)
    target_compile_options(${PLUGIN_NAME}_AppleReleaseFlags INTERFACE
            $<$<CONFIG:Release>:-O3 -ffast-math -fno-finite-math-only>
            $<$<CONFIG:RelWithDebInfo>:-O3 -ffast-math -fno-finite-math-only>
            $<$<CONFIG:MinSizeRel>:-O3 -ffast-math -fno-finite-math-only>
    )

    set(RESONARIUM_PLUGIN_TARGETS
            ${PLUGIN_NAME}_Instrument
            ${PLUGIN_NAME}_Effect
    )

    set(RESONARIUM_APPLE_TARGETS)
    foreach (plugin_target IN LISTS RESONARIUM_PLUGIN_TARGETS)
        list(APPEND RESONARIUM_APPLE_TARGETS
                ${plugin_target}
                ${plugin_target}_All
        )
        foreach (format IN LISTS FORMATS)
            list(APPEND RESONARIUM_APPLE_TARGETS ${plugin_target}_${format})
        endforeach ()
    endforeach ()

    foreach (tgt IN LISTS RESONARIUM_APPLE_TARGETS)
        if (TARGET ${tgt})
            set_target_properties(${tgt} PROPERTIES
                    XCODE_ATTRIBUTE_CLANG_LINK_OBJC_RUNTIME NO
                    XCODE_ATTRIBUTE_ONLY_ACTIVE_ARCH[variant=Debug] "YES"
            )
            if (NOT tgt MATCHES "_All$")
                target_compile_options(${tgt} PRIVATE
                        -Wall -Wstrict-aliasing -Wunused-parameter -Wconditional-uninitialized -Woverloaded-virtual -Wreorder -Wconstant-conversion -Wbool-conversion -Wextra-semi
                        -Wunreachable-code -Winconsistent-missing-destructor-override -Wshift-sign-overflow -Wnullable-to-nonnull-conversion -Wuninitialized -Wno-missing-field-initializers
                        -Wno-ignored-qualifiers -Wno-missing-braces -Wno-char-subscripts -Wno-unused-private-field -fno-aligned-allocation -Wunused-private-field -Wunreachable-code
                        -Wenum-compare -Wshadow -Wfloat-conversion -Wshadow-uncaptured-local -Wshadow-field -Wsign-compare -Wdeprecated-this-capture -Wimplicit-float-conversion -Wno-deprecated-declarations
                )
                target_link_libraries(${tgt} PRIVATE ${PLUGIN_NAME}_AppleReleaseFlags)
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
    target_link_libraries(${PLUGIN_NAME}_Instrument PRIVATE curl)
    target_link_libraries(${PLUGIN_NAME}_Effect PRIVATE curl)
endif ()

if (WIN32)
    set(dest "Program Files")
else ()
    set(dest "Applications")
endif ()

install(TARGETS ${PLUGIN_NAME}_Instrument DESTINATION "${dest}")
install(TARGETS ${PLUGIN_NAME}_Effect DESTINATION "${dest}")
