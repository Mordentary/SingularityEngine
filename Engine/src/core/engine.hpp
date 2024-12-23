#pragma once

#include "SingularityEngine_export.h"
#include <unordered_map>
#include <camera.h>
#include <engine_core.h>
#include <RHI\swapchain.hpp>
#include <renderer\renderer.hpp>
#include "window.hpp"
#include "string"
namespace SE
{
	// Forward declarations
	struct MeshAsset;
	struct LoadedGLTF;

	// Vulkan engine class
	class Engine {
	public:

		SINGULARITY_API static Engine& getInstance() {
			static Engine instance;
			return instance;
		}
		SINGULARITY_API void create(uint32_t width, uint32_t height);
		SINGULARITY_API void run();
		SINGULARITY_API void shutdown();

		Window& getWindow() const { return *m_Window; }
		Editor& getEditor() const { return *m_Editor; }
		Renderer& getRenderer() const { return *m_Renderer; }
	private:
		Engine() = default;
		~Engine() = default;

		Engine(const Engine&) = delete;
		Engine(Engine&&) = delete;
		Engine& operator=(const Engine&) = delete;
		Engine& operator=(Engine&&) = delete;
		void handleEvent(const SDL_Event& event);
	private:
		Scoped<Renderer> m_Renderer;

		Scoped<Editor> m_Editor;
		Scoped<Window> m_Window;
		bool m_StopRendering = false;

		//World
		std::unordered_map<std::string, Shared<LoadedGLTF>> m_LoadedNodes;
		Shared<Camera> m_Camera;
		std::string m_AssetPath;
		std::string m_ShaderPath;
	private:
	};
}