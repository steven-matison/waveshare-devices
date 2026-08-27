file(GLOB_RECURSE SRCS "${CMAKE_CURRENT_LIST_DIR}/*.cpp" "${CMAKE_CURRENT_LIST_DIR}/*.c")
set(EMBEDDED_FILES "")

idf_component_register(
    SRCS ${SRCS}
    INCLUDE_DIRS "."
    EMBED_TXTFILES ${EMBEDDED_FILES}
    PRIV_REQUIRES microfi_agent agent_status_tile
)

# --- Tuna Street runtime apps (#216) -----------------------------------------
# The four tunastreet.* packages are staged into ../littlefs/apps by the build
# itself, through the same upstream hook the stock brookesia.general.* apps use
# (brookesia_stage_runtime_app_package, system_core/cmake/runtime_app_stage.cmake).
#
# Why this must be a build step and not a hand copy: that function's
# cleanup_runtime_stage_root() does a REMOVE_RECURSE of the whole apps stage
# root once per configure, then each registered package copies itself back in.
# An app that is only ever hand-copied into ../littlefs/apps therefore survives
# exactly until the next CMake configure and then silently vanishes from the
# image -- which is how the board ended up running a build older than the repo.
# Registering here makes a clean `setup.sh` produce a complete image.
#
# TUNASTREET_APPS_DIR is set by platform/setup.sh (-D, so it persists in the
# cache for later bare `idf.py build`s); the HOME fallback covers a manual
# build in the usual checkout location.
#
# It is a CMake LIST of app roots (';'-separated), searched in order for each
# package id. Since 2026-08-27 the tunastreet.* apps live in their per-app
# leader repos (TunaStreetTest/amoled-<app>, cloned as ~/amoled-<app>/apps/),
# not in waveshare-devices/amoled-1.8-v2/apps/ (which keeps only the
# tunastreet.hello template and the tunastarlink.* copies) -- so one root is no
# longer enough. setup.sh passes the repo apps dir plus the four leader clones.
if(NOT DEFINED TUNASTREET_APPS_DIR OR "${TUNASTREET_APPS_DIR}" STREQUAL "")
    if(DEFINED ENV{TUNASTREET_APPS_DIR})
        set(TUNASTREET_APPS_DIR "$ENV{TUNASTREET_APPS_DIR}")
    else()
        set(TUNASTREET_APPS_DIR
            "$ENV{HOME}/waveshare-devices/amoled-1.8-v2/apps"
            "$ENV{HOME}/amoled-agent/apps"
            "$ENV{HOME}/amoled-racing/apps"
            "$ENV{HOME}/amoled-tminus/apps"
            "$ENV{HOME}/amoled-x-viewer/apps")
    endif()
endif()
set(tunastreet_app_roots "")
foreach(tunastreet_root IN LISTS TUNASTREET_APPS_DIR)
    get_filename_component(tunastreet_root "${tunastreet_root}" ABSOLUTE)
    list(APPEND tunastreet_app_roots "${tunastreet_root}")
endforeach()

# Generic default = tuna-street's four apps. apply_profile.py (#260) rewrites
# this list per-board from profiles/<name>/profile.json (e.g. cloudera stages
# only agent + racing) -- do not hardcode a single board's app set here.
set(TUNASTREET_APP_PACKAGES
    tunastreet.agent
    tunastreet.racing
    tunastreet.tminus
    tunastreet.xviewer
)

idf_build_get_property(build_components BUILD_COMPONENTS)
if("espressif__brookesia_system_core" IN_LIST build_components)
    idf_component_get_property(tunastreet_system_core_dir espressif__brookesia_system_core COMPONENT_DIR)
else()
    idf_component_get_property(tunastreet_system_core_dir brookesia_system_core COMPONENT_DIR)
endif()
include("${tunastreet_system_core_dir}/cmake/runtime_app_stage.cmake")
brookesia_system_core_get_esp_runtime_app_stage_root(tunastreet_stage_root)

