cmake_minimum_required(VERSION 3.10)
project(Quake3 C CXX ASM)

if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Release)
endif()

# Global definitions
add_definitions(-D__MACOS__ -D__POWERPC__ -DBOTLIB -DQ3_STATIC)
add_definitions(-DQUAKEDATA="/Quake3/") # Default path, adjustable

include_directories(
    ${CMAKE_SOURCE_DIR}/code/game
    ${CMAKE_SOURCE_DIR}/code/cgame
    ${CMAKE_SOURCE_DIR}/code/ui
    ${CMAKE_SOURCE_DIR}/code/client
    ${CMAKE_SOURCE_DIR}/code/server
    ${CMAKE_SOURCE_DIR}/code/renderer
    ${CMAKE_SOURCE_DIR}/code/qcommon
    ${CMAKE_SOURCE_DIR}/code/botlib
    ${CMAKE_SOURCE_DIR}/code/mac
    ${CMAKE_SOURCE_DIR}/code
    "${CMAKE_SOURCE_DIR}/OpenGL SDK 1.2/Headers"
    "${CMAKE_SOURCE_DIR}/tools/MPW/Interfaces&Libraries/Interfaces/CIncludes"
    "${CMAKE_SOURCE_DIR}/tools/Retro68-src/InterfacesAndLibraries/Interfaces/CIncludes"
    ${CMAKE_SOURCE_DIR}/code/game
)

