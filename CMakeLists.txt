# 1. --- 项目基本配置 ---
cmake_minimum_required(VERSION 3.16)
project(Spartan VERSION 1.0 LANGUAGES CXX C)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(DEV_LIBS_ROOT "C:/dev_msvc_2022")
file(GLOB LIB_DIRS "${DEV_LIBS_ROOT}/*")
# 循环加到 CMAKE_PREFIX_PATH
foreach(lib_dir ${LIB_DIRS})
    if(IS_DIRECTORY "${lib_dir}")
        list(APPEND CMAKE_PREFIX_PATH "${lib_dir}")
    endif()
endforeach()

# 将当前目录添加到模块搜索路径，方便后续 include
list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")

SET(DEFINITIONS "")
ADD_DEFINITIONS(-D_CRT_SECURE_NO_WARNINGS)


if (CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
    set(CC_CLANG 1)
    SET(DEFINITIONS "${DEFINITIONS}"
            "-D__CC_CLANG=1"
            "-D__CC_GCC=0"
            "-D__CC_ICC=0"
            "-D__CC_MSVC=0")
elseif (CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
    set(CC_GCC 1)
    SET(DEFINITIONS "${DEFINITIONS}"
            "-D__CC_CLANG=0"
            "-D__CC_GCC=1"
            "-D__CC_ICC=0"
            "-D__CC_MSVC=0")
elseif (CMAKE_CXX_COMPILER_ID STREQUAL "Intel")
    set(CC_ICC 1)
    SET(DEFINITIONS "${DEFINITIONS}"
            "-D__CC_CLANG=0"
            "-D__CC_GCC=0"
            "-D__CC_ICC=1"
            "-D__CC_MSVC=0")
elseif (CMAKE_CXX_COMPILER_ID STREQUAL "MSVC")
    set(CC_MSVC 1)
    SET(DEFINITIONS "${DEFINITIONS}"
            "-D__CC_CLANG=0"
            "-D__CC_GCC=0"
            "-D__CC_ICC=0"
            "-D__CC_MSVC=1")
    add_compile_options(/utf-8)
endif ()

IF (EMSCRIPTEN)
    set(WEB 1)
ELSEIF (CMAKE_SYSTEM_NAME MATCHES "iOS")
    set(IOS 1)
ELSEIF (CMAKE_SYSTEM_NAME MATCHES "Android")
    set(ANDROID 1)
ELSEIF (CMAKE_SYSTEM_NAME MATCHES "Darwin")
    set(MACOS 1)
ELSEIF (WIN32)
    set(WINDOWS 1)
ELSEIF (CMAKE_SYSTEM_NAME MATCHES "Linux")
    set(LINUX 1)
ELSEIF (CMAKE_SYSTEM_NAME MATCHES "OHOS")
    set(OHOS 1)
ELSE ()
    MESSAGE("OS NAME UNDEFINED ")
ENDIF ()

IF (WINDOWS)
    SET(DEFINITIONS "${DEFINITIONS}"
            "-D__OS_WINDOWS=1"
            "-D__OS_ANDROID=0"
            "-D__OS_IOS=0"
            "-D__OS_MACOS=0"
            "-D__OS_LINUX=0"
            "-D__OS_OHOS=0"
            "-D__OS_WEB=0"
    )
elseif (ANDROID)
    SET(DEFINITIONS "${DEFINITIONS}"
            "-D__OS_WINDOWS=0"
            "-D__OS_ANDROID=1"
            "-D__OS_IOS=0"
            "-D__OS_MACOS=0"
            "-D__OS_LINUX=0"
            "-D__OS_OHOS=0"
            "-D__OS_WEB=0"
    )
elseif (IOS)
    SET(DEFINITIONS "${DEFINITIONS}"
            "-D__OS_WINDOWS=0"
            "-D__OS_ANDROID=0"
            "-D__OS_IOS=1"
            "-D__OS_MACOS=0"
            "-D__OS_LINUX=0"
            "-D__OS_OHOS=0"
            "-D__OS_WEB=0")
elseif (MACOS)
    SET(DEFINITIONS "${DEFINITIONS}"
            "-D__OS_WINDOWS=0"
            "-D__OS_ANDROID=0"
            "-D__OS_IOS=0"
            "-D__OS_MACOS=1"
            "-D__OS_LINUX=0"
            "-D__OS_OHOS=0"
            "-D__OS_WEB=0")
elseif (LINUX)
    SET(DEFINITIONS "${DEFINITIONS}"
            "-D__OS_WINDOWS=0"
            "-D__OS_ANDROID=0"
            "-D__OS_IOS=0"
            "-D__OS_MACOS=0"
            "-D__OS_LINUX=1"
            "-D__OS_OHOS=0"
            "-D__OS_WEB=0")
elseif (OHOS)
    SET(DEFINITIONS "${DEFINITIONS}"
            "-D__OS_WINDOWS=0"
            "-D__OS_ANDROID=0"
            "-D__OS_IOS=0"
            "-D__OS_MACOS=0"
            "-D__OS_LINUX=0"
            "-D__OS_OHOS=1"
            "-D__OS_WEB=0")
elseif (EMSCRIPTEN)
    SET(DEFINITIONS "${DEFINITIONS}"
            "-D__OS_WINDOWS=0"
            "-D__OS_ANDROID=0"
            "-D__OS_IOS=0"
            "-D__OS_MACOS=0"
            "-D__OS_LINUX=0"
            "-D__OS_OHOS=0"
            "-D__OS_WEB=1")
ENDIF ()


if(NOT CC_MSVC)
    # 为 Release 模式设置 C++ 编译器标志
    set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} -O3" CACHE STRING "Flags used by the C++ compiler for release builds." FORCE)
    # 为 Release 模式设置 C 编译器标志
    set(CMAKE_C_FLAGS_RELEASE "${CMAKE_C_FLAGS_RELEASE} -O3" CACHE STRING "Flags used by the C compiler for release builds." FORCE)

    # 为了对比，Debug 模式通常不开启优化或使用 -O0，并包含调试信息
    set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -g" CACHE STRING "Flags used by the C++ compiler for debug builds." FORCE)
    set(CMAKE_C_FLAGS_DEBUG "${CMAKE_C_FLAGS_DEBUG} -g" CACHE STRING "Flags used by the C compiler for debug builds." FORCE)
endif()
if(CC_MSVC)
    set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreadedDebug")
endif()



set(BUILD_SHARED_LIBS OFF)

set(OZZ_BUILD_TESTS OFF CACHE BOOL "Disable Ozz tests")

add_subdirectory(libs/ozz-animation)
add_subdirectory(libs/glm)

if(NOT WEB)
    find_package(SDL2 REQUIRED)
endif()
if(NOT WEB)
    set(SRC ${SRC} libs/glad/glad/glad.c
    )
endif()
set(SRC ${SRC}  ${ENGINE_SOURCES}
        src/GltfTools/GltfTools.cpp
        src/GltfTools/AssetSerializer.cpp
        src/GltfTools/GltfToolsAnimation.cpp
        src/SimpleApp.cpp
        src/RenderPipeline.cpp
        src/Renderer.cpp
        src/RenderDevice.cpp
        src/Components.cpp
        src/Systems.cpp
        src/RenderWorld.cpp
        src/AnimationTask.cpp
)


set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} -fno-omit-frame-pointer")

