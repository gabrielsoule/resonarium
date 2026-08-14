if(NOT RESONARIUM_ACTIVE_CONFIG STREQUAL "Release")
    message(FATAL_ERROR
            "This acceptance target requires Release; active configuration is "
            "'${RESONARIUM_ACTIVE_CONFIG}'. Rebuild with --config Release.")
endif()