foreach(tunastreet_pkg IN LISTS TUNASTREET_APP_PACKAGES)
    set(tunastreet_pkg_dir "")
    foreach(tunastreet_root IN LISTS tunastreet_app_roots)
        if(EXISTS "${tunastreet_root}/${tunastreet_pkg}/manifest.json")
            set(tunastreet_pkg_dir "${tunastreet_root}/${tunastreet_pkg}")
            break()
        endif()
    endforeach()
    # Fail the configure rather than quietly shipping an image that is missing
    # an app: a launcher with a tile gone is the symptom this issue is about.
    if("${tunastreet_pkg_dir}" STREQUAL "")
        message(FATAL_ERROR
            "Runtime app package '${tunastreet_pkg}' not found (no manifest.json) in any "
            "app root: ${tunastreet_app_roots}. Point TUNASTREET_APPS_DIR at the roots that "
            "hold it (the per-app leader clones ~/amoled-<app>/apps and/or "
            "waveshare-devices/amoled-1.8-v2/apps) -- see platform/setup.sh.")
    endif()
    message(STATUS "Tuna Street app ${tunastreet_pkg} <- ${tunastreet_pkg_dir}")
    brookesia_stage_runtime_app_package(
        PACKAGE_ID "${tunastreet_pkg}"
        SOURCE_DIR "${tunastreet_pkg_dir}"
        STAGE_ROOT "${tunastreet_stage_root}"
        NO_INDEX
    )
endforeach()
message(STATUS "Tuna Street apps staged: ${TUNASTREET_APP_PACKAGES}")
# -----------------------------------------------------------------------------

# --- Tuna Street sound clips (#191 PlayAudio) --------------------------------
# Small wav/mp3 clips the PlayAudio processor can play by file URL
# (file://littlefs/sounds/<name>) when the array hands the board a local path
# instead of an http(s) URL. Copied at configure time into ../littlefs/sounds,
# a sibling of apps/ and system/, so the partition image carries them. Unlike
# the apps stage root, nothing wipes this directory between configures.
if(NOT DEFINED TUNASTREET_SOUNDS_DIR OR "${TUNASTREET_SOUNDS_DIR}" STREQUAL "")
    # The clips live in waveshare-devices next to its apps/ -- the first root.
    list(GET tunastreet_app_roots 0 tunastreet_first_root)
    set(TUNASTREET_SOUNDS_DIR "${tunastreet_first_root}/../sounds")
endif()
get_filename_component(TUNASTREET_SOUNDS_DIR "${TUNASTREET_SOUNDS_DIR}" ABSOLUTE)
if(IS_DIRECTORY "${TUNASTREET_SOUNDS_DIR}")
    file(GLOB tunastreet_sound_files "${TUNASTREET_SOUNDS_DIR}/*.wav" "${TUNASTREET_SOUNDS_DIR}/*.mp3")
    file(MAKE_DIRECTORY "${CMAKE_CURRENT_LIST_DIR}/../littlefs/sounds")
    file(COPY ${tunastreet_sound_files} DESTINATION "${CMAKE_CURRENT_LIST_DIR}/../littlefs/sounds")
    message(STATUS "Tuna Street sounds staged from ${TUNASTREET_SOUNDS_DIR}: ${tunastreet_sound_files}")
endif()

get_property(brookesia_runtime_app_stage_targets GLOBAL PROPERTY BROOKESIA_RUNTIME_APP_STAGE_TARGETS)
if(brookesia_runtime_app_stage_targets)
    list(REMOVE_DUPLICATES brookesia_runtime_app_stage_targets)
endif()
get_property(brookesia_system_super_stage_targets GLOBAL PROPERTY BROOKESIA_SYSTEM_SUPER_STAGE_TARGETS)
if(brookesia_system_super_stage_targets)
    list(REMOVE_DUPLICATES brookesia_system_super_stage_targets)
endif()

# `littlefs_create_partition_image` is only defined when the joltwallet/littlefs
# component is pulled into the build. That dependency is conditional (see
# brookesia_hal_adaptor: it requires the storage device + littlefs Kconfig options),
# so guard the call to avoid a hard configure error when littlefs is absent.
if(COMMAND littlefs_create_partition_image)
    littlefs_create_partition_image(
        littlefs_data
        ../littlefs
        FLASH_IN_PROJECT
        DEPENDS ${brookesia_runtime_app_stage_targets} ${brookesia_system_super_stage_targets}
    )
else()
    message(WARNING
        "littlefs_create_partition_image is not available (joltwallet/littlefs not in build); "
        "skipping littlefs_data image generation. Enable "
        "BROOKESIA_HAL_ADAPTOR_ENABLE_STORAGE_DEVICE / *_FILE_SYSTEM_IMPL / *_ENABLE_LITTLEFS "
        "if the littlefs partition is needed.")
endif()
