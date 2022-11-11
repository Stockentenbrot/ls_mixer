Project (demo C)
cmake_minimum_required(VERSION 3.1)

SET (CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} ${CMAKE_CURRENT_SOURCE_DIR}/Modules)

if(UNIX)
    link_libraries(m)
endif(UNIX)
  
set (
SOURCES 
${CMAKE_CURRENT_SOURCE_DIR}/ls_mixer.c
${CMAKE_CURRENT_SOURCE_DIR}/liir.c
${CMAKE_CURRENT_SOURCE_DIR}/cmixer.c
${CMAKE_CURRENT_SOURCE_DIR}/stb_vorbis.c
${CMAKE_CURRENT_SOURCE_DIR}/demo.c
)

Find_Package ( SDL2 REQUIRED )
include_directories(${SDL2_INCLUDE_DIR})
set(LIBS ${LIBS} ${SDL2_LIBRARY})

if( NOT CMAKE_BUILD_TYPE )
  set( CMAKE_BUILD_TYPE Debug CACHE STRING
       "Choose the type of build, options are: None Debug Release RelWithDebInfoMinSizeRel."
       FORCE )
endif()

set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} ${CMAKE_C_FLAGS_DEBUG} -Og")


SET(EXECUTABLE_OUTPUT_PATH ${CMAKE_CURRENT_SOURCE_DIR})



add_executable (
   demo
   WIN32 # Only if you don't want the DOS prompt to appear in the background in Windows
   MACOSX_BUNDLE
   ${SOURCES} # We could've listed the source files here directly instead of using a variable to store them
)

target_link_libraries (demo ${LIBS})
