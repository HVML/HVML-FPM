if (NOT TARGET HvmlFpm::HvmlFpm)
    if (NOT INTERNAL_BUILD)
        message(FATAL_ERROR "HvmlFpm::HvmlFpm target not found")
    endif ()

    # This should be moved to an if block if the Apple Mac/iOS build moves completely to CMake
    # Just assuming Windows for the moment
    add_library(HvmlFpm::HvmlFpm STATIC IMPORTED)
    set_target_properties(HvmlFpm::HvmlFpm PROPERTIES
        IMPORTED_LOCATION ${WEBKIT_LIBRARIES_LINK_DIR}/HvmlFpm${DEBUG_SUFFIX}.lib
    )
    set(HvmlFpm_PRIVATE_FRAMEWORK_HEADERS_DIR "${CMAKE_BINARY_DIR}/../include/private")
    target_include_directories(HvmlFpm::HvmlFpm INTERFACE
        ${HvmlFpm_PRIVATE_FRAMEWORK_HEADERS_DIR}
    )
endif ()
