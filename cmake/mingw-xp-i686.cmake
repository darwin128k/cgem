# Windows XP 32-bit (legacy helper; prefer cmake/mingw-windows.cmake).
set(CGEM_MINGW_ARCH i686)
set(CGEM_WIN32_WINNT 0x0501)
include(${CMAKE_CURRENT_LIST_DIR}/mingw-windows.cmake)
