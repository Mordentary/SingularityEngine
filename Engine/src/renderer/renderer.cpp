#include"renderer.hpp"
#include"core/engine.hpp"
#include"shader_compiler.hpp"

using namespace rhi;
namespace SE
{
	Renderer::Renderer()
	{
		Engine::getInstance().getWindow().WindowResizeSignal.connect(&Renderer::onWindowResize, this);
	}
	Renderer::~Renderer()
	{
		Engine::getInstance().getWindow().WindowResizeSignal.disconnect(&Renderer::onWindowResize, this);
	}

	void Renderer::createDevice(rhi::RenderBackend backend, void* window_handle, uint32_t window_width, uint32_t window_height)
	{
		m_WindowSize.x = window_width;
		m_WindowSize.y = window_height;
		m_RenderTargetSize.x = window_width;
		m_RenderTargetSize.y = window_height;

		rhi::DeviceDescription desc;
		desc.backend = backend;
		desc.windowHandle = window_handle;

#ifdef _DEBUG
		desc.enableValidation = true;
#else
		desc.enableValidation = false;
#endif
		auto device = rhi::createDevice(desc);
		SE_ASSERT(device, "Device creation is failed");
		m_Device = std::move(device);

		SE_ASSERT(m_Device, "Device is null");

		rhi::SwapchainDescription swapchainDesc;
		swapchainDesc.windowHandle = window_handle;
		swapchainDesc.width = window_width;
		swapchainDesc.height = window_height;
		swapchainDesc.bufferCount = 3;
		swapchainDesc.format = rhi::Format::R8G8B8A8_UNORM;
		swapchainDesc.vsync = true;

		m_Swapchain.reset(m_Device->createSwapchain(swapchainDesc, "MainSwapchain"));

		//ShaderDescription shaderDesc{};
		//shaderDesc.type = ShaderType::Pixel;
		//Shader* shader = m_Device->createShader();
		initFrameResources();

		compiler = new ShaderCompiler(this);

		std::vector<uint8_t> vsBinary;
		bool vsSuccess = compiler->compile(
			"triangleShader.hlsl",       // Path to HLSL file
			"VSMain",            // Entry point for Vertex Shader
			rhi::ShaderType::Vertex,
			{},                   // Defines
			vsBinary              // Output binary
		);

		if (!vsSuccess) {
			// Handle compilation failure for Vertex Shader
		}

		std::vector<uint8_t> psBinary;
		bool psSuccess = compiler->compile(
			"triangleShader.hlsl",       // Path to HLSL file
			"PSMain",            // Entry point for Vertex Shader
			rhi::ShaderType::Pixel,
			{},                   // Defines
			psBinary              // Output binary
		);

		if (!psSuccess) {
			// Handle compilation failure for Pixel Shader
		}

		ShaderDescription shaderDesc{};
		shaderDesc.type = ShaderType::Vertex;
		shaderDesc.file = "shader.hlsl";
		shaderDesc.entryPoint = "VSMain";
		Shader* shaderVS = m_Device->createShader(shaderDesc, vsBinary, "TestShaderVS");
		shaderDesc.type = ShaderType::Pixel;
		shaderDesc.entryPoint = "PSMain";
		Shader* shaderPS = m_Device->createShader(shaderDesc, psBinary, "TestShaderPS");

		GraphicsPipelineDescription pipeDesc{};
		pipeDesc.vertexShader = shaderVS;
		pipeDesc.pixelShader = shaderPS;
		pipeDesc.renderTargetFormat[0] = Format::R8G8B8A8_UNORM;
		pipeDesc.depthStencilFormat = Format::D24_UNORM_S8_UINT;
		m_DefaultPipeline = m_Device->createGraphicsPipelineState(pipeDesc, "TestGraphicsPipeline");
	}
	void Renderer::createRenderTarget(uint32_t renderWidth, uint32_t renderHeight)
	{
		Engine::getInstance().getEditor().ViewportResizeSignal.connect(&Renderer::onViewportResize, this);
		m_RenderTargetSize.x = renderWidth;
		m_RenderTargetSize.y = renderHeight;
		rhi::TextureDescription textAttachDesc{};
		textAttachDesc.usage = TextureUsageFlags::RenderTarget;

		textAttachDesc.format = Format::R16G16B16A16_UNORM;
		textAttachDesc.width = renderWidth;
		textAttachDesc.height = renderHeight;
		textAttachDesc.depth = 1;
		m_RenderTargetColor.reset(m_Device->createTexture(textAttachDesc, "MainRenderTarget:Color"));
		textAttachDesc.usage = TextureUsageFlags::DepthStencil;
		textAttachDesc.format = Format::D24_UNORM_S8_UINT;
		m_RenderTargetDepth.reset(m_Device->createTexture(textAttachDesc, "MainRenderTarget:Depth"));
	}
	void Renderer::renderFrame()
	{
		beginFrame();
		uploadResources();
		render();
		endFrame();
	}
	void SE::Renderer::onWindowResize(uint32_t width, uint32_t height)
	{
		waitForPreviousFrame();
		m_WindowSize = glm::vec2(width, height);
		m_Swapchain->resize(width, height);
	}
	void SE::Renderer::onViewportResize(uint32_t width, uint32_t height)
	{
		//createRenderTarget(width, height);
	}

