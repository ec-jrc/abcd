set(CPACK_GENERATOR "DEB")
set(CPACK_PACKAGE_NAME "abcd-core")
set(CPACK_PACKAGE_VERSION ${PROJECT_VERSION})
set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "ABCD, a distributed data acquisition system for signal digitizers from multiple vendors")
set(CPACK_PACKAGE_DESCRIPTION
    "ABCD is a distributed data acquisistion system that can acquire waveforms and processed data from digitizers of multiple vendors."
    "ABCD is designed to be versatile and usable on different experiments without the need of modifying the source code."
    "The waveforms processing, spectra calculation and Time-of-Flight analysis may be performed online and offline."
)
set(CPACK_PACKAGE_CONTACT "Cristiano Fontana")
set(CPACK_DEBIAN_PACKAGE_MAINTAINER "Cristiano Fontana")
set(CPACK_DEBIAN_PACKAGE_DEPENDS "tmux, clang, libc++1, libc++-dev, libc++abi-dev, libzmq5, libzmq3-dev, libjsoncpp-dev, libjsoncpp25, libjansson4, libjansson-dev, zlib1g, zlib1g-dev, libbz2-1.0, libbz2-dev, python3, python3-zmq, python3-numpy, python3-scipy, python3-matplotlib, nodejs, npm, linux-headers-generic, build-essential, dkms, libgsl-dev, lua5.4, liblua5.4-dev, liblua5.4-0, swig")
set(CPACK_DEBIAN_PACKAGE_CONTROL_EXTRA "deb/postinst")
include(CPack)