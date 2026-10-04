# Static module brackets (issue #457). Retail 1.32c starts qagame, cgame and
# ui from a fresh QVM image on every VM_Create and VM_Restart. This build links
# them into the application, so Sys_LoadDll restores that image itself: it
# copies back each module's initialized data, saved when the application
# starts, and zeroes the module's bss (code/qcommon/vm_static.c).
#
# quake3_static_modules(<target> <game> <cgame> <ui> [<shared>...]) links
# <target> with a linker script that cmake/static_modules.py writes at
# configure time from Retro68's default XCOFF script
# (`powerpc-apple-macos-ld --verbose`), with each module archive's data and
# bss gathered first and bracketed by q3static_<module>_{data,bss}_{start,end}.
# After the link, cmake/static_modules.py checks the linker map: every data and
# bss section of a module archive lies inside its own bracket, nothing else
# lies inside one, and no global symbol is defined by more than one module,
# shared archive or the engine. It writes <target>.static-modules.txt with the
# ranges; MakePEF waits for it, so a failed check fails the build.
#
# The <shared> archives are never reset: their code serves the engine or more
# than one module (see cmake/static_modules.py).

find_program(RETRO68_LD powerpc-apple-macos-ld
    HINTS "${RETRO68_BIN_DIR}" "${CMAKE_SOURCE_DIR}/tools/Retro68-build/bin")
find_program(RETRO68_NM powerpc-apple-macos-nm
    HINTS "${RETRO68_BIN_DIR}" "${CMAKE_SOURCE_DIR}/tools/Retro68-build/bin")
if(NOT RETRO68_LD OR NOT RETRO68_NM)
    message(FATAL_ERROR "Retro68 powerpc-apple-macos-ld and powerpc-apple-macos-nm are "
        "required to bracket the static game modules (issue #457).")
endif()

set(QUAKE3_STATIC_MODULES_TOOL "${CMAKE_CURRENT_LIST_DIR}/static_modules.py")
set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS "${QUAKE3_STATIC_MODULES_TOOL}")

set(QUAKE3_DEFAULT_LDSCRIPT "${CMAKE_BINARY_DIR}/retro68-default-ldscript.txt")
execute_process(COMMAND "${RETRO68_LD}" --verbose
    RESULT_VARIABLE status OUTPUT_FILE "${QUAKE3_DEFAULT_LDSCRIPT}" ERROR_VARIABLE error)
if(NOT status EQUAL 0)
    message(FATAL_ERROR "${RETRO68_LD} --verbose failed (${status}): ${error}")
endif()

function(quake3_static_modules target game cgame ui)
    set(script "${CMAKE_CURRENT_BINARY_DIR}/${target}.static-modules.x")
    set(modules "")
    set(check_modules "")
    foreach(pair "game;${game}" "cgame;${cgame}" "ui;${ui}")
        list(GET pair 0 name)
        list(GET pair 1 library)
        list(APPEND modules --module
            "${name}=${CMAKE_STATIC_LIBRARY_PREFIX}${library}${CMAKE_STATIC_LIBRARY_SUFFIX}")
        list(APPEND check_modules --module "${name}=$<TARGET_FILE:${library}>")
    endforeach()
    set(check_shared "")
    foreach(library ${ARGN})
        list(APPEND check_shared --shared "$<TARGET_FILE:${library}>")
    endforeach()

    execute_process(
        COMMAND "${Python3_EXECUTABLE}" "${QUAKE3_STATIC_MODULES_TOOL}" ldscript
                --default "${QUAKE3_DEFAULT_LDSCRIPT}" --ld "${RETRO68_LD}"
                --output "${script}" ${modules}
        RESULT_VARIABLE status ERROR_VARIABLE error)
    if(NOT status EQUAL 0)
        message(FATAL_ERROR "Cannot write the static module linker script for ${target} "
            "(issue #457):\n${error}")
    endif()

    target_link_libraries(${target} PRIVATE
        "-Wl,-T,${script}" "-Wl,-Map,${CMAKE_CURRENT_BINARY_DIR}/${target}.map")
    set_property(TARGET ${target} APPEND PROPERTY LINK_DEPENDS "${script}")
    set_property(TARGET ${target} PROPERTY QUAKE3_STATIC_MODULES_LDSCRIPT "${script}")

    add_custom_command(
        OUTPUT "${target}.static-modules.txt"
        COMMAND "${Python3_EXECUTABLE}" "${QUAKE3_STATIC_MODULES_TOOL}" check
                --map "${target}.map" --nm "${RETRO68_NM}"
                ${check_modules} ${check_shared}
                --output "${target}.static-modules.txt"
        DEPENDS ${target} "${QUAKE3_STATIC_MODULES_TOOL}"
        WORKING_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}"
        COMMENT "Checking the static module brackets of ${target}"
        VERBATIM)
    set_property(TARGET ${target} PROPERTY QUAKE3_STATIC_MODULES_REPORT
        "${CMAKE_CURRENT_BINARY_DIR}/${target}.static-modules.txt")
endfunction()