	void Renderer::waitForPreviousFrame()
	{
		if (m_FrameFence)
		{
			m_FrameFence->wait(m_CurrenFrameFenceValue);
		}
	}

	void Renderer::copyToBackBuffer(rhi::CommandList* commandList)
	{
		m_Swapchain->acquireNextImage();
		//commandList->textureBarrier(m_RenderTargetColor.get(), ResourceAccessFlags::Discard, ResourceAccessFlags::RenderTarget);

		Texture* presentImage = m_Swapchain->getCurrentSwapchainImage();
		commandList->textureBarrier(presentImage, ResourceAccessFlags::Present, ResourceAccessFlags::RenderTarget);
		rhi::RenderPassDescription renderPass;
		renderPass.color[0].texture = presentImage;
		renderPass.color[0].loadOp = RenderPassLoadOp::DontCare;
		//renderPass.depth.texture = m_RenderTargetDepth.get();
		//renderPass.depth.loadOp = RenderPassLoadOp::DontCare;
		//renderPass.depth.stencilLoadOp = RenderPassLoadOp::DontCare;
		//renderPass.depth.storeOp = RenderPassStoreOp::DontCare;
		//renderPass.depth.stencilStoreOp = RenderPassStoreOp::DontCare;
		//renderPass.depth.readOnly = false;
		commandList->beginRenderPass(renderPass);
		commandList->bindPipeline(m_DefaultPipeline);
		commandList->draw(3, 1);
		commandList->endRenderPass();

		//renderPass.color[0].texture = presentImage;
		//renderPass.color[0].loadOp = RenderPassLoadOp::DontCare;
		//renderPass.depth.texture = nullptr;
		commandList->beginRenderPass(renderPass);

		Engine::getInstance().getEditor().render(commandList);

		commandList->endRenderPass();
		commandList->textureBarrier(presentImage, ResourceAccessFlags::RenderTarget, ResourceAccessFlags::Present);
	}

	void Renderer::initFrameResources()
	{
		for (auto& frame : m_FrameResources)
		{
			frame.commandList.reset(m_Device->createCommandList(rhi::CommandType::Graphics, "MainCommands"));
			frame.computeCommandList.reset(m_Device->createCommandList(rhi::CommandType::Compute, "ComputeCommands"));
			frame.uploadCommandList.reset(m_Device->createCommandList(rhi::CommandType::Copy, "UploadCommands"));
			frame.stagingBufferAllocator = CreateScoped<StagingBufferAllocator>(this);
		}
		m_FrameFence.reset(m_Device->createFence("FrameFence"));
		m_UploadFence.reset(m_Device->createFence("UploadFence"));
	}

