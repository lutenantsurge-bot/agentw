cmake_minimum_required(VERSION 3.14)
project(llama-box-agent C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)

# ============================================================================
# Options
# ============================================================================
option(LLAMA_ALL_WARNINGS   "llama: enable all compiler warnings" ON)
option(LLAMA_FATAL_WARNINGS "llama: enable -Werror flag"          OFF)
option(WITH_WHISPER         "Enable whisper.cpp voice input"      OFF)
option(AUDIO_TERMUX         "Use Termux audio backend"            OFF)
option(AUDIO_OPENSL         "Use OpenSL ES audio backend"         OFF)
option(WITH_ROBOT           "Enable SimpleSerial robot control"   OFF)
option(WITH_IPCAM           "Enable IP Webcam frame grabber"      OFF)

# ============================================================================
# llama.cpp build flags — must be set BEFORE add_subdirectory
# ============================================================================
set(LLAMA_BUILD_COMMON   ON  CACHE BOOL "" FORCE)
set(LLAMA_BUILD_TESTS    OFF CACHE BOOL "" FORCE)
set(LLAMA_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(LLAMA_BUILD_SERVER   ON CACHE BOOL "" FORCE)
set(LLAMA_CURL           OFF CACHE BOOL "" FORCE)
set(LLAMA_LLGUIDANCE     OFF CACHE BOOL "" FORCE)

# ============================================================================
# Paths
# ============================================================================
set(LLAMA_SOURCE_DIR    "${CMAKE_CURRENT_SOURCE_DIR}/llama.cpp")
set(WHISPER_INCLUDE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/whisper.cpp/include")

# ============================================================================
# llama.cpp
# ============================================================================
add_subdirectory(${LLAMA_SOURCE_DIR} llama-build)
set_target_properties(llama PROPERTIES POSITION_INDEPENDENT_CODE ON)
set_target_properties(ggml  PROPERTIES POSITION_INDEPENDENT_CODE ON)

# ============================================================================
# whisper.cpp — use its own cmake so WHISPER_VERSION gets generated
# must be before agent so whisper target exists when agent links it
# ============================================================================
if(WITH_WHISPER)
    set(WHISPER_BUILD_TESTS    OFF CACHE BOOL "" FORCE)
    set(WHISPER_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
    add_subdirectory(whisper.cpp)
    message(STATUS "Whisper: enabled")
endif()

# ============================================================================
# agent library — defined BEFORE any target_link_libraries referencing it
# ============================================================================
add_library(agent STATIC
    src/agent.cpp
    whisper_input.cpp
    robot_tools.cpp
)
target_include_directories(agent PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/src>
    $<BUILD_INTERFACE:${LLAMA_SOURCE_DIR}/common>
    $<BUILD_INTERFACE:${LLAMA_SOURCE_DIR}/ggml/include>
    $<BUILD_INTERFACE:${LLAMA_SOURCE_DIR}/include>
    $<BUILD_INTERFACE:${LLAMA_SOURCE_DIR}/vendor>
)
target_link_libraries(agent PRIVATE common ggml llama)
target_compile_features(agent PUBLIC cxx_std_17)

if(WITH_WHISPER)
    target_include_directories(agent PUBLIC ${WHISPER_INCLUDE_DIR})
    target_compile_definitions(agent PRIVATE WITH_WHISPER AUDIO_ALSA)
    target_link_libraries(agent PRIVATE whisper)
endif()

# ============================================================================
# model library
# ============================================================================
add_library(model STATIC src/model.cpp)
target_include_directories(model PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/src>
    $<BUILD_INTERFACE:${LLAMA_SOURCE_DIR}/common>
    $<BUILD_INTERFACE:${LLAMA_SOURCE_DIR}/ggml/include>
    $<BUILD_INTERFACE:${LLAMA_SOURCE_DIR}/include>
    $<BUILD_INTERFACE:${LLAMA_SOURCE_DIR}/vendor>
)
target_link_libraries(model PRIVATE common ggml llama)
target_compile_features(model PUBLIC cxx_std_17)

if(WITH_WHISPER)
    target_link_libraries(model PRIVATE whisper)
endif()

# ============================================================================
# Main executable
# ============================================================================
add_executable(llama-box-agent main.cpp)
target_link_libraries(llama-box-agent PRIVATE agent model common ggml llama)
target_compile_features(llama-box-agent PRIVATE cxx_std_17)

if(WITH_WHISPER)
    target_link_libraries(llama-box-agent PRIVATE whisper)
    find_library(ALSA_LIB asound REQUIRED)
    target_link_libraries(llama-box-agent PRIVATE ${ALSA_LIB})
endif()

# ============================================================================
# Robot module
# ============================================================================
if(WITH_ROBOT)
    target_sources(llama-box-agent PRIVATE serial_robot.cpp)
    target_compile_definitions(llama-box-agent PRIVATE WITH_ROBOT)
    target_include_directories(llama-box-agent PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
    message(STATUS "Building with robot control enabled")
endif()

# ============================================================================
# IP Webcam module
# ============================================================================
if(WITH_IPCAM)
    target_sources(llama-box-agent PRIVATE ipcam.cpp)
    target_compile_definitions(llama-box-agent PRIVATE WITH_IPCAM)
    target_include_directories(llama-box-agent PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
    find_package(Threads REQUIRED)
    target_link_libraries(llama-box-agent PRIVATE Threads::Threads)
    message(STATUS "Building with IP Webcam enabled")
endif()
