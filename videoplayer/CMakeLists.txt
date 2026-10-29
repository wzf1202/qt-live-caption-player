cmake_minimum_required(VERSION 3.21)
project(videoplayer VERSION 0.1 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

# 播放后端：qmediaplayer | ffmpegplayer
set(PLAYER_BACKEND "qmediaplayer" CACHE STRING "Player backend")
set_property(CACHE PLAYER_BACKEND PROPERTY STRINGS qmediaplayer ffmpegplayer)

option(ENABLE_SHERPA_SUBTITLE "Realtime ASR via sherpa-onnx Online (qmediaplayer only)" ON)

find_package(Qt6 REQUIRED COMPONENTS Widgets Multimedia MultimediaWidgets LinguistTools)

set(APP_SOURCES
    src/main.cpp
    src/controller/videoplayercontroller.cpp
    src/ui/videoplayermainwindow.cpp
    src/player/playerfactory.cpp
)

set(APP_HEADERS
    src/controller/videoplayercontroller.h
    src/ui/videoplayermainwindow.h
    src/util/fileutil.h
    src/player/imediaplayer.h
    src/player/playerfactory.h
)

set(SHERPA_SUBTITLE_ENABLED FALSE)
if(PLAYER_BACKEND STREQUAL "qmediaplayer" AND ENABLE_SHERPA_SUBTITLE)
    set(SHERPA_SUBTITLE_ENABLED TRUE)
endif()

if(SHERPA_SUBTITLE_ENABLED)
    list(APPEND APP_SOURCES
        src/subtitle/realtimesubtitleservice.cpp
    )
    list(APPEND APP_HEADERS
        src/subtitle/realtimesubtitleservice.h
    )
endif()

if(PLAYER_BACKEND STREQUAL "ffmpegplayer")
    list(APPEND APP_SOURCES
        src/player/ffmpeg/ffmpegplayer.cpp
        src/player/ffmpeg/session/playsession.cpp
        src/player/ffmpeg/session/videosyncer.cpp
        src/player/ffmpeg/session/audiosession.cpp
        src/player/ffmpeg/worker/demuxer.cpp
        src/player/ffmpeg/worker/videodecoder.cpp
        src/player/ffmpeg/worker/audiodecoder.cpp
        src/player/ffmpeg/queue/avpacketqueue.cpp
        src/player/ffmpeg/queue/videoframequeue.cpp
        src/player/ffmpeg/queue/audioringbuffer.cpp
    )
    list(APPEND APP_HEADERS
        src/player/ffmpeg/ffmpegplayer.h
        src/player/ffmpeg/session/playsession.h
        src/player/ffmpeg/session/videosyncer.h
        src/player/ffmpeg/session/audiosession.h
        src/player/ffmpeg/worker/demuxer.h
        src/player/ffmpeg/worker/videodecoder.h
        src/player/ffmpeg/worker/audiodecoder.h
        src/player/ffmpeg/queue/avpacketqueue.h
        src/player/ffmpeg/queue/videoframequeue.h
        src/player/ffmpeg/queue/audioringbuffer.h
        src/player/ffmpeg/avcontext.h
    )
elseif(PLAYER_BACKEND STREQUAL "qmediaplayer")
    list(APPEND APP_SOURCES src/player/qmedia/qmediaplayerbackend.cpp)
    list(APPEND APP_HEADERS src/player/qmedia/qmediaplayerbackend.h)
else()
    message(FATAL_ERROR "Unknown PLAYER_BACKEND=${PLAYER_BACKEND}")
endif()

if(SHERPA_SUBTITLE_ENABLED)
    set(SHERPA_ONNX_DIR "${CMAKE_CURRENT_SOURCE_DIR}/thirdparty/sherpa-onnx")
    # Qt 使用动态 CRT（/MD）；与 sherpa 对齐，避免 /MT 冲突
    set(SHERPA_ONNX_USE_STATIC_CRT OFF CACHE BOOL "" FORCE)
    if(MSVC AND NOT DEFINED CMAKE_MSVC_RUNTIME_LIBRARY)
        set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>DLL")
    endif()
    set(SHERPA_ONNX_ENABLE_PYTHON OFF CACHE BOOL "" FORCE)
    set(SHERPA_ONNX_ENABLE_TESTS OFF CACHE BOOL "" FORCE)
    set(SHERPA_ONNX_ENABLE_CHECK OFF CACHE BOOL "" FORCE)
    set(SHERPA_ONNX_ENABLE_PORTAUDIO OFF CACHE BOOL "" FORCE)
    set(SHERPA_ONNX_ENABLE_WEBSOCKET OFF CACHE BOOL "" FORCE)
    set(SHERPA_ONNX_ENABLE_TTS OFF CACHE BOOL "" FORCE)
    set(SHERPA_ONNX_ENABLE_SPEAKER_DIARIZATION OFF CACHE BOOL "" FORCE)
    set(SHERPA_ONNX_ENABLE_BINARY OFF CACHE BOOL "" FORCE)
    set(SHERPA_ONNX_BUILD_C_API_EXAMPLES OFF CACHE BOOL "" FORCE)
    set(SHERPA_ONNX_ENABLE_C_API ON CACHE BOOL "" FORCE)
    set(SHERPA_ONNX_ENABLE_GPU OFF CACHE BOOL "" FORCE)
    set(BUILD_SHARED_LIBS OFF CACHE BOOL "" FORCE)

    if(NOT EXISTS "${SHERPA_ONNX_DIR}/CMakeLists.txt")
        include(FetchContent)
        FetchContent_Declare(
            sherpaonnx
            GIT_REPOSITORY https://github.com/k2-fsa/sherpa-onnx.git
            GIT_TAG v1.13.7
            SOURCE_DIR "${SHERPA_ONNX_DIR}"
        )
        FetchContent_MakeAvailable(sherpaonnx)
    else()
        add_subdirectory("${SHERPA_ONNX_DIR}" "${CMAKE_BINARY_DIR}/sherpa-onnx")
    endif()
endif()

qt_add_executable(videoplayer
    ${APP_SOURCES}
    ${APP_HEADERS}
)

# sherpa 会改写全局 RUNTIME_OUTPUT_DIRECTORY，固定 exe 输出到构建根目录
set_target_properties(videoplayer PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}"
    RUNTIME_OUTPUT_DIRECTORY_DEBUG "${CMAKE_BINARY_DIR}"
    RUNTIME_OUTPUT_DIRECTORY_RELEASE "${CMAKE_BINARY_DIR}"
    RUNTIME_OUTPUT_DIRECTORY_RELWITHDEBINFO "${CMAKE_BINARY_DIR}"
    RUNTIME_OUTPUT_DIRECTORY_MINSIZEREL "${CMAKE_BINARY_DIR}"
)

