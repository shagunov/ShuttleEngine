cmake_minimum_required(VERSION 3.20)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include(FetchContent)

FetchContent_Declare(
		Assimp
		GIT_REPOSITORY https://github.com/assimp/assimp
		SOURCE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/external/assimp
		CMAKE_ARGS
			-DASSIMP_NO_WERROR=ON
			-DASSIMP_BUILD_ZLIB=ON
)

FetchContent_MakeAvailable(Assimp)

FetchContent_Declare(
		ConcurrentQueue
		GIT_REPOSITORY https://github.com/cameron314/concurrentqueue
		SOURCE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/external/concurrentqueue
)

FetchContent_Declare(
		imgui
		GIT_REPOSITORY https://github.com/ocornut/imgui
		SOURCE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/external/imgui
)

FetchContent_MakeAvailable(imgui)

FetchContent_Declare(
		stdexec
		GIT_REPOSITORY https://github.com/nvidia/stdexec
		GIT_TAG        main # Или укажите конкретный стабильный тег/коммит
		SOURCE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/external/stdexec
)
FetchContent_MakeAvailable(stdexec)

message(STATUS "ImGui source dir: ${imgui_SOURCE_DIR}" )

set(IMGUI_SOURCES
		${imgui_SOURCE_DIR}/imgui.cpp
		${imgui_SOURCE_DIR}/imgui_draw.cpp
		${imgui_SOURCE_DIR}/imgui_widgets.cpp
		${imgui_SOURCE_DIR}/imgui_tables.cpp
		${imgui_SOURCE_DIR}/imgui_demo.cpp            # Для тестирования
		${imgui_SOURCE_DIR}/backends/imgui_impl_sdl2.cpp   # Бэкэнд для SDL2
		${imgui_SOURCE_DIR}/backends/imgui_impl_vulkan.cpp
		src/Painters/SunLightControlPanel/SunLightControlPanel.cpp
		src/Painters/SunLightControlPanel/SunLightControlPanel.hpp
		src/FrameManager/FrameManager.cpp
		src/FrameManager/FrameManager.hpp
		src/SwapchainFactory/SwapchainFactory.cpp
		src/SwapchainFactory/SwapchainFactory.hpp
		src/RetireController/RetireController.cpp
		src/RetireController/RetireController.hpp
        src/Input/Command.hpp
		src/Input/Command.hpp
		src/Input/InputFocus.hpp
		src/Input/ActiveMask.hpp
		src/Input/ModularInputSettings.hpp
		src/Input/CommandBuffer.hpp
		src/Input/InputTranslator.hpp
		src/Input/ApplicationController.hpp
		src/Input/CommandExecutor.hpp
		src/Engine/Engine.cpp
		src/Engine/Engine.hpp
		src/Painters/DebugOverlay/DebugOverlay.hpp
		src/RenderMode/RenderMode.hpp
		src/EditorRenderMode/EditorRenderMode.cpp
		src/EditorRenderMode/EditorRenderMode.hpp
		src/Scene/SceneGraph.cpp
		src/Scene/SceneGraph.hpp
		src/Rendering/Render/IRenderPass.hpp
		src/Rendering/Render/RenderGraph.cpp
		src/Rendering/Render/RenderGraph.hpp
		src/Rendering/Render/RenderPass/ShadowRenderPass.cpp
		src/Rendering/Render/RenderPass/ShadowRenderPass.hpp
		src/memory/StagingBufferController/StagingBufferController.cpp
		src/memory/StagingBufferController/StagingBufferController.hpp
		src/Assets/NormalMipGenerator/NormalMipGenerator.cpp
		src/Assets/NormalMipGenerator/NormalMipGenerator.hpp
		src/Assets/GeneralMipGenerator/GeneralMipGenerator.cpp
		src/Assets/GeneralMipGenerator/GeneralMipGenerator.hpp
		src/Assets/TextureManager/TextureManager.cpp
		src/Assets/TextureManager/TextureManager.hpp
		src/Assets/GeometryManager/GeometryManager.cpp
		src/Assets/GeometryManager/GeometryManager.hpp
		src/Assets/GeometryStore/GeometryStore.cpp
		src/Assets/GeometryStore/GeometryStore.hpp
		src/assets/MaterialStore/MaterialStore.cpp
		src/assets/MaterialStore/MaterialStore.hpp
		src/Assets/AssetManager/AssetManager.cpp
		src/Assets/AssetManager/AssetManager.hpp
		src/Assets/TextureStore/TextureStore.cpp
		src/Assets/TextureStore/TextureStore.hpp
		src/assets/SceneStores/SceneStores.cpp
		src/assets/SceneStores/SceneStores.hpp
		src/HandleManager/HandleManager.cpp
		src/HandleManager/HandleManager.hpp
		src/Memory/DeviceAllocator/DeviceAllocator.hpp
		src/execution/QueueExecutionChannel/QueueExecutionChannel.cpp
		src/execution/QueueExecutionChannel/QueueExecutionChannel.hpp
		src/execution/Executor.hpp
		src/execution/FenceWaiter/FenceWaiter.cpp
		src/execution/FenceWaiter/FenceWaiter.hpp
		src/execution/QueueExecutionController/QueueExecutionController.cpp
		src/execution/QueueExecutionController/QueueExecutionController.hpp
		src/assets/HostImage/HostImage.hpp
		src/assets/HostImage/HostImage.hpp
		src/memory/VirtualBlock/VirtualBlock.cpp
		src/memory/VirtualBlock/VirtualBlock.hpp
        src/assets/HostImage/HostImageLoader.cpp
        src/assets/HostImage/HostImageLoader.hpp
		src/assets/HostImage/OrmPacker.cpp
		src/assets/HostImage/OrmPacker.hpp
		src/assets/TextureLoader/TextureLoader.cpp
		src/assets/TextureLoader/TextureLoader.hpp
		src/assets/TextureStore/Texture.hpp
		src/assets/MaterialStore/Material.hpp
		src/assets/MeshStore/MeshInfo.hpp
		src/assets/MeshStore/MeshStorage.cpp
		src/assets/MeshStore/MeshStorage.hpp
		src/assets/NodeStore/NodeStore.cpp
		src/assets/NodeStore/NodeStore.hpp
		src/assets/TransformStore/TransformStore.cpp
		src/assets/TransformStore/TransformStore.hpp
)