# Source lists
file(GLOB COMMON_SRCS code/qcommon/*.c)
file(GLOB SERVER_SRCS code/server/*.c)
file(GLOB CLIENT_SRCS code/client/*.c)
file(GLOB RENDERER_SRCS code/renderer/*.c)
file(GLOB MAC_SRCS code/mac/*.c code/mac/*.r)
file(GLOB BOTLIB_SRCS code/botlib/*.c)
file(GLOB JPEG_SRCS code/jpeg-6/*.c)
file(GLOB NULL_SRCS code/null/*.c) # Might need null implementations for some things?

# Exclude incompatible/architecture-specific files
list(REMOVE_ITEM COMMON_SRCS 
    "${CMAKE_SOURCE_DIR}/code/qcommon/vm_x86.c"
    "${CMAKE_SOURCE_DIR}/code/qcommon/vm_ppc.c"
    "${CMAKE_SOURCE_DIR}/code/qcommon/vm_ppc_new.c"
    "${CMAKE_SOURCE_DIR}/code/qcommon/vm_sparc.c"
    "${CMAKE_SOURCE_DIR}/code/qcommon/vm_mips.c"
    "${CMAKE_SOURCE_DIR}/code/qcommon/vm_arm.c"
)
list(REMOVE_ITEM JPEG_SRCS
    "${CMAKE_SOURCE_DIR}/code/jpeg-6/jmemdos.c"
    "${CMAKE_SOURCE_DIR}/code/jpeg-6/jmemansi.c" # usually jmemnobs is best for games
    "${CMAKE_SOURCE_DIR}/code/jpeg-6/jmemname.c"
    "${CMAKE_SOURCE_DIR}/code/jpeg-6/jpegtran.c"
)
list(REMOVE_ITEM SERVER_SRCS "${CMAKE_SOURCE_DIR}/code/server/sv_rankings.c")

# Remove q_shared.c from COMMON_SRCS if it causes issues, but usually it's fine.
# We might need to handle 'unzip.c' carefully if zlib is required/included? 
# code/qcommon/unzip.c seems to be the implementation.

# Game Modules - Compiled with unique naming for monolithic build
file(GLOB GAME_SRCS code/game/*.c)
list(REMOVE_ITEM GAME_SRCS "${CMAKE_SOURCE_DIR}/code/game/g_rankings.c")

add_library(game_obj OBJECT ${GAME_SRCS})
target_compile_definitions(game_obj PRIVATE 
    -DvmMain=Game_vmMain 
    -DdllEntry=Game_dllEntry
    -DGAME_MODULE
)

file(GLOB CGAME_SRCS code/cgame/*.c)
list(REMOVE_ITEM CGAME_SRCS "${CMAKE_SOURCE_DIR}/code/cgame/cg_newdraw.c")

add_library(cgame_obj OBJECT ${CGAME_SRCS})
target_compile_definitions(cgame_obj PRIVATE 
    -DvmMain=CGame_vmMain 
    -DdllEntry=CGame_dllEntry
    -DCGAME_MODULE
)

file(GLOB UI_SRCS code/ui/*.c)
add_library(ui_obj OBJECT ${UI_SRCS})
target_compile_definitions(ui_obj PRIVATE 
    -DvmMain=UI_vmMain 
    -DdllEntry=UI_dllEntry
    -DUI_MODULE
)

# Mission Pack (Team Arena) Modules
add_library(game_mp_obj OBJECT ${GAME_SRCS})
target_compile_definitions(game_mp_obj PRIVATE 
    -DvmMain=Game_vmMain 
    -DdllEntry=Game_dllEntry
    -DGAME_MODULE
    -DMISSIONPACK
)

add_library(cgame_mp_obj OBJECT ${CGAME_SRCS})
target_compile_definitions(cgame_mp_obj PRIVATE 
    -DvmMain=CGame_vmMain 
    -DdllEntry=CGame_dllEntry
    -DCGAME_MODULE
    -DMISSIONPACK
)

add_library(ui_mp_obj OBJECT ${UI_SRCS})
target_compile_definitions(ui_mp_obj PRIVATE 
    -DvmMain=UI_vmMain 
    -DdllEntry=UI_dllEntry
    -DUI_MODULE
    -DMISSIONPACK
)

# Main Executable
add_executable(Quake3
    ${COMMON_SRCS}
    ${SERVER_SRCS}
    ${CLIENT_SRCS}
    ${RENDERER_SRCS}
    ${MAC_SRCS}
    ${BOTLIB_SRCS}
    ${JPEG_SRCS}
    $<TARGET_OBJECTS:game_obj>
    $<TARGET_OBJECTS:cgame_obj>
    $<TARGET_OBJECTS:ui_obj>
)

# Team Arena Executable
add_executable(Quake3_TeamArena
    ${COMMON_SRCS}
    ${SERVER_SRCS}
    ${CLIENT_SRCS}
    ${RENDERER_SRCS}
    ${MAC_SRCS}
    ${BOTLIB_SRCS}
    ${JPEG_SRCS}
    $<TARGET_OBJECTS:game_mp_obj>
    $<TARGET_OBJECTS:cgame_mp_obj>
    $<TARGET_OBJECTS:ui_mp_obj>
)

# Link libraries
# Retro68 automatically links standard libs, but check if we need m
# Add MPW Library Paths
link_directories(
    "${CMAKE_SOURCE_DIR}/tools/Retro68-src/InterfacesAndLibraries/Libraries/SharedLibraries"
    "${CMAKE_SOURCE_DIR}/tools/Retro68-src/InterfacesAndLibraries/Libraries/PPCLibraries"
    "${CMAKE_SOURCE_DIR}/tools/Retro68-src/InterfacesAndLibraries/Libraries/Libraries"
    "${CMAKE_SOURCE_DIR}/tools/Retro68-src/InterfacesAndLibraries/Libraries"
)

# Link Libraries
# Note: Retro68/GCC expects libraries to be -lName (searching libName.a) or full path.

set(MPW_LIB_DIR "${CMAKE_SOURCE_DIR}/tools/Retro68-src/InterfacesAndLibraries/Libraries/SharedLibraries")
set(OGL_LIB_DIR "${CMAKE_SOURCE_DIR}/tools/Retro68-src/InterfacesAndLibraries/Libraries")

target_link_libraries(Quake3 PUBLIC
    -lm
    -lInterfaceLib
    -lOpenTransportLib
    -lOpenTransportAppPPC
    -lOpenTptUtilsAppPPC
    -lOpenTptInternetLib
    -lCarbonLib
    -lInputSprocketLib
    -lDrawSprocketLib
    -lOpenGLLibraryStub
)

target_link_libraries(Quake3_TeamArena PUBLIC
    -lm
    -lInterfaceLib
    -lOpenTransportLib
    -lOpenTransportAppPPC
    -lOpenTptUtilsAppPPC
    -lOpenTptInternetLib
    -lCarbonLib
    -lInputSprocketLib
    -lDrawSprocketLib
    -lOpenGLLibraryStub
)

if(ENABLE_SOUND)
    target_link_libraries(Quake3 PUBLIC "${MPW_LIB_DIR}/SoundLib")
    target_link_libraries(Quake3_TeamArena PUBLIC "${MPW_LIB_DIR}/SoundLib")
endif()

# Output name
set_target_properties(Quake3 PROPERTIES OUTPUT_NAME "Quake3")
set_target_properties(Quake3_TeamArena PROPERTIES OUTPUT_NAME "Quake3_TeamArena")


# Bundle creation (Retro68 specific helpers if available, or just the bin)
# We might wan explicitly set the CREATOR and TYPE
# set_target_properties(Quake3 PROPERTIES 
#     LINK_FLAGS "-Wl,--creator,Q3A0 -Wl,--type,APPL"
# )
