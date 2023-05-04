
if( NOT LIBXCB_ROOT )
  find_path (LIBXCB_INCLUDE_DIRS NAMES X11/Xutil.h PATHS /usr/include /usr/local/include )
  find_library (LIBXCB_LIBRARY NAMES xcb )
else()
  find_path (LIBXCB_INCLUDE_DIRS NAMES X11/Xutil.h PATHS ${LIBXCB_ROOT}/include )
  find_library (LIBXCB_LIBRARY NAMES xcb PATHS ${LIBXCB_ROOT}/lib ${LIBXCB_ROOT}/lib64 ${LIBXCB_ROOT} NO_DEFAULT_PATH )
endif()
include (FindPackageHandleStandardArgs)
find_package_handle_standard_args(
  LIBXCB DEFAULT_MSG
  LIBXCB_INCLUDE_DIRS LIBXCB_LIBRARY
)
set(LIBXCB_LIBRARIES "${LIBXCB_LIBRARY}")
mark_as_advanced(LIBXCB_INCLUDE_DIRS LIBXCB_LIBRARIES LIBXCB_LIBRARY)

