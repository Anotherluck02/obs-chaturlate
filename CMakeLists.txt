cmake_minimum_required(VERSION 3.16...3.26)

include("${CMAKE_CURRENT_SOURCE_DIR}/cmake/common/bootstrap.cmake" NO_POLICY_SCOPE)

project(${_name} VERSION ${_version})

option(ENABLE_FRONTEND_API "Use obs-frontend-api for UI functionality" OFF)
option(ENABLE_QT "Use Qt functionality" OFF)
option(USE_SYSTEM_ONNXRUNTIME "Use system ONNX Runtime" OFF)
option(USE_SYSTEM_ICU "Use system ICU" OFF)

include(compilerconfig)
include(defaults)
include(helpers)

add_library(${CMAKE_PROJECT_NAME} MODULE)

find_package(libobs REQUIRED)
target_link_libraries(${CMAKE_PROJECT_NAME} PRIVATE OBS::libobs)

if(ENABLE_FRONTEND_API)
  find_package(obs-frontend-api REQUIRED)
  target_link_libraries(${CMAKE_PROJECT_NAME} PRIVATE OBS::obs-frontend-api)
endif()

if(ENABLE_QT)
  find_package(Qt6 COMPONENTS Widgets Core)
  target_link_libraries(${CMAKE_PROJECT_NAME} PRIVATE Qt6::Core Qt6::Widgets)
  target_compile_options(
    ${CMAKE_PROJECT_NAME} PRIVATE $<$<C_COMPILER_ID:Clang,AppleClang>:-Wno-quoted-include-in-framework-header
                                  -Wno-comma>)
  set_target_properties(
    ${CMAKE_PROJECT_NAME}
    PROPERTIES AUTOMOC ON
               AUTOUIC ON
               AUTORCC ON)
endif()

# Disable newline-eof warning (files may not have trailing newlines)
target_compile_options(
  ${CMAKE_PROJECT_NAME} PRIVATE $<$<CXX_COMPILER_ID:Clang,AppleClang,GNU>:-Wno-newline-eof>)

# build cURL
set(USE_SYSTEM_CURL
    OFF
    CACHE STRING "Use system cURL")
if(USE_SYSTEM_CURL)
  find_package(CURL REQUIRED)
  target_link_libraries(${CMAKE_PROJECT_NAME} PRIVATE "${CURL_LIBRARIES}")
  target_include_directories(${CMAKE_PROJECT_NAME} SYSTEM PUBLIC "${CURL_INCLUDE_DIRS}")
else()
  include(cmake/BuildMyCurl.cmake)
  target_link_libraries(${CMAKE_PROJECT_NAME} PRIVATE libcurl)
endif()

# build Whisper.cpp for STT
include(cmake/BuildWhispercpp.cmake)
target_link_libraries(${CMAKE_PROJECT_NAME} PRIVATE Whispercpp)

# build CTranslate2 and Sentencepiece for translation
include(cmake/BuildCTranslate2.cmake)
include(cmake/BuildSentencepiece.cmake)
target_link_libraries(${CMAKE_PROJECT_NAME} PRIVATE ct2 sentencepiece)

# build ONNX Runtime for VAD
if(NOT USE_SYSTEM_ONNXRUNTIME)
  include(cmake/FetchOnnxruntime.cmake)
  target_link_libraries(${CMAKE_PROJECT_NAME} PRIVATE Onnxruntime)
endif()

# build sherpa-onnx for TTS
include(cmake/BuildSherpaOnnx.cmake)
target_link_libraries(${CMAKE_PROJECT_NAME} PRIVATE sherpa-onnx)

# build libarchive
include(cmake/BuildLibarchive.cmake)
target_link_libraries(${CMAKE_PROJECT_NAME} PRIVATE libarchive)

# build ICU for Unicode support
if(USE_SYSTEM_ICU)
  find_package(
    ICU 70.1
    COMPONENTS data uc i18n
    REQUIRED)
  target_link_libraries(${CMAKE_PROJECT_NAME} PRIVATE "${ICU_LIBRARIES}")
  target_include_directories(${CMAKE_PROJECT_NAME} SYSTEM PUBLIC "${ICU_INCLUDE_DIRS}")
else()
  set(ICU_LIBRARIES "")
  find_library(ICU_DATA_LIB icudata OPTIONAL)
  find_library(ICU_UC_LIB icuuc OPTIONAL)
  find_library(ICU_I18N_LIB icui18n OPTIONAL)

  if(ICU_DATA_LIB
     AND ICU_UC_LIB
     AND ICU_I18N_LIB)
    list(APPEND ICU_LIBRARIES ${ICU_DATA_LIB} ${ICU_UC_LIB} ${ICU_I18N_LIB})
    message(STATUS "Cached ICU libs: " ${ICU_LIBRARIES})
    set(ICU_INCLUDE_DIR "${CMAKE_BINARY_DIR}/ICU_build-prefix/include")
    message(STATUS "Cached ICU include dir: " ${ICU_INCLUDE_DIR})

    target_link_libraries(${CMAKE_PROJECT_NAME} PRIVATE "${ICU_LIBRARIES}")
    target_include_directories(${CMAKE_PROJECT_NAME} SYSTEM PUBLIC ${ICU_INCLUDE_DIR})
  else()
    include(cmake/BuildICU.cmake)
    # Add ICU to the target
    target_link_libraries(${CMAKE_PROJECT_NAME} PRIVATE ICU)
    target_include_directories(${CMAKE_PROJECT_NAME} SYSTEM PUBLIC ${ICU_INCLUDE_DIR})
  endif()
endif()

target_sources(
  ${CMAKE_PROJECT_NAME}
  PRIVATE src/input-thread.cpp
          src/model-utils/model-downloader-ui.cpp
          src/model-utils/model-downloader.cpp
          src/model-utils/model-find-utils.cpp
          src/model-utils/model-infos.cpp
          src/plugin-main.c
          src/sherpa-tts/sherpa-tts.cpp
          src/audio-thread.cpp
          src/squawk-source.c
          src/squawk-source.cpp
          src/tts-utils.cpp
          # Chaturlate Filter (STT → Translation → TTS)
          src/chaturlate-filter.c
          src/chaturlate-filter.cpp
          # Whisper STT
          src/whisper-utils/whisper-processing.cpp
          src/whisper-utils/vad-processing.cpp
          src/whisper-utils/whisper-model-utils.cpp
          src/whisper-utils/whisper-params.cpp
          src/whisper-utils/whisper-utils.cpp
          src/whisper-utils/silero-vad-onnx.cpp
          src/whisper-utils/token-buffer-thread.cpp
          src/transcription-filter-callbacks.cpp
          src/transcription-utils.cpp
          # Translation
          src/translation/translation.cpp
          src/translation/language_codes.cpp
          src/translation/translation-language-utils.cpp
          src/translation/translation-utils.cpp
          # Cloud Translation
          src/translation/cloud-translation/translation-cloud.cpp
          src/translation/cloud-translation/curl-helper.cpp
          src/translation/cloud-translation/azure.cpp
          src/translation/cloud-translation/claude.cpp
          src/translation/cloud-translation/custom-api.cpp
          src/translation/cloud-translation/deepl.cpp
          src/translation/cloud-translation/google-cloud.cpp
          src/translation/cloud-translation/openai.cpp
          src/translation/cloud-translation/papago.cpp)

set_target_properties_plugin(${CMAKE_PROJECT_NAME} PROPERTIES OUTPUT_NAME ${_name})
