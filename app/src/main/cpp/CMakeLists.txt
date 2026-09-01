cmake_minimum_required(VERSION 3.22.1)
project(sloppatv LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if(NOT DEFINED SLOPPATV_VERSION_NAME)
    set(SLOPPATV_VERSION_NAME "dev")
endif()

add_library(sloppatv SHARED
    ${ANDROID_NDK}/sources/android/native_app_glue/android_native_app_glue.c
    main.cpp
    discovery.cpp
    device_capabilities.cpp
    display_mode.cpp
    renderer.cpp
    jni_http.cpp
    jellyfin.cpp
    image_decoder.cpp
    media_player.cpp
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

target_link_libraries(sloppatv PRIVATE
    android
    log
    jnigraphics
    EGL
    GLESv3
)
