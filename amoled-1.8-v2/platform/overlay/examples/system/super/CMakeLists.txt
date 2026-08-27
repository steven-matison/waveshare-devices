# The following lines of boilerplate have to be in your project's CMakeLists
# in this exact order for cmake to work correctly

# Force Ninja to use response files to completely solve the 32KB Windows command line length limit
if("${CMAKE_GENERATOR}" MATCHES "Ninja")
    set(CMAKE_NINJA_FORCE_RESPONSE_FILE 1 CACHE INTERNAL "")
endif()

cmake_minimum_required(VERSION 3.16)

option(CCACHE_ENABLE "Use ccache to accelerate repeat ESP-IDF builds" ON)

include($ENV{IDF_PATH}/tools/cmake/project.cmake)

# Work around esp-idf-configdep mapping CONFIG_ESP_LVGL_ADAPTER_PARTIAL_AUX_IMG_CACHE
# to a Windows-invalid path containing the reserved device name "aux".
# Only force this off on Windows: elsewhere it makes idf.py's default
# `-DCONFIGDEP_ENABLE=True` permanently disagree with the forced cache value, which
# makes idf.py re-run CMake configure on every build/flash (~24s each time).
if(CMAKE_HOST_WIN32)
    set(CONFIGDEP_ENABLE 0 CACHE BOOL "Disable esp-idf-configdep for Windows AUX path compatibility" FORCE)
endif()

# MicroFi guest component (#188 Phase 4): the agent's sparkplug dependency
# lives in the MicroFi tree
list(APPEND EXTRA_COMPONENT_DIRS "/mnt/c/Users/tunas/MicroFi/components")

# Per-board build flags (#260/#261/#263): apply_profile.py generates
# main/board_profile.cmake from the selected profile as plain set() variables
# (BOARD_HIDE_FILES, BOARD_HAS_BATTERY, BOARD_HAS_AGENT). It is included HERE,
# before project(), because the component CMakeLists that project() processes
# (components/microfi_agent, components/agent_status_tile) read BOARD_HAS_AGENT
# to decide whether to build the agent or an empty stub -- a component
# CMakeLists runs via add_subdirectory and sees top-level variables, but only
# ones set before project(). OPTIONAL so a tree built without a profile applied
# still configures: every consumer treats "undefined" as the generic default
# (Files tile shown, no battery gauge, agent ON).
include("${CMAKE_CURRENT_LIST_DIR}/main/board_profile.cmake" OPTIONAL)

project(example_system_super)

# Temporary workaround for `espressif__mcp-c-sdk` stringop-truncation warning
idf_build_set_property(COMPILE_OPTIONS -Wno-error=stringop-truncation APPEND)

# The same flags for the C++ consumers, as build-wide compile definitions: the
# launcher / status bar live in the brookesia_system_super component and
# main.cpp in main/, none of which can include a header from the other, so a
# build-wide define is the one mechanism that reaches all of them. Only defined
# when the profile defined them, so the "undefined = generic" contract holds.
foreach(board_flag BOARD_HIDE_FILES BOARD_HAS_BATTERY BOARD_HAS_AGENT)
    if(DEFINED ${board_flag})
        idf_build_set_property(COMPILE_DEFINITIONS "${board_flag}=${${board_flag}}" APPEND)
    endif()
endforeach()

brookesia_system_super_apply_compile_policy()
