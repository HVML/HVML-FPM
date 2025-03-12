# FIXME: These should line up with versions in Version.xcconfig
set(HVMLFPM_MAC_VERSION 0.0.1)
set(MACOSX_FRAMEWORK_BUNDLE_VERSION 0.0.1)

HVMLFPM_OPTION_BEGIN()
# Private options shared with other HvmlFpm ports. Add options here only if
# we need a value different from the default defined in GlobalFeatures.cmake.
HVMLFPM_OPTION_END()

set(HvmlFpm_PKGCONFIG_FILE ${CMAKE_BINARY_DIR}/src/hvmlfpm/hvmlfpm.pc)

set(HvmlFpm_LIBRARY_TYPE SHARED)
set(HvmlFpmTestSupport_LIBRARY_TYPE SHARED)

