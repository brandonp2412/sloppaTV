cmake_minimum_required(VERSION 3.22.1)
project(sloppatv LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

if(NOT DEFINED SLOPPATV_VERSION_NAME)
    set(SLOPPATV_VERSION_NAME "dev")
endif()

# Pinned whisper.cpp; speech recognition stays on the device.
include(FetchContent)
set(BUILD_SHARED_LIBS OFF CACHE BOOL "" FORCE)
set(GGML_NATIVE OFF CACHE BOOL "" FORCE)
set(GGML_OPENMP OFF CACHE BOOL "" FORCE)
set(GGML_BACKEND_DL OFF CACHE BOOL "" FORCE)
set(WHISPER_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(WHISPER_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(WHISPER_BUILD_SERVER OFF CACHE BOOL "" FORCE)
set(WHISPER_ALL_WARNINGS OFF CACHE BOOL "" FORCE)
FetchContent_Declare(subtitle_whisper
    URL https://codeload.github.com/ggml-org/whisper.cpp/tar.gz/d1be6fde11ac6e0407606b4e42fe72d34add8037
    URL_HASH SHA256=645445da884df6dbd52b30f82d7be7c01d1767c938714e477e786b88d529e9f4
)
FetchContent_MakeAvailable(subtitle_whisper)
# Speech inference needs optimized kernels even in isolated debug builds.
foreach(speech_target whisper ggml ggml-base ggml-cpu)
    if(TARGET ${speech_target})
        get_target_property(speech_target_type ${speech_target} TYPE)
        if(NOT speech_target_type STREQUAL "INTERFACE_LIBRARY")
            target_compile_options(${speech_target} PRIVATE -O3)
        endif()
    endif()
endforeach()


add_library(sloppatv SHARED
    ${ANDROID_NDK}/sources/android/native_app_glue/android_native_app_glue.c
    main.cpp
    account_flow.cpp
    account_completion_controller.cpp
    account_navigation_controller.cpp
    browse_completion_controller.cpp
    browse_navigation_controller.cpp
    details_completion_controller.cpp
    details_navigation_controller.cpp
    details_flow.cpp
    item_mutation_controller.cpp
    content_mutation_flow.cpp
    home_completion_controller.cpp
    home_completion_flow.cpp
    home_navigation_controller.cpp
    playback_completion_controller.cpp
    playback_completion_flow.cpp
    player_completion_controller.cpp
    queue_navigation_controller.cpp
    search_completion_controller.cpp
    search_navigation_controller.cpp
    seerr_drive_navigation_controller.cpp
    server_info_completion_controller.cpp
    settings_action_controller.cpp
    settings_navigation_controller.cpp
    system_text_input_controller.cpp
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
    subtitle_audio_probe.cpp
    subtitle_speech_recognizer.cpp
    media_session.cpp
    playback_info.cpp
    seerr.cpp
    session_store.cpp
    task_runner.cpp
    video_surface.cpp
)

add_library(sloppatv_fvad STATIC
    third_party/libfvad/src/fvad.c
    third_party/libfvad/src/signal_processing/division_operations.c
    third_party/libfvad/src/signal_processing/energy.c
    third_party/libfvad/src/signal_processing/get_scaling_square.c
    third_party/libfvad/src/signal_processing/resample_48khz.c
    third_party/libfvad/src/signal_processing/resample_by_2_internal.c
    third_party/libfvad/src/signal_processing/resample_fractional.c
    third_party/libfvad/src/signal_processing/spl_inl.c
    third_party/libfvad/src/vad/vad_core.c
    third_party/libfvad/src/vad/vad_filterbank.c
    third_party/libfvad/src/vad/vad_gmm.c
    third_party/libfvad/src/vad/vad_sp.c
)
set_target_properties(sloppatv_fvad PROPERTIES POSITION_INDEPENDENT_CODE ON)
target_include_directories(sloppatv_fvad PRIVATE third_party/libfvad/src third_party/libfvad/include)

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

if(SLOPPATV_E2E_TELEMETRY)
    target_compile_definitions(sloppatv PRIVATE SLOPPATV_E2E_TELEMETRY=1)
endif()

target_link_libraries(sloppatv PRIVATE
    sloppatv_fvad
    whisper
    android
    dl
    log
    jnigraphics
    EGL
    GLESv3
)
