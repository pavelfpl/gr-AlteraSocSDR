# Looking for AT86RF215 on the machine
# ------------------------------------
# Inspirated by: https://wiki.gnuradio.org/index.php?title=Interfacing_Hardware_with_a_C%2B%2B_OOT_Module
# -------------------------------------------------------------------------------------------------------
#
# -- Once done this will define these vars --
#
# AT86RF215_INCLUDE_DIRS - where to find the header files
# AT86RF215_LIBRARYS - where to find the dynamically loaded library files (.so)
# AT86RF215_FOUND - true if LIMESUITE was found

find_path(
                AT86RF215_INCLUDE_DIR
                NAMES at86rf215.h at86rf215_radio.h
                # Local testing path
                # PATHS /home/pavelf/gnuradio_addons/lib_at86rf215/include
                PATHS /usr/include /usr/local/include /usr/local/include/io_utils
                DOC "AT86RF215 include file"
)

find_library(
                AT86RF215_LIBRARY
                _at86rf215_lib
                # Local testing path
                # PATHS /home/pavelf/gnuradio_addons/lib_at86rf215/lib
                PATHS /usr/lib /usr/local/lib
                DOC "AT86RF215 shared library object file"
)

if(AT86RF215_INCLUDE_DIR AND AT86RF215_LIBRARY)
  set(AT86RF215_FOUND 1)
  set(AT86RF215_LIBRARIES ${AT86RF215_LIBRARY})
  set(AT86RF215_INCLUDE_DIRS ${AT86RF215_INCLUDE_DIR})
else()
  set(AT86RF215_FOUND 0)
  set(AT86RF215_LIBRARIES)
  set(AT86RF215_INCLUDE_DIRS)
endif()

mark_as_advanced(AT86RF215_INCLUDE_DIR)
mark_as_advanced(AT86RF215_LIBRARY)
mark_as_advanced(AT86RF215_FOUND)

if(NOT AT86RF215_FOUND)
        set(AT86RF215_DIR_MESSAGE "AT86RF215 was not found. Has it been already installed ?")
        if(NOT AT86RF215_FIND_QUIETLY)
           message(STATUS "${AT86RF215_DIR_MESSAGE}")
        else()
           if(AT86RF215_FIND_REQUIRED)
              message(FATAL_ERROR "${AT86RF215_DIR_MESSAGE}")
           endif()
        endif()
else()
        message(STATUS "Found AT86RF215 Suite: ${AT86RF215_LIBRARIES} and ${AT86RF215_INCLUDE_DIRS}")
endif()
