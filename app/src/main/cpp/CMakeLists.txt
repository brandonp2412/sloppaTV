cmake_minimum_required(VERSION 3.22.1)
project(sloppatv LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

if(NOT DEFINED SLOPPATV_VERSION_NAME)
    set(SLOPPATV_VERSION_NAME "dev")
endif()

add_library(sloppatv SHARED
    ${ANDROID_NDK}/sources/android/native_app_glue/android_native_app_glue.c
    main.cpp
    account_completion_controller.cpp
    account_navigation_controller.cpp
    browse_navigation_controller.cpp
    details_navigation_controller.cpp
    home_navigation_controller.cpp
    queue_navigation_controller.cpp
    search_navigation_controller.cpp
    seerr_drive_navigation_controller.cpp
    settings_navigation_controller.cpp
    discovery.cpp
    device_capabilities.cpp
    display_mode.cpp
    dream_renderer.cpp
    external_player.cpp
    renderer.cpp
    jni_http.cpp
    jellyfin.cpp
    jellyfin_item_parser.cpp
    jellyfin_media_segment_parser.cpp
    launch_intent.cpp
    image_decoder.cpp
    media_player.cpp
    media_session.cpp
    playback_info.cpp
    seerr.cpp
    session_store.cpp
    task_runner.cpp
    video_surface.cpp
)

target_include_directories(sloppatv PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/third_party
    ${ANDROID_NDK}/sources/android/native_app_glue
)

target_compile_options(sloppatv PRIVATE
    $<$<COMPILE_LANGUAGE:CXX>:-Wall;-Wextra;-Wpedantic>
)

target_compile_definitions(sloppatv PRIVATE
    SLOPPATV_VERSION_NAME="${SLOPPATV_VERSION_NAME}"
)

if(SLOPPATV_BENCHMARK)
    target_compile_definitions(sloppatv PRIVATE SLOPPATV_BENCHMARK=1)
endif()

target_link_libraries(sloppatv PRIVATE
    android
    dl
    log
    jnigraphics
    EGL
    GLESv3
)
