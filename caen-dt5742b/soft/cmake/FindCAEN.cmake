### @author: Roberto Preghenella
### @email: preghenella@bo.infn.it

find_path(CAEN_INCLUDE_DIR
  NAMES CAENDigitizer.h
  PATH_SUFFIXES uhal
  PATHS /usr/include)

set(CAEN_INCLUDE_DIR ${CAEN_INCLUDE_DIR}/..)

find_library(CAEN_LIBRARIES
  NAMES libCAENDigitizer.so
  PATHS /usr/lib)

find_package_handle_standard_args(CAEN
  REQUIRED_VARS CAEN_INCLUDE_DIR CAEN_LIBRARIES
  FAIL_MESSAGE "CAEN could not be found")
