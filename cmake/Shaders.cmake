# Offline HLSL compilation with fxc.exe (Windows SDK). Bytecode is emitted as C headers in the
# build tree (/Fh + /Vn); nothing is compiled at runtime and no DXBC is stored in the repository.

function(_freeframe_find_fxc)
  if(FREEFRAME_FXC)
    return()
  endif()
  set(_hints "")
  set(_kits "")
  if(CMAKE_WINDOWS_KITS_10_DIR)
    list(APPEND _kits "${CMAKE_WINDOWS_KITS_10_DIR}")
  endif()
  if(DEFINED ENV{WindowsSdkDir})
    list(APPEND _kits "$ENV{WindowsSdkDir}")
  endif()
  set(_pf86 "ProgramFiles(x86)")
  if(DEFINED ENV{${_pf86}})
    list(APPEND _kits "$ENV{${_pf86}}/Windows Kits/10")
  endif()
  list(APPEND _kits "C:/Program Files (x86)/Windows Kits/10")
  if(DEFINED ENV{WindowsSdkVerBinPath})
    list(APPEND _hints "$ENV{WindowsSdkVerBinPath}/x64")
  endif()
  foreach(_k IN LISTS _kits)
    if(CMAKE_VS_WINDOWS_TARGET_PLATFORM_VERSION)
      list(APPEND _hints "${_k}/bin/${CMAKE_VS_WINDOWS_TARGET_PLATFORM_VERSION}/x64")
    endif()
    file(GLOB _vers LIST_DIRECTORIES true "${_k}/bin/10.*")
    list(SORT _vers COMPARE NATURAL ORDER DESCENDING)
    foreach(_v IN LISTS _vers)
      list(APPEND _hints "${_v}/x64")
    endforeach()
    list(APPEND _hints "${_k}/bin/x64")
  endforeach()
  foreach(_t CMAKE_RC_COMPILER CMAKE_MT)
    if(${_t})
      get_filename_component(_d "${${_t}}" DIRECTORY)
      list(APPEND _hints "${_d}")
    endif()
  endforeach()
  find_program(FREEFRAME_FXC NAMES fxc fxc.exe HINTS ${_hints})
  if(NOT FREEFRAME_FXC)
    message(FATAL_ERROR "fxc.exe not found. Install the Windows 10/11 SDK or pass -DFREEFRAME_FXC=<path>.")
  endif()
  message(STATUS "fxc: ${FREEFRAME_FXC}")
endfunction()

# freeframe_add_compute_shader(<target> <hlsl> <var-name> [DEFINES a=b ...])
# Adds ${CMAKE_CURRENT_BINARY_DIR}/shaders/<var-name>.h (const BYTE <var-name>[]) to <target>.
function(freeframe_add_compute_shader target hlsl var)
  cmake_parse_arguments(ARG "" "" "DEFINES" ${ARGN})
  _freeframe_find_fxc()
  set(out "${CMAKE_CURRENT_BINARY_DIR}/shaders/${var}.h")
  set(defs "")
  foreach(d IN LISTS ARG_DEFINES)
    list(APPEND defs "/D" "${d}")
  endforeach()
  add_custom_command(
    OUTPUT "${out}"
    COMMAND "${CMAKE_COMMAND}" -E make_directory "${CMAKE_CURRENT_BINARY_DIR}/shaders"
    COMMAND "${FREEFRAME_FXC}" /nologo /T cs_5_0 /E main /O3 /WX
            /Vn ${var} /Fh "${out}"
            /I "${FREEFRAME_FSR_INCLUDE_DIR}"
            ${defs} "${hlsl}"
    DEPENDS "${hlsl}" "${FREEFRAME_FSR_INCLUDE_DIR}/ffx_a.h" "${FREEFRAME_FSR_INCLUDE_DIR}/ffx_fsr1.h"
    COMMENT "fxc ${var}"
    VERBATIM)
  target_sources(${target} PRIVATE "${out}")
endfunction()
