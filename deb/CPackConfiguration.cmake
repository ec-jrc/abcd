set(CPACK_GENERATOR "DEB")

set(CPACK_DEBIAN_FILE_NAME DEB-DEFAULT)

set(CPACK_DEB_COMPONENT_INSTALL ON)
set(CPACK_COMPONENTS_ALL core)

if(BUILD_ABSP)
    list(APPEND CPACK_COMPONENTS_ALL absp)
endif()

set(CPACK_DEBIAN_PACKAGE_MAINTAINER "Cristiano Fontana <cristiano.fontana@ec.europa.eu>")
set(CPACK_DEBIAN_PACKAGE_VERSION ${PROJECT_VERSION})
set(CPACK_DEBIAN_PACKAGE_SECTION "science")
set(CPACK_DEBIAN_PACKAGE_PRIORITY "optional")

set(CPACK_COMPONENT_CORE_DESCRIPTION_SUMMARY
    "ABCD is a distributed data acquisistion system that can acquire waveforms and processed data from digitizers of multiple vendors."
    "ABCD is designed to be versatile and usable on different experiments without the need of modifying the source code."
    "The waveforms processing, spectra calculation and Time-of-Flight analysis may be performed online and offline."
)
set(CPACK_DEBIAN_CORE_PACKAGE_NAME "abcd-core")
set(CPACK_DEBIAN_CORE_PACKAGE_DEPENDS "tmux, libzmq5, libjansson4, gsl-bin, python3, python3-zmq, python3-numpy, python3-scipy, python3-matplotlib, nodejs, npm")
set(CPACK_DEBIAN_CORE_PACKAGE_CONTROL_EXTRA "${CMAKE_CURRENT_SOURCE_DIR}/deb/postinst")

set(CPACK_COMPONENT_ABSP_DESCRIPTION_SUMMARY "ABCD's interface for SP Devices digitizers")
set(CPACK_DEBIAN_ABSP_PACKAGE_NAME "abcd-absp")
set(CPACK_DEBIAN_ABSP_PACKAGE_DEPENDS "libzmq5, libjansson4, lua5.4, liblua5.4-0, swig, spd-adq-pci-dkms, libadq0, adqtools")

include(CPack)

cpack_add_component(core REQUIRED)

if(BUILD_ABSP)
    cpack_add_component(absp DEPENDS core)
endif()