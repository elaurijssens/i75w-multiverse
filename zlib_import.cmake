include(FetchContent)
set(ZLIB_BUILD_SHARED OFF CACHE BOOL "Build shared libraries" FORCE)
set(ZLIB_BUILD_STATIC ON CACHE BOOL "Build shared libraries" FORCE)
set(ZLIB_INSTALL OFF CACHE BOOL "Build shared libraries" FORCE)
set(ZLIB_BUILD_MINIZIP OFF CACHE BOOL "Build shared libraries" FORCE)
set(ZLIB_BUILD_TESTING OFF CACHE BOOL "Build shared libraries" FORCE)

FetchContent_Declare(
        zlib_git
        GIT_REPOSITORY https://github.com/madler/zlib.git
        GIT_TAG        5a82f71ed1dfc0bec044d9702463dbdf84ea3b71
)
FetchContent_MakeAvailable(zlib_git)

set(ZLIB_PATH ${zlib_git_SOURCE_DIR})

include_directories(
        ${ZLIB_PATH}/
        ${CMAKE_CURRENT_BINARY_DIR}/
)