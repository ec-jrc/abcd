set(CPACK_GENERATOR "DEB")

set(CPACK_DEBIAN_FILE_NAME DEB-DEFAULT)

set(CPACK_DEB_COMPONENT_INSTALL ON)
set(CPACK_COMPONENTS_ALL core wit)

if(BUILD_ABSP)
    list(APPEND CPACK_COMPONENTS_ALL absp)
endif()
if(BUILD_ABCD)
    list(APPEND CPACK_COMPONENTS_ALL abcd)
endif()

set(CPACK_DEBIAN_PACKAGE_MAINTAINER "Cristiano Fontana <cristiano.fontana@ec.europa.eu>")
set(CPACK_DEBIAN_PACKAGE_VERSION ${PROJECT_VERSION})
set(CPACK_DEBIAN_PACKAGE_SECTION "science")
set(CPACK_DEBIAN_PACKAGE_PRIORITY "optional")

set(CPACK_DEBIAN_PACKAGE_SHLIBDEPS_PRIVATE_DIRS LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}/abcd/)

#
# Core package settings
#

set(CPACK_COMPONENT_CORE_DESCRIPTION_SUMMARY
    "ABCD is a distributed data acquisistion system that can acquire waveforms and processed data from digitizers of multiple vendors."
    "ABCD is designed to be versatile and usable on different experiments without the need of modifying the source code."
    "The waveforms processing, spectra calculation and Time-of-Flight analysis may be performed online and offline."
)
set(CPACK_DEBIAN_CORE_PACKAGE_NAME "abcd-core")

# Automatically determine shared library dependencies.
set(CPACK_DEBIAN_CORE_PACKAGE_SHLIBDEPS ON)

# The libjansson-dev and libgsl-dev are necessary for the installation as the
# users might want to compile the waan_libraries
set(CPACK_DEBIAN_CORE_PACKAGE_DEPENDS
    "tmux,
     libjansson-dev,
     libgsl-dev,
     python3,
     python3-zmq,
     python3-numpy,
     python3-scipy,
     python3-matplotlib")

set(CPACK_DEBIAN_CORE_PACKAGE_CONTROL_EXTRA "${CMAKE_CURRENT_SOURCE_DIR}/deb/postinst")

#
# wit package settings
#

set(CPACK_COMPONENT_WIT_DESCRIPTION_SUMMARY
    "ABCD Web Interface"
)

set(CPACK_DEBIAN_WIT_PACKAGE_NAME "abcd-wit")

# IMPORTANT: disable shlibdeps for Node.js addons,
# otherwise it would analyze the whole node_modules directory
set(CPACK_DEBIAN_WIT_PACKAGE_SHLIBDEPS OFF)

set(CPACK_DEBIAN_WIT_PACKAGE_DEPENDS
    "nodejs,
     npm"
)

#
# absp package settings
#

set(CPACK_COMPONENT_ABSP_DESCRIPTION_SUMMARY "ABCD's interface for SP Devices digitizers")
set(CPACK_DEBIAN_ABSP_PACKAGE_NAME "abcd-absp")
set(CPACK_DEBIAN_ABSP_PACKAGE_SHLIBDEPS ON)
set(CPACK_DEBIAN_ABSP_PACKAGE_DEPENDS
    "swig,
     lua5.4,
     spd-adq-pci-dkms,
     adqtools")

#
# abcd package settings
#

set(CPACK_COMPONENT_ABCD_DESCRIPTION_SUMMARY "ABCD's interface for CAEN digitizers")
set(CPACK_DEBIAN_ABCD_PACKAGE_NAME "abcd-abcd")
set(CPACK_DEBIAN_ABCD_PACKAGE_SHLIBDEPS ON)
set(CPACK_DEBIAN_ABCD_PACKAGE_DEPENDS "")
set(CPACK_DEBIAN_ABCD_PACKAGE_CONTROL_EXTRA "${CMAKE_CURRENT_SOURCE_DIR}/abcd/deb/preinst")

include(CPack)

cpack_add_component(core REQUIRED)

if(BUILD_ABSP)
    cpack_add_component(absp DEPENDS core)
endif()
if(BUILD_ABCD)
    cpack_add_component(abcd DEPENDS core)
endif()