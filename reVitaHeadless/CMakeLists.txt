cmake_minimum_required(VERSION 2.8)

if(NOT DEFINED CMAKE_TOOLCHAIN_FILE)
  if(DEFINED ENV{VITASDK})
    set(CMAKE_TOOLCHAIN_FILE "$ENV{VITASDK}/share/vita.toolchain.cmake" CACHE PATH "toolchain file")
  else()
    message(FATAL_ERROR "Please define VITASDK to point to your SDK path!")
  endif()
endif()

project(reVitaHeadless)
include("$ENV{VITASDK}/share/vita.cmake" REQUIRED)

# -Os instead of -O3: the module text is resident kernel RAM and nothing here is
# performance critical (measured: -O3 118 KB, -O2 98 KB, -Os 80 KB of text).
# Do not add --gc-sections: it discards the exported syscall functions and
# module_stop, and vita-elf-create then fails to create the SELF.
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wl,-q -Wall -Os -std=gnu99")
set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -nostdlib -Wl,--defsym=__sce_headroom=0x1000")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11 -fno-rtti -fno-exceptions")

add_definitions(-DRELEASE) 

link_directories(
  ${CMAKE_CURRENT_BINARY_DIR}
)

add_executable(${PROJECT_NAME}
  src/main.c
  src/vitasdkext.c
  src/common.c
  src/remap.c
  src/sysactions.c
  src/revita.c
  src/fio/ini.c
  src/fio/fio.c
  src/fio/profile.c
  src/fio/theme.c
  src/fio/settings.c
  src/fio/hotkeys.c
  src/log.c
)

target_link_libraries(${PROJECT_NAME}
  k
  gcc
  taihenForKernel_stub
  taihenModuleUtils_stub
  SceThreadmgrForDriver_stub
  SceSysmemForDriver_stub
  SceSysmemForKernel_stub
  SceSysrootForDriver_stub.a
  SceSysrootForKernel_stub
  SceSblACMgrForDriver_stub
  SceAppMgrForDriver_stub
  SceIofilemgrForDriver_stub
  SceCtrlForDriver_stub
  SceTouchForDriver_stub
  SceDisplayForDriver_stub 
  ScePowerForDriver_stub 
	SceRegMgrForDriver_stub 
  SceDebugForDriver_stub
  ds34vita_stub_weak
)

set_target_properties(${PROJECT_NAME}
	PROPERTIES LINK_FLAGS "-nostdlib"
	COMPILE_FLAGS "-D__VITA_KERNEL__"
)

vita_create_self(${PROJECT_NAME}.skprx ${PROJECT_NAME}
  UNSAFE
  CONFIG export.yml
)
# add_dependencies(${PROJECT_NAME}.skprx ${PROJECT_NAME})
 
# vita_create_stubs(stubs ${PROJECT_NAME} export.yml KERNEL)

# install(DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/stubs/
#   DESTINATION lib
#   FILES_MATCHING PATTERN "*.a"
# )
