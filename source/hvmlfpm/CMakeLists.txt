include(GlobalCommon)

configure_file(config.h.in ${FORWARDING_HEADERS_DIR}/config.h @ONLY)

HVMLFPM_EXECUTABLE_DECLARE(hvmlfpm)
HVMLFPM_INCLUDE_CONFIG_FILES_IF_EXISTS()

list(APPEND hvmlfpm_PRIVATE_INCLUDE_DIRECTORIES
    "${CMAKE_BINARY_DIR}"
    "${FORWARDING_HEADERS_DIR}"
)

list(APPEND hvmlfpm_SYSTEM_INCLUDE_DIRECTORIES
    "${PURC_INCLUDE_DIRS}"
)

HVMLFPM_EXECUTABLE(hvmlfpm)

list(APPEND hvmlfpm_SOURCES
    "hvml-fpm.c"
    "hvml-executor.c"
    "multipart-parser.c"
    "mpart-body-processor.c"
    "libfcgi/fcgiapp.c"
    "libfcgi/fcgi_stdio.c"
    "libfcgi/strerror.c"
    "util/avl.c"
    "util/avl-cmp.c"
    "util/kvlist.c"
)

set(hvmlfpm_LIBRARIES
    PurC::PurC
)

set_target_properties(hvmlfpm PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/sbin"
)

HVMLFPM_COMPUTE_SOURCES(hvmlfpm)
HVMLFPM_FRAMEWORK(hvmlfpm)

install(TARGETS hvmlfpm DESTINATION "${SYSEXEC_INSTALL_DIR}/")
