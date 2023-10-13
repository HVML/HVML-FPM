include(GlobalCommon)

configure_file(config.h.in ${FORWARDING_HEADERS_DIR}/config.h @ONLY)

HVMLFCGI_EXECUTABLE_DECLARE(hvmlfcgi)

list(APPEND hvmlfcgi_PRIVATE_INCLUDE_DIRECTORIES
    "${CMAKE_BINARY_DIR}"
    "${FORWARDING_HEADERS_DIR}"
    "${PURC_INCLUDE_DIRS}"
)

HVMLFCGI_EXECUTABLE(hvmlfcgi)

list(APPEND hvmlfcgi_SOURCES
    hvml-fcgi.c
)

set(hvmlfcgi_LIBRARIES
    PurC::PurC
)

set_target_properties(hvmlfcgi PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/sbin"
)

HVMLFCGI_COMPUTE_SOURCES(hvmlfcgi)
HVMLFCGI_FRAMEWORK(hvmlfcgi)

install(TARGETS hvmlfcgi DESTINATION "${SYSEXEC_INSTALL_DIR}/")
