include(FetchContent)
set(ZLIB_BUILD_SHARED OFF CACHE BOOL "Build shared libraries" FORCE)
FetchContent_Declare(
        zlib_git
        GIT_REPOSITORY https://github.com/madler/zlib.git
        GIT_TAG        5a82f71ed1dfc0bec044d9702463dbdf84ea3b71
)
FetchContent_MakeAvailable(zlib_git)
