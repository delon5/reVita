cmake_minimum_required(VERSION 2.8)

if(NOT DEFINED CMAKE_TOOLCHAIN_FILE)
  if(DEFINED ENV{VITASDK})
    set(CMAKE_TOOLCHAIN_FILE "$ENV{VITASDK}/share/vita.toolchain.cmake" CACHE PATH "toolchain file")
  else()
    message(FATAL_ERROR "Please define VITASDK to point to your SDK path!")
  endif()
endif()

# Verbose kernel log (ksceDebugPrintf at every LOG() site). Off by default: it
# adds a 256-byte stack buffer and formatting code to every call site.
option(REVITA_LOG_DEBUG "Enable verbose kernel debug logging" OFF)
if(REVITA_LOG_DEBUG)
  add_definitions(-DLOG_DEBUG)
endif()
# add_definitions(-DLOG_DISC)

project(reVita)
add_subdirectory(reVita)
add_subdirectory(reVitaMotion)
add_subdirectory(reVitaHeadless)
