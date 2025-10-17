# FindCAEN.cmake
# This module searches for CAEN libraries and sets the following variables:
# CAEN_FOUND            - True if all required components are found
# CAEN_INCLUDE_DIR      - The main directory containing CAEN headers
# CAEN_LIBRARIES        - All CAEN library files
# CAEN<COMPONENT>_LIBRARY - Specific component library files

include(FindPackageHandleStandardArgs)

find_path(CAEN_INCLUDE_DIR
    NAMES CAENVMElib.h
    PATHS /usr/include /usr/local/include
    DOC "Path to CAEN header files"
)

# Find specific library files
find_library(CAENVME_LIBRARY
    NAMES CAENVME
    PATHS /usr/lib
    DOC "Path to the CAENVME library"
)

find_library(CAENCOMM_LIBRARY
    NAMES CAENComm
    PATHS /usr/lib
    DOC "Path to the CAENComm library"
)

find_library(CAENDIGITIZER_LIBRARY
    NAMES CAENDigitizer
    PATHS /usr/lib
    DOC "Path to the CAENDigitizer library"
)

# This is only used for DPP-PHA firmwares
#find_library(CAENDPPLIB_LIBRARY OPTIONAL
#    NAMES CAENDPPLib
#    PATHS /usr/lib
#    DOC "Path to the CAENDPPLib library"
#)

function(check_dkms_module module_name)
    execute_process(
        COMMAND dkms status
        OUTPUT_VARIABLE DKMS_STATUS
        ERROR_VARIABLE DKMS_ERROR
        OUTPUT_STRIP_TRAILING_WHITESPACE
    )

    string(FIND "${DKMS_STATUS}" "${module_name}" MODULE_FOUND)

    if (MODULE_FOUND GREATER -1)
        set(${module_name}_FOUND TRUE PARENT_SCOPE)
    else()
        set(${module_name}_FOUND FALSE PARENT_SCOPE)
    endif()
endfunction()

check_dkms_module("CAENUSBdrvB")

if(CAENUSBdrvB_FOUND)
    message(STATUS "DKMS module CAENUSBdrvB is installed")
else()
    message(FATAL_ERROR "DKMS module CAENUSBdrvB is NOT installed")
endif()

set(CAEN_LIBRARIES ${CAENVME_LIBRARY} ${CAENCOMM_LIBRARY} ${CAENDIGITIZER_LIBRARY})

find_package_handle_standard_args(CAEN DEFAULT_MSG
    CAEN_INCLUDE_DIR
    CAENVME_LIBRARY
    CAENCOMM_LIBRARY
    CAENDIGITIZER_LIBRARY
    CAENUSBdrvB_FOUND
)

if(CAEN_FOUND)
    message(STATUS "Found CAEN libraries include directory: ${CAEN_INCLUDE_DIR}")
    message(STATUS "Found CAENVME library: ${CAENVME_LIBRARY}")
    message(STATUS "Found CAENComm library: ${CAENCOMM_LIBRARY}")
    message(STATUS "Found CAENDigitizer library: ${CAENDIGITIZER_LIBRARY}")
endif()