add_executable(Spartan ${SRC}
)
target_include_directories(Spartan PUBLIC src/v3/)
target_compile_definitions(Spartan PUBLIC ${DEFINITIONS})
if (MSVC)
    target_compile_options(Spartan PRIVATE /RTC1 /GS /sdl)
else ()
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wno-deprecated-literal-operator")
endif()
target_include_directories(Spartan PUBLIC libs/glm/ libs/tiny_gltf libs/entt-3.13.2/single_include)
#target_compile_options(Spartan PRIVATE -fsanitize=address)
#target_link_options(Spartan PRIVATE -fsanitize=address)
if(CC_MSVC)
    target_include_directories(Spartan PUBLIC
            C:/dev_msvc_2022/FBX_SDK_2020.3.7/include
    )
endif()
# PUBLIC: 任何链接到 Spartan 的目标，都会自动获得这些头文件路径
target_include_directories(Spartan PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/src

        libs/ozz-animation/include
        libs/glad
)

set(SDL "")
if(NOT WEB)
    set(SDL SDL2::SDL2 SDL2::SDL2main)
endif ()
target_link_directories(Spartan PUBLIC
        C:/dev_msvc_2022/FBX_SDK_2020.3.7/lib/x64/debug)
# 链接引擎所需的核心依赖库
target_link_libraries(Spartan PUBLIC
        ${SDL}
        ozz_animation
        ozz_base
        ozz_options
        ozz_geometry
        #        ozz_animation_fbx
        ozz_animation_offline


)

if(EMSCRIPTEN)
    target_compile_options(Spartan PRIVATE "-sUSE_SDL=2")
    target_link_options(Spartan PRIVATE
            "-sUSE_SDL=2"
            "-sUSE_WEBGL2=1"
            "-sFULL_ES3=1"
            "-sALLOW_MEMORY_GROWTH=1"
            "-sSTACK_SIZE=5MB"
            "-sMIN_WEBGL_VERSION=2"
    )
    set_target_properties(Spartan PROPERTIES
            LINK_FLAGS "--preload-file ${CMAKE_SOURCE_DIR}/media@/media"
    )
    set_target_properties(Spartan PROPERTIES SUFFIX ".html")
endif()
