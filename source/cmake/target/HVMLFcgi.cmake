if (NOT TARGET HVMLFcgi::HVMLFcgi)
    if (NOT INTERNAL_BUILD)
        message(FATAL_ERROR "HVMLFcgi::HVMLFcgi target not found")
    endif ()

    # This should be moved to an if block if the Apple Mac/iOS build moves completely to CMake
    # Just assuming Windows for the moment
    add_library(HVMLFcgi::HVMLFcgi STATIC IMPORTED)
    set_target_properties(HVMLFcgi::HVMLFcgi PROPERTIES
        IMPORTED_LOCATION ${WEBKIT_LIBRARIES_LINK_DIR}/HVMLFcgi${DEBUG_SUFFIX}.lib
    )
    set(HVMLFcgi_PRIVATE_FRAMEWORK_HEADERS_DIR "${CMAKE_BINARY_DIR}/../include/private")
    target_include_directories(HVMLFcgi::HVMLFcgi INTERFACE
        ${HVMLFcgi_PRIVATE_FRAMEWORK_HEADERS_DIR}
    )
endif ()