target_include_directories(videoplayer PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${CMAKE_CURRENT_SOURCE_DIR}/src/ui
    ${CMAKE_CURRENT_SOURCE_DIR}/src/controller
    ${CMAKE_CURRENT_SOURCE_DIR}/src/util
    ${CMAKE_CURRENT_SOURCE_DIR}/src/player
)

if(SHERPA_SUBTITLE_ENABLED)
    target_include_directories(videoplayer PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src/subtitle
    )
endif()

target_link_libraries(videoplayer PRIVATE
    Qt6::Widgets
    Qt6::Multimedia
    Qt6::MultimediaWidgets
)

if(PLAYER_BACKEND STREQUAL "ffmpegplayer")
    target_compile_definitions(videoplayer PRIVATE FFMPEGPLAYER)
    target_include_directories(videoplayer PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src/player/ffmpeg
        ${CMAKE_CURRENT_SOURCE_DIR}/src/player/ffmpeg/worker
        ${CMAKE_CURRENT_SOURCE_DIR}/src/player/ffmpeg/queue
        ${CMAKE_CURRENT_SOURCE_DIR}/src/player/ffmpeg/session
        ${CMAKE_CURRENT_SOURCE_DIR}/thirdparty/ffmpeg/include
    )
    target_link_directories(videoplayer PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/thirdparty/ffmpeg/lib
    )
    target_link_libraries(videoplayer PRIVATE
        avformat avcodec avfilter avutil swscale swresample
    )

    if(WIN32)
        add_custom_command(TARGET videoplayer POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E make_directory $<TARGET_FILE_DIR:videoplayer>
            COMMAND ${CMAKE_COMMAND} -E copy_directory
                ${CMAKE_CURRENT_SOURCE_DIR}/thirdparty/ffmpeg/bin
                $<TARGET_FILE_DIR:videoplayer>
        )
    endif()
