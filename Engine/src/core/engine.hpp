#pragma once

#include "SingularityEngine_export.h"
#include <unordered_map>
#include <scene/camera.hpp>
#include <engine_core.h>
#include <RHI\swapchain.hpp>
#include "window.hpp"
#include "string"
#include "utils/memory.hpp"

namespace SE
{
	// Forward declarations
	struct MeshAsset;
	struct LoadedGLTF;
	struct Renderer;

	// Vulkan engine class
	class Engine {
	public:

		SINGULARITY_API static Engine& getInstance() {
			SE_INIT_ALLOC();
			static Engine instance;
			return instance;
		}
		SINGULARITY_API void create(uint32_t width, uint32_t height);
		SINGULARITY_API void run();
		SINGULARITY_API void shutdown();

		const std::string& getAssetPath() const { return m_AssetPath; }
		const std::string& getShaderPath() const { return m_ShaderPath; }
		Window& getWindow() { return *m_Window; }
		Editor& getEditor() { return *m_Editor; }
		Renderer& getRenderer() { return *m_Renderer; }
		Camera& getCamera() { return *m_Camera; }
	private:
		Engine() = default;
		~Engine() = default;

		Engine(const Engine&) = delete;
		Engine(Engine&&) = delete;
		Engine& operator=(const Engine&) = delete;
		Engine& operator=(Engine&&) = delete;
		void handleEvent(const SDL_Event& event, const Uint8* state);
	private:
		Scoped<Renderer> m_Renderer;
		Scoped<Editor> m_Editor;
		Scoped<Window> m_Window;
		bool m_StopRendering = false;

		//World
		Scoped<Camera> m_Camera;
		std::unordered_map<std::string, Shared<LoadedGLTF>> m_LoadedNodes;
		std::string m_AssetPath;
		std::string m_ShaderPath;
	private:
	};
}