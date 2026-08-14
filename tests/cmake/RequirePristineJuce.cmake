include_guard(GLOBAL)

# A Resonarium build is reproducible only when JUCE is the exact, unmodified
# submodule committed by the superproject. Keep the detailed policy and JSON
# evidence format in the cross-platform Python verifier used by CI as well.
if(DEFINED RESONARIUM_SOURCE_ROOT)
    set(_resonarium_integrity_source_root "${RESONARIUM_SOURCE_ROOT}")
else()
    set(_resonarium_integrity_source_root "${CMAKE_SOURCE_DIR}")
endif()

if(DEFINED RESONARIUM_INTEGRITY_REPORT)
    set(_resonarium_juce_integrity_report "${RESONARIUM_INTEGRITY_REPORT}")
else()
    set(_resonarium_juce_integrity_report "${CMAKE_BINARY_DIR}/juce-integrity.json")
endif()

find_package(Python3 COMPONENTS Interpreter REQUIRED)
execute_process(
    COMMAND
        "${Python3_EXECUTABLE}"
        "${_resonarium_integrity_source_root}/tests/ci/verify_pristine_juce.py"
        --repo-root "${_resonarium_integrity_source_root}"
        --output "${_resonarium_juce_integrity_report}"
    RESULT_VARIABLE _resonarium_juce_integrity_result
    OUTPUT_VARIABLE _resonarium_juce_integrity_stdout
    ERROR_VARIABLE _resonarium_juce_integrity_stderr
)

if(NOT _resonarium_juce_integrity_result EQUAL 0)
    string(STRIP "${_resonarium_juce_integrity_stderr}" _resonarium_juce_integrity_error)
    message(FATAL_ERROR
        "Resonarium refuses to configure against a modified or unpinned JUCE checkout.\n"
        "${_resonarium_juce_integrity_error}\n"
        "Restore the committed modules/juce submodule; do not patch JUCE in place.\n"
        "Integrity evidence: ${_resonarium_juce_integrity_report}")
endif()

string(STRIP "${_resonarium_juce_integrity_stdout}" _resonarium_juce_integrity_message)
message(STATUS "${_resonarium_juce_integrity_message}")

unset(_resonarium_integrity_source_root)
unset(_resonarium_juce_integrity_report)
unset(_resonarium_juce_integrity_result)
unset(_resonarium_juce_integrity_stdout)
unset(_resonarium_juce_integrity_stderr)
unset(_resonarium_juce_integrity_error)