else()
    target_compile_definitions(videoplayer PRIVATE QMEDIAPLAYER)
    target_include_directories(videoplayer PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src/player/qmedia
    )
endif()

if(SHERPA_SUBTITLE_ENABLED)
    target_compile_definitions(videoplayer PRIVATE SHERPA_SUBTITLE)
    target_link_libraries(videoplayer PRIVATE
        sherpa-onnx-cxx-api
        sherpa-onnx-c-api
    )

    if(WIN32)
        set(_sherpa_runtime_copy_cmds "")
        if(TARGET onnxruntime)
            list(APPEND _sherpa_runtime_copy_cmds
                COMMAND ${CMAKE_COMMAND} -E copy_if_different
                    $<TARGET_FILE:onnxruntime>
                    $<TARGET_FILE_DIR:videoplayer>
            )
        endif()
        # 构建时拷贝 ${CMAKE_BINARY_DIR}/bin 下的 DLL（onnxruntime 等）
        list(APPEND _sherpa_runtime_copy_cmds
            COMMAND ${CMAKE_COMMAND}
                -D_SRC_DIR=${CMAKE_BINARY_DIR}/bin
                -D_DST_DIR=$<TARGET_FILE_DIR:videoplayer>
                -P ${CMAKE_CURRENT_SOURCE_DIR}/cmake/copy_runtime_dlls.cmake
        )
        if(DEFINED ONNXRUNTIME_DIR)
            list(APPEND _sherpa_runtime_copy_cmds
                COMMAND ${CMAKE_COMMAND}
                    -D_SRC_DIR=${ONNXRUNTIME_DIR}/lib
                    -D_DST_DIR=$<TARGET_FILE_DIR:videoplayer>
                    -P ${CMAKE_CURRENT_SOURCE_DIR}/cmake/copy_runtime_dlls.cmake
                COMMAND ${CMAKE_COMMAND}
                    -D_SRC_DIR=${ONNXRUNTIME_DIR}/bin
                    -D_DST_DIR=$<TARGET_FILE_DIR:videoplayer>
                    -P ${CMAKE_CURRENT_SOURCE_DIR}/cmake/copy_runtime_dlls.cmake
            )
        endif()
        add_custom_command(TARGET videoplayer POST_BUILD
            ${_sherpa_runtime_copy_cmds}
            COMMENT "Copy sherpa/onnxruntime runtime next to videoplayer"
        )
    endif()

    set(SHERPA_MODEL_DIR_NAME "sherpa-onnx-streaming-zipformer-bilingual-zh-en-2023-02-20")
    set(SHERPA_MODEL_SRC "${CMAKE_CURRENT_SOURCE_DIR}/thirdparty/models/${SHERPA_MODEL_DIR_NAME}")
    if(EXISTS "${SHERPA_MODEL_SRC}/tokens.txt")
        add_custom_command(TARGET videoplayer POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E make_directory
                $<TARGET_FILE_DIR:videoplayer>/models/${SHERPA_MODEL_DIR_NAME}
            COMMAND ${CMAKE_COMMAND} -E copy_directory
                "${SHERPA_MODEL_SRC}"
                $<TARGET_FILE_DIR:videoplayer>/models/${SHERPA_MODEL_DIR_NAME}
            COMMENT "Copy sherpa Zipformer model next to videoplayer"
        )
    else()
        message(WARNING "Sherpa model not found at ${SHERPA_MODEL_SRC}. "
            "Download sherpa-onnx-streaming-zipformer-bilingual-zh-en-2023-02-20 into thirdparty/models/ "
            "or set VIDEOPLAYER_SHERPA_MODEL_DIR.")
    endif()
endif()

# 翻译（对应 .pro 里的 lrelease + embed_translations）
set(TS_FILES mui/zh_CN/videoplayer.ts)
qt_add_translations(videoplayer TS_FILES ${TS_FILES})

# Windows 部署 Qt DLL（可选但实用）
if(WIN32)
    add_custom_command(TARGET videoplayer POST_BUILD
        COMMAND Qt6::windeployqt
            --no-translations
            $<TARGET_FILE:videoplayer>
        COMMENT "Running windeployqt"
    )
endif()
