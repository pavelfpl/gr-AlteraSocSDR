INCLUDE(FindPkgConfig)
PKG_CHECK_MODULES(PC_ALTERASOCSDR AlteraSocSDR)

FIND_PATH(
    ALTERASOCSDR_INCLUDE_DIRS
    NAMES AlteraSocSDR/api.h
    HINTS $ENV{ALTERASOCSDR_DIR}/include
        ${PC_ALTERASOCSDR_INCLUDEDIR}
    PATHS ${CMAKE_INSTALL_PREFIX}/include
          /usr/local/include
          /usr/include
)

FIND_LIBRARY(
    ALTERASOCSDR_LIBRARIES
    NAMES gnuradio-AlteraSocSDR
    HINTS $ENV{ALTERASOCSDR_DIR}/lib
        ${PC_ALTERASOCSDR_LIBDIR}
    PATHS ${CMAKE_INSTALL_PREFIX}/lib
          ${CMAKE_INSTALL_PREFIX}/lib64
          /usr/local/lib
          /usr/local/lib64
          /usr/lib
          /usr/lib64
)

INCLUDE(FindPackageHandleStandardArgs)
FIND_PACKAGE_HANDLE_STANDARD_ARGS(ALTERASOCSDR DEFAULT_MSG ALTERASOCSDR_LIBRARIES ALTERASOCSDR_INCLUDE_DIRS)
MARK_AS_ADVANCED(ALTERASOCSDR_LIBRARIES ALTERASOCSDR_INCLUDE_DIRS)

