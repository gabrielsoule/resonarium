include(FetchContent)

# Keep the native test stack aligned with Applause.
FetchContent_Declare(Catch2
        GIT_REPOSITORY https://github.com/catchorg/Catch2.git
        GIT_TAG v3.7.1
        GIT_SHALLOW TRUE)
FetchContent_MakeAvailable(Catch2)
include("${catch2_SOURCE_DIR}/extras/Catch.cmake")

set(GIN_VOICE_UNIT_TEST_RUNNER
        "${CMAKE_CURRENT_SOURCE_DIR}/gin/GinVoiceUnitTestRunner.cpp")
set(RESONARIUM_ALLOCATION_PROBE
        "${CMAKE_CURRENT_SOURCE_DIR}/performance/AllocationProbe.cpp")
set(EDITOR_LIFECYCLE_TEST
        "${CMAKE_CURRENT_SOURCE_DIR}/ui/EditorLifecycleTest.cpp")

set(RESONARIUM_LINUX_ALLOCATION_WRAP_OPTIONS
        "-Wl,--wrap=malloc"
        "-Wl,--wrap=calloc"
        "-Wl,--wrap=realloc"
        "-Wl,--wrap=free"
        "-Wl,--wrap=aligned_alloc"
        "-Wl,--wrap=posix_memalign"
        "-Wl,--wrap=valloc"
        "-Wl,--wrap=memalign"
        "-Wl,--wrap=strdup"
        "-Wl,--wrap=strndup")

if(APPLE)
    set(RESONARIUM_ALLOCATION_PROBE_SOURCE
            "${CMAKE_CURRENT_SOURCE_DIR}/performance/AllocationProbe.mm")
    set(RESONARIUM_RT_ALLOCATION_GATE_SOURCE
            "${CMAKE_CURRENT_SOURCE_DIR}/rt/ProcessorAllocationGate.mm")
