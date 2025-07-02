# Framework library

add_library(glad STATIC
        ${PROJECT_SOURCE_DIR}/extern/glad/glad/glad.c
)
target_include_directories(glad PUBLIC ${PROJECT_SOURCE_DIR}/extern/glad)

add_executable(sample_framework
  application.h
  application.cc
  imgui.h
  image.h
  image.cc
  mesh.h
  mesh.cc
  motion_utils.h
  motion_utils.cc
  profile.h
  profile.cc
  renderer.h
  utils.h
  utils.cc
  internal/camera.h
  internal/camera.cc
  internal/icosphere.h
  internal/immediate.h
  internal/immediate.cc
  internal/imgui_impl.h
  internal/imgui_impl.cc
  internal/renderer_impl.h
  internal/renderer_impl.cc
  internal/shader.h
  internal/shader.cc
  internal/shooter.h
  internal/shooter.cc
)


# Samples requires OpenGL package.
if(NOT EMSCRIPTEN)
  find_package(SDL2 REQUIRED)
  target_link_libraries(sample_framework SDL2::SDL2 SDL2::SDL2main glad)
  target_include_directories(sample_framework PUBLIC )
else()
  target_link_options(sample_framework PUBLIC -sUSE_SDL=2 -sMIN_WEBGL_VERSION=2 -sMAX_WEBGL_VERSION=2)
endif()

target_link_libraries(sample_framework
  ozz_geometry
  ozz_animation_offline
  ozz_options)
  
target_include_directories(sample_framework PUBLIC
  ${PROJECT_SOURCE_DIR}/samples
  ${PROJECT_SOURCE_DIR}/extern/glad
  )

if(TARGET BUILD_DATA_SAMPLE)
  add_dependencies(sample_framework BUILD_DATA_SAMPLE)
endif()

set_target_properties(sample_framework PROPERTIES FOLDER "samples")

add_subdirectory(tools)