FetchContent_MakeAvailable(ConcurrentQueue)

FetchContent_declare(
		Mimalloc
		GIT_REPOSITORY https://github.com/microsoft/mimalloc main
		GIT_TAG main
		SOURCE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/external/mimalloc
)

FetchContent_MakeAvailable(Mimalloc)

#TaskFlow
FetchContent_Declare(
		TaskFlow
		GIT_REPOSITORY https://github.com/taskflow/taskflow
		SOURCE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/external/taskflow
)

FetchContent_MakeAvailable(TaskFlow)

#vk-bootstrap
FetchContent_Declare(
		VkBootstrap
		GIT_REPOSITORY https://github.com/charles-lunarg/vk-bootstrap
		GIT_TAG main
		SOURCE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/external/vk-bootstrap
)

FetchContent_MakeAvailable(VkBootstrap)

FetchContent_Declare(
		meshoptimizer
		GIT_REPOSITORY https://github.com/zeux/meshoptimizer.git
		SOURCE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/external/meshoptimizer
)

FetchContent_MakeAvailable(meshoptimizer)

project(HelloTriangle)

add_executable(HelloTriangle
	"src/Sdl/SdlLibrary/SdlLibrary.cpp" 
	"src/Sdl/SdlWindow/SdlWindow.cpp"
	"src/VulkanDebugger/VulkanDebugger.cpp"
	"src/ImageLoader/Image.hpp"
	"src/ImageLoader/Image.cpp"
	"src/VulkanHelperFunctions/VulkanHelperFunctions.cpp"
	"src/Camera/Camera.cpp"
	"src/Skybox/Skybox.cpp"
	src/Terrain/Terrain.cpp
	src/Terrain/Terrain.hpp
	src/Terrain/Terrain.hpp
	src/PbrRender/Render.cpp
	src/PbrRender/Render.hpp
		src/Assets/AssimpLoader/AssimpLoader.cpp
		src/Assets/AssimpLoader/AssimpLoader.hpp
	src/HostRenderData/HostRenderData.cpp
		src/Memory/DeviceAllocator/DeviceAllocator.cpp
	src/PbrRender/RenderInit.cpp
	src/PbrRender/PrepareMaterial.cpp
	src/CameraController/CameraController.cpp
	src/CameraController/CameraController.hpp
	main.cpp
	src/PbrRender/PrepareMesh.cpp
        src/UiRender/UiRender.cpp
        src/UiRender/UiRender.hpp
		${IMGUI_SOURCES}
		src/Rendering/Render/RenderPass/PbrMainRenderPass.cpp
		src/Rendering/Render/RenderPass/PbrMainRenderPass.hpp
)

