# OpenGLCode/ 根目录：资源与库路径（configure_file 注入 root_directory.h）
get_filename_component(GAME_EIGEN_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/.." ABSOLUTE)

configure_file(
  "${CMAKE_CURRENT_SOURCE_DIR}/root_directory.in.h"
  "${CMAKE_CURRENT_BINARY_DIR}/root_directory.h"
  @ONLY
)

# GLAD 为 C 源文件：单独静态库可确保 MSVC/CMake 一定编译并链接（避免仅 CXX 工程漏链 glad_gl*）
add_library(glad STATIC glad.c)
target_include_directories(glad PUBLIC "${GAME_EIGEN_ROOT}/includes")
set_source_files_properties(glad.c PROPERTIES LANGUAGE C)

set(GAME_EIGEN_ENGINE_SOURCES
    core/GameWindow.cpp
    engine/Engine.cpp
    engine/physics/Collision.cpp
    engine/physics/PhysicsWorld.cpp
    engine/physics/CharacterController.cpp
    engine/lighting/LightingSystem.cpp
    rendering/RigidMesh.cpp
    stb_image.cpp)

set(GAME_EIGEN_VOXEL_ONLY_SOURCES
    engine/physics/VoxelCollider.cpp
    engine/physics/CharacterController_voxel.cpp
    engine/physics/PhysicsWorld_tick_voxel.cpp
    engine/lighting/LightingSystem_voxel.cpp)

set(GAME_EIGEN_VOXEL_SOURCES
    ${GAME_EIGEN_VOXEL_ONLY_SOURCES}
    game/GameAPI.cpp
    game/SceneComposer.cpp
    voxel/Chunk.cpp
    voxel/VoxelWorld.cpp
    app/VoxelApplication.cpp
    rendering/SceneryRenderer.cpp
    rendering/SmokePlumeRenderer.cpp
    rendering/ScaledBlockRenderer.cpp
    rendering/VoxelAtlas.cpp)

set(MESHCRAFT_ONLY_SOURCES engine/physics/CharacterController_mesh.cpp)

set(MESHCRAFT_SOURCES
    ${MESHCRAFT_ONLY_SOURCES}
    engine/physics/MeshTerrainCollider.cpp
    game/meshcraft/TerrainMesh.cpp
    game/meshcraft/MeshBuildingWorld.cpp
    game/meshcraft/MeshCraftWorld.cpp
    app/MeshCraftApplication.cpp
    rendering/MeshSceneRenderer.cpp
    rendering/VoxelAtlas.cpp)

function(gameeigen_configure_target target_name)
  target_compile_definitions(
    ${target_name}
    PRIVATE NOMINMAX
            "SHADER_ROOT=\"${CMAKE_CURRENT_SOURCE_DIR}/shaders\"")
  target_include_directories(
    ${target_name}
    PRIVATE "${GAME_EIGEN_ROOT}/includes" "${CMAKE_CURRENT_SOURCE_DIR}"
            "${CMAKE_CURRENT_BINARY_DIR}")
  target_link_directories(${target_name} PRIVATE "${GAME_EIGEN_ROOT}/lib")
  target_link_libraries(${target_name} PRIVATE glad opengl32 glfw3)
  if(MSVC)
    target_compile_options(${target_name} PRIVATE /utf-8 /W3)
  endif()
  set_target_properties(
    ${target_name}
    PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${GAME_EIGEN_ROOT}/bin"
               RUNTIME_OUTPUT_DIRECTORY_DEBUG "${GAME_EIGEN_ROOT}/bin"
               RUNTIME_OUTPUT_DIRECTORY_RELEASE "${GAME_EIGEN_ROOT}/bin"
               RUNTIME_OUTPUT_DIRECTORY_MINSIZEREL "${GAME_EIGEN_ROOT}/bin"
               RUNTIME_OUTPUT_DIRECTORY_RELWITHDEBINFO "${GAME_EIGEN_ROOT}/bin")
endfunction()

# 体素村落
add_executable(GameEigen main_voxel.cpp ${GAME_EIGEN_ENGINE_SOURCES} ${GAME_EIGEN_VOXEL_SOURCES})
gameeigen_configure_target(GameEigen)

# Mesh 地形建造（独立 exe）
add_executable(
  MeshCraft
  main_meshcraft.cpp
  ${GAME_EIGEN_ENGINE_SOURCES}
  engine/physics/PhysicsWorld_tick_stub.cpp
  ${MESHCRAFT_SOURCES})
gameeigen_configure_target(MeshCraft)
if(MSVC)
  set_target_properties(MeshCraft PROPERTIES OUTPUT_NAME "MeshCraft")
endif()
