if(NOT DEFINED LV2_BINARY OR NOT DEFINED EXPECTED_AUDIO_INPUTS
   OR NOT DEFINED EXPECTED_AUDIO_OUTPUTS)
    message(FATAL_ERROR
            "LV2_BINARY, EXPECTED_AUDIO_INPUTS, and EXPECTED_AUDIO_OUTPUTS are required")
endif()

get_filename_component(LV2_BUNDLE_DIR "${LV2_BINARY}" DIRECTORY)
set(LV2_DSP_TTL "${LV2_BUNDLE_DIR}/dsp.ttl")
if(NOT EXISTS "${LV2_DSP_TTL}")
    message(FATAL_ERROR "LV2 DSP manifest was not generated: ${LV2_DSP_TTL}")
endif()

file(READ "${LV2_DSP_TTL}" LV2_DSP_TEXT)
string(REGEX MATCHALL
       "a[ \t\r\n]+lv2:InputPort[ \t\r\n]*,[ \t\r\n]*lv2:AudioPort"
       LV2_AUDIO_INPUT_PORTS "${LV2_DSP_TEXT}")
string(REGEX MATCHALL
       "a[ \t\r\n]+lv2:OutputPort[ \t\r\n]*,[ \t\r\n]*lv2:AudioPort"
       LV2_AUDIO_OUTPUT_PORTS "${LV2_DSP_TEXT}")
list(LENGTH LV2_AUDIO_INPUT_PORTS LV2_AUDIO_INPUT_COUNT)
list(LENGTH LV2_AUDIO_OUTPUT_PORTS LV2_AUDIO_OUTPUT_COUNT)

if(NOT LV2_AUDIO_INPUT_COUNT EQUAL EXPECTED_AUDIO_INPUTS)
    message(FATAL_ERROR
            "${LV2_DSP_TTL} advertises ${LV2_AUDIO_INPUT_COUNT} audio inputs; "
            "expected ${EXPECTED_AUDIO_INPUTS}")
endif()
if(NOT LV2_AUDIO_OUTPUT_COUNT EQUAL EXPECTED_AUDIO_OUTPUTS)
    message(FATAL_ERROR
            "${LV2_DSP_TTL} advertises ${LV2_AUDIO_OUTPUT_COUNT} audio outputs; "
            "expected ${EXPECTED_AUDIO_OUTPUTS}")
endif()

# Counts alone are too weak for a host-facing port contract. JUCE's generated
# metadata uses stable audio_in_N/audio_out_N symbols, consecutive indices,
# and explicit stereo designations. Pin all three so the intentional
# zero-input Instrument LV2 topology cannot drift silently.
if(EXPECTED_AUDIO_INPUTS GREATER 0)
    math(EXPR LV2_LAST_INPUT "${EXPECTED_AUDIO_INPUTS} - 1")
    foreach(LV2_ZERO_BASED_INDEX RANGE 0 ${LV2_LAST_INPUT})
        math(EXPR LV2_CHANNEL "${LV2_ZERO_BASED_INDEX} + 1")
        if(LV2_CHANNEL EQUAL 1)
            set(LV2_DESIGNATION "left")
        else()
            set(LV2_DESIGNATION "right")
        endif()
        string(REGEX MATCH
               "a[ \t\r\n]+lv2:InputPort[ \t\r\n]*,[ \t\r\n]*lv2:AudioPort[ \t\r\n]*;[^]]*lv2:index[ \t]+${LV2_ZERO_BASED_INDEX}[ \t]*;[^]]*lv2:symbol[ \t]+\"audio_in_${LV2_CHANNEL}\"[^]]*lv2:designation[ \t]+<http://lv2plug.in/ns/ext/port-groups#${LV2_DESIGNATION}>"
               LV2_INPUT_CONTRACT_MATCH "${LV2_DSP_TEXT}")
        if(NOT LV2_INPUT_CONTRACT_MATCH)
            message(FATAL_ERROR
                    "${LV2_DSP_TTL} is missing required audio input contract "
                    "audio_in_${LV2_CHANNEL} at index ${LV2_ZERO_BASED_INDEX}")
        endif()
    endforeach()
endif()

if(EXPECTED_AUDIO_OUTPUTS GREATER 0)
    math(EXPR LV2_LAST_OUTPUT "${EXPECTED_AUDIO_OUTPUTS} - 1")
    foreach(LV2_OUTPUT_OFFSET RANGE 0 ${LV2_LAST_OUTPUT})
        math(EXPR LV2_CHANNEL "${LV2_OUTPUT_OFFSET} + 1")
        if(LV2_CHANNEL EQUAL 1)
            set(LV2_DESIGNATION "left")
        else()
            set(LV2_DESIGNATION "right")
        endif()
        math(EXPR LV2_ZERO_BASED_INDEX
             "${EXPECTED_AUDIO_INPUTS} + ${LV2_OUTPUT_OFFSET}")
        string(REGEX MATCH
               "a[ \t\r\n]+lv2:OutputPort[ \t\r\n]*,[ \t\r\n]*lv2:AudioPort[ \t\r\n]*;[^]]*lv2:index[ \t]+${LV2_ZERO_BASED_INDEX}[ \t]*;[^]]*lv2:symbol[ \t]+\"audio_out_${LV2_CHANNEL}\"[^]]*lv2:designation[ \t]+<http://lv2plug.in/ns/ext/port-groups#${LV2_DESIGNATION}>"
               LV2_OUTPUT_CONTRACT_MATCH "${LV2_DSP_TEXT}")
        if(NOT LV2_OUTPUT_CONTRACT_MATCH)
            message(FATAL_ERROR
                    "${LV2_DSP_TTL} is missing required audio output contract "
                    "audio_out_${LV2_CHANNEL} at index ${LV2_ZERO_BASED_INDEX}")
        endif()
    endforeach()
endif()

string(REGEX MATCHALL
       "a[ \t\r\n]+lv2:(Input|Output)Port[ \t\r\n]*,[ \t\r\n]*lv2:AudioPort[ \t\r\n]*;[^]]*lv2:connectionOptional"
       LV2_OPTIONAL_AUDIO_PORTS "${LV2_DSP_TEXT}")
list(LENGTH LV2_OPTIONAL_AUDIO_PORTS LV2_OPTIONAL_AUDIO_PORT_COUNT)
if(NOT LV2_OPTIONAL_AUDIO_PORT_COUNT EQUAL 0)
    message(FATAL_ERROR
            "${LV2_DSP_TTL} unexpectedly advertises connection-optional audio ports")
endif()

message(STATUS
        "Verified LV2 audio ports: ${LV2_AUDIO_INPUT_COUNT} input, "
        "${LV2_AUDIO_OUTPUT_COUNT} output, exact symbols/indices, no optional "
        "audio ports (${LV2_DSP_TTL})")
