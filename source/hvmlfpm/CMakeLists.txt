include(GlobalCommon)

configure_file(config.h.in ${FORWARDING_HEADERS_DIR}/config.h @ONLY)

HVMLFPM_INCLUDE_CONFIG_FILES_IF_EXISTS()

HVMLFPM_EXECUTABLE_DECLARE(hvmlfpm)

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

HVMLFPM_EXECUTABLE_DECLARE(testexecutor)

list(APPEND testexecutor_DEFINITIONS
    "NO_FCGI_DEFINES"
)

list(APPEND testexecutor_PRIVATE_INCLUDE_DIRECTORIES
    "${CMAKE_BINARY_DIR}"
    "${FORWARDING_HEADERS_DIR}"
)

list(APPEND testexecutor_SYSTEM_INCLUDE_DIRECTORIES
    "${PURC_INCLUDE_DIRS}"
)

HVMLFPM_EXECUTABLE(testexecutor)

list(APPEND testexecutor_SOURCES
    "test-executor.c"
    "hvml-executor.c"
    "multipart-parser.c"
    "mpart-body-processor.c"
    "util/avl.c"
    "util/avl-cmp.c"
    "util/kvlist.c"
)

set(testexecutor_LIBRARIES
    PurC::PurC
)

HVMLFPM_COMPUTE_SOURCES(testexecutor)
HVMLFPM_FRAMEWORK(testexecutor)

set_target_properties(testexecutor PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
)

set(testcase_FILES
    "testcase/get.txt"
    "testcase/echo.hvml"
    "testcase/post-plain.txt"
    "testcase/post-json.txt"
    "testcase/post-urlencoded.txt"
    "testcase/post-multipart-form-data.txt"
)

HVMLFPM_COPY_FILES(TestCaseFiles
    DESTINATION "${CMAKE_BINARY_DIR}"
    FILES ${testcase_FILES}
)