find_package(Vulkan REQUIRED)
message("Vulkan include directories: ${Vulkan_INCLUDE_DIRS}")
set(SDL2Libraries
		${Vulkan_INCLUDE_DIRS}/../Lib/SDL2main.lib
		${Vulkan_INCLUDE_DIRS}/../Lib/SDL2.lib)

target_include_directories(HelloTriangle PRIVATE ${Vulkan_INCLUDE_DIRS})

target_compile_definitions(HelloTriangle
	PRIVATE
		IMGUI_IMPL_VULKAN_NO_PROTOTYPES
		IMGUI_IMPL_VULKAN_HAS_EXTENSION
		IMGUI_IMPL_VULKAN_USE_RUNTIME_BINDINGS
		IMGUI_DISABLE_OBSOLETE_FUNCTIONS # Тоже полезно

		GLM_FORCE_DEPTH_ZERO_TO_ONE
		GLM_ENABLE_EXPERIMENTAL
		VULKAN_HPP_HANDLE_ERROR_OUT_OF_DATE_AS_SUCCESS


)

target_link_libraries(HelloTriangle PRIVATE meshoptimizer)
target_link_libraries(HelloTriangle PRIVATE vk-bootstrap::vk-bootstrap)
target_link_libraries(HelloTriangle PRIVATE ${SDL2Libraries})
target_link_libraries(HelloTriangle PRIVATE assimp::assimp)
target_link_libraries(HelloTriangle PRIVATE mimalloc)
target_include_directories(HelloTriangle PRIVATE "${imgui_SOURCE_DIR}")
target_include_directories(HelloTriangle PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/include")
target_include_directories(HelloTriangle PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/src")
target_include_directories(HelloTriangle SYSTEM PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/external/VMA/include")
target_include_directories(HelloTriangle SYSTEM PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/external/stb")
target_include_directories(HelloTriangle SYSTEM PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/external/assimp/include")
target_include_directories(HelloTriangle SYSTEM PRIVATE "${CMAKE_CURRENT_BINARY_DIR}/_deps/assimp-build/include")
target_include_directories(HelloTriangle SYSTEM PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/external/concurrentqueue")
target_include_directories(HelloTriangle SYSTEM PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/external/mimalloc/include")
target_include_directories(HelloTriangle SYSTEM PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/external/taskflow")
target_include_directories(HelloTriangle SYSTEM PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/external/vk-bootstrap/src")
target_include_directories(HelloTriangle SYSTEM PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/external/stdexec/include")
target_include_directories(HelloTriangle SYSTEM PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/external/meshoptimizer/src")

add_subdirectory(shaders)
add_subdirectory(img)
add_subdirectory(assets)
add_dependencies(HelloTriangle CopyAssets)
add_dependencies(HelloTriangle CompileShaders)
add_dependencies(HelloTriangle CopyCubeMapImages)
add_dependencies(HelloTriangle assimp)
add_dependencies(HelloTriangle meshoptimizer)
