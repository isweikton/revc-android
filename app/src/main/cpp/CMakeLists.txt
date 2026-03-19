cmake_minimum_required(VERSION 3.22.1)

project(revc_android LANGUAGES C CXX)

list(PREPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_LIST_DIR}/cmake")

set(REPO_ROOT "${CMAKE_CURRENT_LIST_DIR}/../../../../")
set(REVC_NATIVE "${REPO_ROOT}/native")
set(REVC_SRC "${REVC_NATIVE}/src")
set(REVC_VENDOR "${REVC_NATIVE}/vendor")
set(REVC_LIB "${REVC_NATIVE}/lib/android")
set(REVC_THIRD_PARTY "${REPO_ROOT}/third_party")

if(ANDROID_ABI STREQUAL "armeabi-v7a")
    set(REVC_ARCH_DIR "arm")
elseif(ANDROID_ABI STREQUAL "arm64-v8a")
    set(REVC_ARCH_DIR "aarch64")
else()
    message(FATAL_ERROR "Unsupported ABI: ${ANDROID_ABI}")
endif()

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(ANDROID_ABI STREQUAL "arm64-v8a")
    add_link_options("-Wl,-z,max-page-size=16384")
endif()

set(SDL_SHARED ON CACHE BOOL "" FORCE)
set(SDL_STATIC OFF CACHE BOOL "" FORCE)
set(SDL_TEST OFF CACHE BOOL "" FORCE)
set(SDL_TEST_LIBRARY OFF CACHE BOOL "" FORCE)
set(SDL_DISABLE_INSTALL ON CACHE BOOL "" FORCE)
add_subdirectory("${REVC_THIRD_PARTY}/SDL" "${CMAKE_CURRENT_BINARY_DIR}/SDL")

add_library(OpenAL::OpenAL SHARED IMPORTED GLOBAL)
set_target_properties(OpenAL::OpenAL PROPERTIES
    IMPORTED_LOCATION "${REVC_LIB}/${REVC_ARCH_DIR}/libopenal.so"
    INTERFACE_INCLUDE_DIRECTORIES "${REVC_VENDOR}/openal-soft/include"
)

add_library(MPG123::libmpg123 SHARED IMPORTED GLOBAL)
set_target_properties(MPG123::libmpg123 PROPERTIES
    IMPORTED_LOCATION "${REVC_LIB}/${REVC_ARCH_DIR}/libmpg123.so"
    INTERFACE_INCLUDE_DIRECTORIES "${REVC_VENDOR}/mpg123/include"
)

if(EXISTS "${REVC_LIB}/${REVC_ARCH_DIR}/libandroid_support.a")
    add_library(android_support STATIC IMPORTED GLOBAL)
    set_target_properties(android_support PROPERTIES
        IMPORTED_LOCATION "${REVC_LIB}/${REVC_ARCH_DIR}/libandroid_support.a"
    )
endif()

set(LIBRW_PLATFORM GL3 CACHE STRING "" FORCE)
set(LIBRW_GL3_GFXLIB SDL2 CACHE STRING "" FORCE)
set(LIBRW_TOOLS OFF CACHE BOOL "" FORCE)
set(LIBRW_EXAMPLES OFF CACHE BOOL "" FORCE)
set(LIBRW_INSTALL OFF CACHE BOOL "" FORCE)
add_subdirectory("${REVC_VENDOR}/librw" "${CMAKE_CURRENT_BINARY_DIR}/librw")

if(TARGET SDL2 AND ANDROID_ABI STREQUAL "arm64-v8a")
    target_link_options(SDL2 PRIVATE "-Wl,-z,max-page-size=16384")
endif()

file(GLOB_RECURSE REVC_SOURCES CONFIGURE_DEPENDS "${REVC_SRC}/*.cpp")
list(FILTER REVC_SOURCES EXCLUDE REGEX ".*/src/skel/android/demangle/.*")
list(FILTER REVC_SOURCES EXCLUDE REGEX ".*/src/audio/PoliceRadio\\.cpp$")

set(REVC_GENERATED_DIR "${CMAKE_CURRENT_BINARY_DIR}/generated")
file(MAKE_DIRECTORY "${REVC_GENERATED_DIR}")
file(WRITE "${REVC_GENERATED_DIR}/GitSHA1.cpp" "#define GIT_SHA1 \"android-studio\"\nconst char* g_GIT_SHA1 = GIT_SHA1;\n")
list(APPEND REVC_SOURCES "${REVC_GENERATED_DIR}/GitSHA1.cpp")

add_library(revc SHARED ${REVC_SOURCES})

target_include_directories(revc PRIVATE
    "${REVC_SRC}"
    "${REVC_SRC}/core"
    "${REVC_SRC}/fakerw"
    "${REVC_SRC}/skel"
    "${REVC_SRC}/renderer"
    "${REVC_SRC}/math"
    "${REVC_SRC}/rw"
    "${REVC_SRC}/control"
    "${REVC_SRC}/buildings"
    "${REVC_SRC}/animation"
    "${REVC_SRC}/objects"
    "${REVC_SRC}/entities"
    "${REVC_SRC}/modelinfo"
    "${REVC_SRC}/weapons"
    "${REVC_SRC}/collision"
    "${REVC_SRC}/peds"
    "${REVC_SRC}/vehicles"
    "${REVC_SRC}/text"
    "${REVC_SRC}/audio"
    "${REVC_VENDOR}/librw"
    "${REVC_SRC}/extras"
    "${REVC_SRC}/save"
    "${REVC_SRC}/audio/eax"
    "${REVC_SRC}/skel/android/libunwind"
    "${REVC_THIRD_PARTY}/SDL/include"
    "${REVC_VENDOR}/openal-soft/include"
    "${REVC_VENDOR}/mpg123/include"
)

target_compile_definitions(revc PRIVATE
    RW_GL3
    LIBRW_SDL2
    LIBRW
    AUDIO_OAL
    USE_OUR_VERSIONING
    ANDROID=1
    _ANDROID=1
    LINUX=1
    _LINUX=1
    POSIX=1
    _POSIX=1
    GNUC
    NO_HOOK_MALLOC
    AL_LIBTYPE_STATIC
)

target_compile_options(revc PRIVATE
    -fpermissive
    -fPIC
    -funwind-tables
    -Wall
    -Wno-reorder
    -Wno-unknown-pragmas
    -Wno-unused-function
    -Wno-unused-but-set-variable
    -Wno-unused-value
    -Wno-unused-variable
)

find_library(android-lib android)
find_library(log-lib log)
find_library(EGL-lib EGL)
find_library(GLESv3-lib GLESv3)

target_link_libraries(revc PRIVATE
    librw::librw
    SDL2::SDL2
    OpenAL::OpenAL
    MPG123::libmpg123
    ${android-lib}
    ${log-lib}
    ${EGL-lib}
    ${GLESv3-lib}
)

if(TARGET android_support)
    target_link_libraries(revc PRIVATE android_support)
endif()