	void SE::Renderer::beginFrame()
	{
		uint32_t frameIndex = m_Device->getFrameID() % SE_MAX_FRAMES_IN_FLIGHT;
		FrameResources& frame = m_FrameResources[frameIndex];
		m_FrameFence->wait(frame.frameFenceValue);

		m_Device->beginFrame();

		rhi::CommandList* pCommandList = frame.commandList.get();
		pCommandList->resetAllocator();
		pCommandList->begin();

		rhi::CommandList* pComputeCommandList = frame.computeCommandList.get();
		pComputeCommandList->resetAllocator();
		pComputeCommandList->begin();
	}

	void SE::Renderer::endFrame()
	{
		uint32_t frameIndex = m_Device->getFrameID() % SE_MAX_FRAMES_IN_FLIGHT;
		FrameResources& frame = m_FrameResources[frameIndex];

		rhi::CommandList* pComputeCommandList = frame.computeCommandList.get();
		pComputeCommandList->end();

		rhi::CommandList* pCommandList = frame.commandList.get();
		pCommandList->end();

		frame.frameFenceValue = ++m_CurrenFrameFenceValue;

		pCommandList->present(m_Swapchain.get());
		pCommandList->signal(m_FrameFence.get(), m_CurrenFrameFenceValue);
		pCommandList->submit();

		m_Device->endFrame();
	}

	void SE::Renderer::uploadResources()
	{
		if (m_PendingTextureUploads.empty() && m_PendingBufferUpload.empty())
		{
			return;
		}

		uint32_t frame_index = m_Device->getFrameID() % SE_MAX_FRAMES_IN_FLIGHT;

		FrameResources& currentFrame = m_FrameResources[frame_index];
		rhi::CommandList* uploadCommandList = currentFrame.uploadCommandList.get();
		uploadCommandList->resetAllocator();
		uploadCommandList->begin();

		{
			for (size_t i = 0; i < m_PendingBufferUpload.size(); ++i)
			{
				const BufferUpload& upload = m_PendingBufferUpload[i];
				uploadCommandList->copyBuffer(upload.buffer, upload.offset,
					upload.staging_buffer.buffer, upload.staging_buffer.offset, upload.staging_buffer.size);
			}

			for (size_t i = 0; i < m_PendingTextureUploads.size(); ++i)
			{
				const TextureUpload& upload = m_PendingTextureUploads[i];
				uploadCommandList->copyBufferToTexture(upload.texture, upload.mip_level, upload.array_slice,
					upload.staging_buffer.buffer, upload.staging_buffer.offset + upload.offset);
			}
		}

		uploadCommandList->end();
		uploadCommandList->signal(m_UploadFence.get(), ++m_CurrentUploadFenceValue);
		uploadCommandList->submit();

		CommandList* commandList = currentFrame.commandList.get();
		commandList->wait(m_UploadFence.get(), m_CurrentUploadFenceValue);

		if (m_Device->getDescription().backend == rhi::RenderBackend::Vulkan)
		{
			for (size_t i = 0; i < m_PendingTextureUploads.size(); ++i)
			{
				const TextureUpload& upload = m_PendingTextureUploads[i];
				commandList->textureBarrier(upload.texture,
					rhi::ResourceAccessFlags::TransferDst, rhi::ResourceAccessFlags::ShaderRead);
			}
		}

		m_PendingBufferUpload.clear();
		m_PendingTextureUploads.clear();
	}
	void SE::Renderer::render()
	{
		uint32_t frameIndex = m_Device->getFrameID() % SE_MAX_FRAMES_IN_FLIGHT;
		FrameResources& frame = m_FrameResources[frameIndex];
		CommandList* commandList = frame.commandList.get();

		copyToBackBuffer(commandList);
	}
}