elseif(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    set(RESONARIUM_ALLOCATION_PROBE_SOURCE
            "${CMAKE_CURRENT_SOURCE_DIR}/performance/AllocationProbeLinux.cc")
    set(RESONARIUM_RT_ALLOCATION_GATE_SOURCE
            "${CMAKE_CURRENT_SOURCE_DIR}/rt/ProcessorAllocationGatePortable.cc")
elseif(WIN32)
    set(RESONARIUM_ALLOCATION_PROBE_SOURCE
            "${CMAKE_CURRENT_SOURCE_DIR}/performance/AllocationProbeWindows.cc")
    set(RESONARIUM_RT_ALLOCATION_GATE_SOURCE
            "${CMAKE_CURRENT_SOURCE_DIR}/rt/ProcessorAllocationGatePortable.cc")
else()
    set(RESONARIUM_ALLOCATION_PROBE_SOURCE "${RESONARIUM_ALLOCATION_PROBE}")
endif()

file(GLOB_RECURSE RESONARIUM_TEST_SOURCES CONFIGURE_DEPENDS
        "${CMAKE_CURRENT_SOURCE_DIR}/*.cpp")
list(REMOVE_ITEM RESONARIUM_TEST_SOURCES "${GIN_VOICE_UNIT_TEST_RUNNER}")
list(REMOVE_ITEM RESONARIUM_TEST_SOURCES
        "${RESONARIUM_ALLOCATION_PROBE}"
        "${EDITOR_LIFECYCLE_TEST}")

juce_add_console_app(resonarium_tests
        PRODUCT_NAME "Resonarium Tests")

target_sources(resonarium_tests PRIVATE
        ${RESONARIUM_TEST_SOURCES}
        "${PROJECT_SOURCE_DIR}/plugin/Source/dsp/MultiDelay.cpp"
        "${PROJECT_SOURCE_DIR}/plugin/Source/dsp/Sampler.cpp"
        "${PROJECT_SOURCE_DIR}/plugin/Source/dsp/WaveguideKernel.cpp"
        "${PROJECT_SOURCE_DIR}/plugin/Source/util/MSEGDataStore.cpp")

target_include_directories(resonarium_tests PRIVATE
        "${PROJECT_SOURCE_DIR}/plugin/Source")

target_link_libraries(resonarium_tests PRIVATE
        Catch2::Catch2WithMain
        gin_dsp
        juce::juce_audio_basics
        juce::juce_audio_processors
        juce::juce_dsp
        juce::juce_recommended_config_flags)

target_compile_definitions(resonarium_tests PRIVATE
        JUCE_WEB_BROWSER=0
        JUCE_USE_CURL=0)

set_target_properties(resonarium_tests PROPERTIES FOLDER Tests)

catch_discover_tests(resonarium_tests
        PROPERTIES LABELS native)

# The fixed voice manager and modulation matrix use JUCE's UnitTest API in the
# Gin fork. Keep their runner separate from the Catch executable so the two
# test frameworks retain independent entry points.
juce_add_console_app(resonarium_gin_voice_tests
        PRODUCT_NAME "Resonarium Gin Voice Tests")

target_sources(resonarium_gin_voice_tests PRIVATE
        "${GIN_VOICE_UNIT_TEST_RUNNER}")

target_link_libraries(resonarium_gin_voice_tests PRIVATE
        gin_dsp
        gin_plugin
        juce::juce_audio_basics
        juce::juce_audio_processors
        juce::juce_dsp
        juce::juce_recommended_config_flags)

target_compile_definitions(resonarium_gin_voice_tests PRIVATE
        GIN_UNIT_TESTS=1
        JUCE_WEB_BROWSER=0
        JUCE_USE_CURL=0)

set_target_properties(resonarium_gin_voice_tests PROPERTIES FOLDER Tests)

add_test(NAME Resonarium.GinVoiceManager
        COMMAND "$<TARGET_FILE:resonarium_gin_voice_tests>")
set_tests_properties(Resonarium.GinVoiceManager PROPERTIES
        LABELS "resonarium;native;gin;voice-lifecycle"
        TIMEOUT 120)

if(RESONARIUM_RT_ALLOCATION_GATE_SOURCE)
    # These are standalone executables rather than Catch cases: allocator
    # interposition must be linked against the exact Release shared-code
    # archives exercised by instrument and effect plugin hosts.
    function(add_resonarium_rt_allocation_gate target_name plugin_target test_name variant_definition)
        add_executable(${target_name})
        target_sources(${target_name} PRIVATE
                "${RESONARIUM_RT_ALLOCATION_GATE_SOURCE}")
        if(NOT APPLE)
            target_sources(${target_name} PRIVATE
                    "${RESONARIUM_ALLOCATION_PROBE_SOURCE}")
        endif()

        target_include_directories(${target_name} PRIVATE
            "${PROJECT_SOURCE_DIR}/plugin/Source"
            "${PROJECT_SOURCE_DIR}/modules/gin/modules"
            "${PROJECT_SOURCE_DIR}/modules/juce/modules"
            "${PROJECT_SOURCE_DIR}/modules/melatonin_perfetto"
            "${PROJECT_BINARY_DIR}/_deps/perfetto-src/sdk")

        target_link_libraries(${target_name} PRIVATE ${plugin_target})

        target_compile_definitions(${target_name} PRIVATE
            DONT_SET_USING_JUCE_NAMESPACE=1
            JUCE_DISPLAY_SPLASH_SCREEN=0
            JUCE_GLOBAL_MODULE_SETTINGS_INCLUDED=1
            JUCE_MODULE_AVAILABLE_gin=1
            JUCE_MODULE_AVAILABLE_gin_dsp=1
            JUCE_MODULE_AVAILABLE_gin_graphics=1
            JUCE_MODULE_AVAILABLE_gin_gui=1
            JUCE_MODULE_AVAILABLE_gin_plugin=1
            JUCE_MODULE_AVAILABLE_gin_simd=1
            JUCE_MODULE_AVAILABLE_juce_audio_basics=1
            JUCE_MODULE_AVAILABLE_juce_audio_devices=1
            JUCE_MODULE_AVAILABLE_juce_audio_formats=1
            JUCE_MODULE_AVAILABLE_juce_audio_processors=1
            JUCE_MODULE_AVAILABLE_juce_audio_processors_headless=1
            JUCE_MODULE_AVAILABLE_juce_audio_utils=1
            JUCE_MODULE_AVAILABLE_juce_core=1
            JUCE_MODULE_AVAILABLE_juce_cryptography=1
            JUCE_MODULE_AVAILABLE_juce_data_structures=1
            JUCE_MODULE_AVAILABLE_juce_dsp=1
            JUCE_MODULE_AVAILABLE_juce_events=1
            JUCE_MODULE_AVAILABLE_juce_graphics=1
            JUCE_MODULE_AVAILABLE_juce_gui_basics=1
            JUCE_MODULE_AVAILABLE_juce_gui_extra=1
            JUCE_USE_CURL=0
            JUCE_WEB_BROWSER=0
            ${variant_definition}=1)

        if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
            target_link_options(${target_name} PRIVATE
                    ${RESONARIUM_LINUX_ALLOCATION_WRAP_OPTIONS})
        elseif(WIN32)
            target_link_libraries(${target_name} PRIVATE psapi)
        endif()

        set_target_properties(${target_name} PROPERTIES FOLDER Tests)
        add_test(NAME ${test_name}
                COMMAND "$<TARGET_FILE:${target_name}>"
                CONFIGURATIONS Release)
        set_tests_properties(${test_name} PROPERTIES
            LABELS "resonarium;native;realtime;performance"
            RUN_SERIAL TRUE
            TIMEOUT 1200)
    endfunction()

    add_resonarium_rt_allocation_gate(
            resonarium_rt_allocation_gate
            Resonarium_Instrument
            Resonarium.RTAllocationGate
            RESONARIUM_IS_INSTRUMENT)
    add_resonarium_rt_allocation_gate(
            resonarium_effect_rt_allocation_gate
            Resonarium_Effect
            Resonarium.EffectRTAllocationGate
            RESONARIUM_IS_EFFECT)
endif()

add_executable(resonarium_effect_external_input_gate
        "${CMAKE_CURRENT_SOURCE_DIR}/rt/EffectExternalInputGate.cc")
target_include_directories(resonarium_effect_external_input_gate PRIVATE
        "${PROJECT_SOURCE_DIR}/plugin/Source"
        "${PROJECT_SOURCE_DIR}/modules/gin/modules"
        "${PROJECT_SOURCE_DIR}/modules/juce/modules"
        "${PROJECT_SOURCE_DIR}/modules/melatonin_perfetto"
        "${PROJECT_BINARY_DIR}/_deps/perfetto-src/sdk")
target_link_libraries(resonarium_effect_external_input_gate PRIVATE Resonarium_Effect)
target_compile_definitions(resonarium_effect_external_input_gate PRIVATE
        DONT_SET_USING_JUCE_NAMESPACE=1
        JUCE_DISPLAY_SPLASH_SCREEN=0
        JUCE_GLOBAL_MODULE_SETTINGS_INCLUDED=1
        JUCE_MODULE_AVAILABLE_gin=1
        JUCE_MODULE_AVAILABLE_gin_dsp=1
        JUCE_MODULE_AVAILABLE_gin_graphics=1
        JUCE_MODULE_AVAILABLE_gin_gui=1
        JUCE_MODULE_AVAILABLE_gin_plugin=1
        JUCE_MODULE_AVAILABLE_gin_simd=1
        JUCE_MODULE_AVAILABLE_juce_audio_basics=1
        JUCE_MODULE_AVAILABLE_juce_audio_devices=1
        JUCE_MODULE_AVAILABLE_juce_audio_formats=1
        JUCE_MODULE_AVAILABLE_juce_audio_processors=1
        JUCE_MODULE_AVAILABLE_juce_audio_processors_headless=1
        JUCE_MODULE_AVAILABLE_juce_audio_utils=1
        JUCE_MODULE_AVAILABLE_juce_core=1
        JUCE_MODULE_AVAILABLE_juce_cryptography=1
        JUCE_MODULE_AVAILABLE_juce_data_structures=1
        JUCE_MODULE_AVAILABLE_juce_dsp=1
        JUCE_MODULE_AVAILABLE_juce_events=1
        JUCE_MODULE_AVAILABLE_juce_graphics=1
        JUCE_MODULE_AVAILABLE_juce_gui_basics=1
        JUCE_MODULE_AVAILABLE_juce_gui_extra=1
        JUCE_USE_CURL=0
        JUCE_WEB_BROWSER=0
        RESONARIUM_IS_EFFECT=1)
set_target_properties(resonarium_effect_external_input_gate PROPERTIES FOLDER Tests)
add_test(NAME Resonarium.EffectExternalInput
        COMMAND "$<TARGET_FILE:resonarium_effect_external_input_gate>"
        CONFIGURATIONS Release)
set_tests_properties(Resonarium.EffectExternalInput PROPERTIES
        LABELS "resonarium;native;effect;audio;exciter"
        RUN_SERIAL TRUE
        TIMEOUT 120)

add_executable(resonarium_instrument_no_input_gate
        "${CMAKE_CURRENT_SOURCE_DIR}/rt/InstrumentNoInputGate.cc")
target_include_directories(resonarium_instrument_no_input_gate PRIVATE
        "${PROJECT_SOURCE_DIR}/plugin/Source"
        "${PROJECT_SOURCE_DIR}/modules/gin/modules"
        "${PROJECT_SOURCE_DIR}/modules/juce/modules"
        "${PROJECT_SOURCE_DIR}/modules/melatonin_perfetto"
        "${PROJECT_BINARY_DIR}/_deps/perfetto-src/sdk")
target_link_libraries(resonarium_instrument_no_input_gate PRIVATE Resonarium_Instrument)
target_compile_definitions(resonarium_instrument_no_input_gate PRIVATE
        DONT_SET_USING_JUCE_NAMESPACE=1
        JUCE_DISPLAY_SPLASH_SCREEN=0
        JUCE_GLOBAL_MODULE_SETTINGS_INCLUDED=1
        JUCE_MODULE_AVAILABLE_gin=1
        JUCE_MODULE_AVAILABLE_gin_dsp=1
        JUCE_MODULE_AVAILABLE_gin_graphics=1
        JUCE_MODULE_AVAILABLE_gin_gui=1
        JUCE_MODULE_AVAILABLE_gin_plugin=1
        JUCE_MODULE_AVAILABLE_gin_simd=1
        JUCE_MODULE_AVAILABLE_juce_audio_basics=1
        JUCE_MODULE_AVAILABLE_juce_audio_devices=1
        JUCE_MODULE_AVAILABLE_juce_audio_formats=1
        JUCE_MODULE_AVAILABLE_juce_audio_processors=1
        JUCE_MODULE_AVAILABLE_juce_audio_processors_headless=1
        JUCE_MODULE_AVAILABLE_juce_audio_utils=1
        JUCE_MODULE_AVAILABLE_juce_core=1
        JUCE_MODULE_AVAILABLE_juce_cryptography=1
        JUCE_MODULE_AVAILABLE_juce_data_structures=1
        JUCE_MODULE_AVAILABLE_juce_dsp=1
        JUCE_MODULE_AVAILABLE_juce_events=1
        JUCE_MODULE_AVAILABLE_juce_graphics=1
        JUCE_MODULE_AVAILABLE_juce_gui_basics=1
        JUCE_MODULE_AVAILABLE_juce_gui_extra=1
        JUCE_USE_CURL=0
        JUCE_WEB_BROWSER=0
        RESONARIUM_IS_INSTRUMENT=1)
set_target_properties(resonarium_instrument_no_input_gate PROPERTIES FOLDER Tests)
add_test(NAME Resonarium.InstrumentNoInput
        COMMAND "$<TARGET_FILE:resonarium_instrument_no_input_gate>"
        CONFIGURATIONS Release)
set_tests_properties(Resonarium.InstrumentNoInput PROPERTIES
        LABELS "resonarium;native;instrument;audio;realtime"
        RUN_SERIAL TRUE
        TIMEOUT 120)

# Exercise the editor lifecycle through the same instrument shared-code archive
# used by plugin hosts. This owns main(), so it stays separate from the Catch
# source glob above. The test gives each editor a temporary desktop peer so its
# shared animation scheduler and visual-consumer gate run. Linux uses Xvfb when
# available and otherwise reports an ordinary CTest skip without a display.
add_executable(resonarium_editor_lifecycle_test
        "${EDITOR_LIFECYCLE_TEST}")
target_include_directories(resonarium_editor_lifecycle_test PRIVATE
        "${PROJECT_SOURCE_DIR}/plugin/Source"
        "${PROJECT_SOURCE_DIR}/modules/gin/modules"
        "${PROJECT_SOURCE_DIR}/modules/juce/modules"
        "${PROJECT_SOURCE_DIR}/modules/melatonin_perfetto"
        "${PROJECT_BINARY_DIR}/_deps/perfetto-src/sdk")
target_link_libraries(resonarium_editor_lifecycle_test PRIVATE
        Resonarium_Instrument)
target_compile_definitions(resonarium_editor_lifecycle_test PRIVATE
        DONT_SET_USING_JUCE_NAMESPACE=1
        JUCE_DISPLAY_SPLASH_SCREEN=0
        JUCE_GLOBAL_MODULE_SETTINGS_INCLUDED=1
        JUCE_MODULE_AVAILABLE_gin=1
        JUCE_MODULE_AVAILABLE_gin_dsp=1
        JUCE_MODULE_AVAILABLE_gin_graphics=1
        JUCE_MODULE_AVAILABLE_gin_gui=1
        JUCE_MODULE_AVAILABLE_gin_plugin=1
        JUCE_MODULE_AVAILABLE_gin_simd=1
        JUCE_MODULE_AVAILABLE_juce_audio_basics=1
        JUCE_MODULE_AVAILABLE_juce_audio_devices=1
        JUCE_MODULE_AVAILABLE_juce_audio_formats=1
        JUCE_MODULE_AVAILABLE_juce_audio_processors=1
        JUCE_MODULE_AVAILABLE_juce_audio_processors_headless=1
        JUCE_MODULE_AVAILABLE_juce_audio_utils=1
        JUCE_MODULE_AVAILABLE_juce_core=1
        JUCE_MODULE_AVAILABLE_juce_cryptography=1
        JUCE_MODULE_AVAILABLE_juce_data_structures=1
        JUCE_MODULE_AVAILABLE_juce_dsp=1
        JUCE_MODULE_AVAILABLE_juce_events=1
        JUCE_MODULE_AVAILABLE_juce_graphics=1
        JUCE_MODULE_AVAILABLE_juce_gui_basics=1
        JUCE_MODULE_AVAILABLE_juce_gui_extra=1
        JUCE_USE_CURL=0
        JUCE_WEB_BROWSER=0
        RESONARIUM_IS_INSTRUMENT=1)
set_target_properties(resonarium_editor_lifecycle_test PROPERTIES FOLDER Tests)

if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    find_program(RESONARIUM_XVFB_RUN_EXECUTABLE xvfb-run)
endif()

if(RESONARIUM_XVFB_RUN_EXECUTABLE)
    add_test(NAME Resonarium.EditorLifecycle
            COMMAND "${RESONARIUM_XVFB_RUN_EXECUTABLE}" -a
                    "$<TARGET_FILE:resonarium_editor_lifecycle_test>")
else()
    add_test(NAME Resonarium.EditorLifecycle
            COMMAND "$<TARGET_FILE:resonarium_editor_lifecycle_test>")
endif()
set_tests_properties(Resonarium.EditorLifecycle PROPERTIES
        LABELS "resonarium;native;instrument;ui;lifecycle"
        RUN_SERIAL TRUE
        SKIP_RETURN_CODE 77
        TIMEOUT 180)

# JUCE 8.0.8's LV2 client cannot process a connection-optional audio input
# whose host buffer is null. The Instrument deliberately publishes no LV2
# audio inputs, while the Effect retains its required stereo input. Validate
# the generated manifests so a future bus change cannot silently reintroduce
# the wrapper crash.
add_test(NAME Resonarium.InstrumentLV2AudioPorts
        COMMAND "${CMAKE_COMMAND}"
                "-DLV2_BINARY=$<TARGET_FILE:Resonarium_Instrument_LV2>"
                -DEXPECTED_AUDIO_INPUTS=0
                -DEXPECTED_AUDIO_OUTPUTS=2
                -P "${CMAKE_CURRENT_SOURCE_DIR}/cmake/VerifyLv2AudioPorts.cmake"
        CONFIGURATIONS Release)
set_tests_properties(Resonarium.InstrumentLV2AudioPorts PROPERTIES
        LABELS "resonarium;native;lv2;instrument;audio"
        RUN_SERIAL TRUE
        TIMEOUT 120)

add_test(NAME Resonarium.EffectLV2AudioPorts
        COMMAND "${CMAKE_COMMAND}"
                "-DLV2_BINARY=$<TARGET_FILE:Resonarium_Effect_LV2>"
                -DEXPECTED_AUDIO_INPUTS=2
                -DEXPECTED_AUDIO_OUTPUTS=2
                -P "${CMAKE_CURRENT_SOURCE_DIR}/cmake/VerifyLv2AudioPorts.cmake"
        CONFIGURATIONS Release)
set_tests_properties(Resonarium.EffectLV2AudioPorts PROPERTIES
        LABELS "resonarium;native;lv2;effect;audio"
        RUN_SERIAL TRUE
        TIMEOUT 120)

add_test(NAME Resonarium.VoiceLifecycleHarnessModel
        COMMAND "${Python3_EXECUTABLE}" -m unittest tests.lifecycle.test_harness)
set_tests_properties(Resonarium.VoiceLifecycleHarnessModel PROPERTIES
        LABELS "resonarium;voice-lifecycle;python"
        TIMEOUT 60
        WORKING_DIRECTORY "${PROJECT_SOURCE_DIR}")

add_test(NAME Resonarium.VoicePerformanceHarnessModel
        COMMAND "${Python3_EXECUTABLE}" -m unittest tests.performance.test_steady_face_roll)
set_tests_properties(Resonarium.VoicePerformanceHarnessModel PROPERTIES
        LABELS "resonarium;voice-performance;python"
        TIMEOUT 60
        WORKING_DIRECTORY "${PROJECT_SOURCE_DIR}")

add_test(NAME Resonarium.ExciterHarnessModel
        COMMAND "${Python3_EXECUTABLE}" -m unittest
                tests.exciter.test_audio_contract)
set_tests_properties(Resonarium.ExciterHarnessModel PROPERTIES
        LABELS "resonarium;exciter;python"
        TIMEOUT 60
        WORKING_DIRECTORY "${PROJECT_SOURCE_DIR}")

set(RESONARIUM_EXCITER_AUDIO_SCRIPT
        "${PROJECT_SOURCE_DIR}/tests/exciter/audio_contract.py")
set(RESONARIUM_EXCITER_RACK_SCRIPT
        "${PROJECT_SOURCE_DIR}/tests/exciter/rack_contract.py")
set(RESONARIUM_EXCITER_RESULTS
        "${PROJECT_BINARY_DIR}/exciter-results/$<CONFIG>")
set(RESONARIUM_REQUIRE_RELEASE_SCRIPT
        "${CMAKE_CURRENT_SOURCE_DIR}/cmake/RequireReleaseConfig.cmake")

add_custom_target(Resonarium_ExciterAudioContract
        COMMAND "${CMAKE_COMMAND}"
                -DRESONARIUM_ACTIVE_CONFIG=$<CONFIG>
                -P "${RESONARIUM_REQUIRE_RELEASE_SCRIPT}"
        COMMAND "${Python3_EXECUTABLE}"
                "${RESONARIUM_EXCITER_AUDIO_SCRIPT}"
                --module "$<TARGET_FILE:Resonarium_Python_Test>"
                --output "${RESONARIUM_EXCITER_RESULTS}/audio-contract.json"
        DEPENDS Resonarium_Python_Test
        WORKING_DIRECTORY "${PROJECT_SOURCE_DIR}"
        USES_TERMINAL
        VERBATIM)

add_test(NAME Resonarium.ExciterAudioContract
        COMMAND "${Python3_EXECUTABLE}"
                "${RESONARIUM_EXCITER_AUDIO_SCRIPT}"
                --module "$<TARGET_FILE:Resonarium_Python_Test>"
                --output "${RESONARIUM_EXCITER_RESULTS}/audio-contract.json"
        CONFIGURATIONS Release)
set_tests_properties(Resonarium.ExciterAudioContract PROPERTIES
        LABELS "resonarium;exciter;python;audio"
        RUN_SERIAL TRUE
        TIMEOUT 1200
        WORKING_DIRECTORY "${PROJECT_SOURCE_DIR}")

add_test(NAME Resonarium.ExciterRackContract
        COMMAND "${Python3_EXECUTABLE}"
                "${RESONARIUM_EXCITER_RACK_SCRIPT}"
                --module "$<TARGET_FILE:Resonarium_Python_Test>"
                --output "${RESONARIUM_EXCITER_RESULTS}/rack-contract.json"
        CONFIGURATIONS Release)
set_tests_properties(Resonarium.ExciterRackContract PROPERTIES
        LABELS "resonarium;exciter;python;audio;lifecycle"
        RUN_SERIAL TRUE
        TIMEOUT 1200
        WORKING_DIRECTORY "${PROJECT_SOURCE_DIR}")

set(RESONARIUM_VOICE_HARNESS
        "${PROJECT_SOURCE_DIR}/tests/lifecycle/harness.py")
set(RESONARIUM_VOICE_RESULTS
        "${PROJECT_BINARY_DIR}/voice-lifecycle-results/$<CONFIG>")

add_custom_target(Resonarium_VoiceLifecycle
        COMMAND "${Python3_EXECUTABLE}"
                "${RESONARIUM_VOICE_HARNESS}"
                check
                --module "$<TARGET_FILE:Resonarium_Python>"
                --suite core
                --output "${RESONARIUM_VOICE_RESULTS}"
        DEPENDS Resonarium_Python
        WORKING_DIRECTORY "${PROJECT_SOURCE_DIR}"
        USES_TERMINAL
        VERBATIM)

add_custom_target(Resonarium_VoiceStress
        COMMAND "${Python3_EXECUTABLE}"
                "${RESONARIUM_VOICE_HARNESS}"
                stress
                --module "$<TARGET_FILE:Resonarium_Python>"
                --events 100000
                --output "${RESONARIUM_VOICE_RESULTS}"
        DEPENDS Resonarium_Python
        WORKING_DIRECTORY "${PROJECT_SOURCE_DIR}"
        USES_TERMINAL
        VERBATIM)

set(RESONARIUM_VOICE_PERFORMANCE_SCRIPT
        "${PROJECT_SOURCE_DIR}/tests/performance/steady_face_roll.py")
set(RESONARIUM_VOICE_PERFORMANCE_RESULTS
        "${PROJECT_BINARY_DIR}/voice-performance-results/$<CONFIG>")
add_custom_target(Resonarium_VoicePerformance
        COMMAND "${CMAKE_COMMAND}"
                -DRESONARIUM_ACTIVE_CONFIG=$<CONFIG>
                -P "${RESONARIUM_REQUIRE_RELEASE_SCRIPT}"
        COMMAND "${Python3_EXECUTABLE}"
                "${RESONARIUM_VOICE_PERFORMANCE_SCRIPT}"
                --module "$<TARGET_FILE:Resonarium_Python>"
                --output "${RESONARIUM_VOICE_PERFORMANCE_RESULTS}"
        DEPENDS Resonarium_Python
        WORKING_DIRECTORY "${PROJECT_SOURCE_DIR}"
        USES_TERMINAL
        VERBATIM)

add_test(NAME Resonarium.VoicePerformance
        COMMAND "${Python3_EXECUTABLE}"
                "${RESONARIUM_VOICE_PERFORMANCE_SCRIPT}"
                --module "$<TARGET_FILE:Resonarium_Python>"
                --output "${RESONARIUM_VOICE_PERFORMANCE_RESULTS}"
        CONFIGURATIONS Release)
set_tests_properties(Resonarium.VoicePerformance PROPERTIES
        LABELS "resonarium;voice-performance;python;performance"
        RUN_SERIAL TRUE
        TIMEOUT 1200
        WORKING_DIRECTORY "${PROJECT_SOURCE_DIR}")

set(RESONARIUM_AUDIO_INVARIANTS_SCRIPT
        "${PROJECT_SOURCE_DIR}/tests/lifecycle/audio_invariants.py")
set(RESONARIUM_AUDIO_INVARIANTS_RESULTS
        "${PROJECT_BINARY_DIR}/voice-lifecycle-results/$<CONFIG>/audio-invariants.json")

add_custom_target(Resonarium_AudioInvariants
        COMMAND "${CMAKE_COMMAND}"
                -DRESONARIUM_ACTIVE_CONFIG=$<CONFIG>
                -P "${RESONARIUM_REQUIRE_RELEASE_SCRIPT}"
        COMMAND "${Python3_EXECUTABLE}"
                "${RESONARIUM_AUDIO_INVARIANTS_SCRIPT}"
                --module "$<TARGET_FILE:Resonarium_Python_Test>"
                --output "${RESONARIUM_AUDIO_INVARIANTS_RESULTS}"
        DEPENDS Resonarium_Python_Test
        WORKING_DIRECTORY "${PROJECT_SOURCE_DIR}"
        USES_TERMINAL
        VERBATIM)

add_test(NAME Resonarium.AudioInvariants
        COMMAND "${Python3_EXECUTABLE}"
                "${RESONARIUM_AUDIO_INVARIANTS_SCRIPT}"
                --module "$<TARGET_FILE:Resonarium_Python_Test>"
                --output "${RESONARIUM_AUDIO_INVARIANTS_RESULTS}"
        CONFIGURATIONS Release)
set_tests_properties(Resonarium.AudioInvariants PROPERTIES
        LABELS "resonarium;voice-lifecycle;python;audio"
        RUN_SERIAL TRUE
        TIMEOUT 1200
        WORKING_DIRECTORY "${PROJECT_SOURCE_DIR}")
