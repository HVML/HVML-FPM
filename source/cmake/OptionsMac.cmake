# FIXME: These should line up with versions in Version.xcconfig
set(HVMLFPM_MAC_VERSION 0.0.1)
set(MACOSX_FRAMEWORK_BUNDLE_VERSION 0.0.1)

find_package(LibXml2 2.8.0)
find_package(LibXslt 1.1.7)
find_package(CURL 7.60.0)
find_package(OpenSSL 1.1.1)
find_package(SQLite3 3.10.0)

HVMLFPM_OPTION_BEGIN()
# Private options shared with other HvmlFpm ports. Add options here only if
# we need a value different from the default defined in GlobalFeatures.cmake.

HVMLFPM_OPTION_DEFAULT_PORT_VALUE(ENABLE_XML PUBLIC OFF)
HVMLFPM_OPTION_DEFAULT_PORT_VALUE(ENABLE_HTTP PUBLIC OFF)
HVMLFPM_OPTION_DEFAULT_PORT_VALUE(ENABLE_LSQL PUBLIC OFF)
HVMLFPM_OPTION_DEFAULT_PORT_VALUE(ENABLE_RSQL PUBLIC OFF)
HVMLFPM_OPTION_DEFAULT_PORT_VALUE(ENABLE_HIBUS PUBLIC OFF)
HVMLFPM_OPTION_DEFAULT_PORT_VALUE(ENABLE_SSL PUBLIC OFF)

HVMLFPM_OPTION_END()

set(HvmlFpm_PKGCONFIG_FILE ${CMAKE_BINARY_DIR}/src/hvmlfpm/hvmlfpm.pc)

set(HvmlFpm_LIBRARY_TYPE SHARED)
set(HvmlFpmTestSupport_LIBRARY_TYPE SHARED)

