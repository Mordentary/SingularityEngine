#pragma once
#include "../command_list.hpp"
#include "vulkan_core.hpp"

namespace rhi::vulkan
{
	class VulkanDevice;
	class VulkanCommandList : public CommandList {
	public:
		VulkanCommandList(VulkanDevice* device, CommandType type, const std::string& name);
		~VulkanCommandList();

		bool create();
		void* getHandle() const override { return m_CommandBuffer; }

		// Core command list operations
		void resetAllocator() override;
		void begin() override;
		void end() override;
		void wait(Fence* dstFence, uint64_t value) override;
		void signal(Fence* dstFence, uint64_t value) override;
		void present(Swapchain* dstSwapchain) override;
		void submit() override;
		void resetState() override;

		// Resource operations
		void copyBufferToTexture(Texture* dstTexture, uint32_t mipLevel, uint32_t arraySlice, Buffer* srcBuffer, uint32_t offset) override;
		void copyTextureToBuffer(Buffer* dstBuffer, uint32_t offset, Texture* srcTexture, uint32_t mipLevel, uint32_t arraySlice) override;
		void copyBuffer(Buffer* dstBuffer, uint32_t dstOffset, Buffer* srcBuffer, uint32_t srcOffset, uint32_t size) override;
		void copyTexture(Texture* dstTexture, uint32_t dstMip, uint32_t dstArray, Texture* srcTexture, uint32_t srcMip, uint32_t srcArray) override;
		void clearStorageBuffer(Resource* resource, Descriptor* storage, const float* clearValue) override;
		void clearStorageBuffer(Resource* resource, Descriptor* storage, const uint32_t* clearValue) override;
		void writeBuffer(Buffer* dstBuffer, uint32_t offset, uint32_t data) override;
		//void updateTileMappings(Texture* texture, Heap* heap, uint32_t mappingCount, const TileMapping* mappings) override;

		// Barriers
		void textureBarrier(Texture* texture, ResourceAccessFlags accessBefore, ResourceAccessFlags accessAfter) override;
		void bufferBarrier(Buffer* buffer, ResourceAccessFlags accessBefore, ResourceAccessFlags accessAfter) override;
		void globalBarrier(ResourceAccessFlags accessBefore, ResourceAccessFlags accessAfter) override;
		void flushBarriers() override;

		// Render state
		void beginRenderPass(const RenderPassDescription& renderPass) override;
		void endRenderPass() override;
		void bindPipeline(Pipeline* state) override;
		void setStencilReference(uint8_t stencil) override;
		void setBlendFactor(const float* blendFactor) override;
		void setIndexBuffer(Buffer* buffer, uint32_t offset, Format format) override;
		void setViewport(uint32_t x, uint32_t y, uint32_t width, uint32_t height) override;
		void setScissorRect(uint32_t x, uint32_t y, uint32_t width, uint32_t height) override;
		void setGraphicsPushConstants(uint32_t slot, const void* data, size_t dataSize) override;
		void setComputePushConstants(uint32_t slot, const void* data, size_t dataSize) override;

		// Draw/dispatch commands
		void draw(uint32_t vertexCount, uint32_t instanceCount = 1) override;
		void drawIndexed(uint32_t indexCount, uint32_t instanceCount = 1, uint32_t indexOffset = 0) override;
		void dispatch(uint32_t groupCountX, uint32_t groupCountY, uint32_t groupCountZ) override;
		//void dispatchMesh(uint32_t groupCountX, uint32_t groupCountY, uint32_t groupCountZ) override;

		//// Indirect commands
		//void drawIndirect(Buffer* buffer, uint32_t offset) override;
		//void drawIndexedIndirect(Buffer* buffer, uint32_t offset) override;
		//void dispatchIndirect(Buffer* buffer, uint32_t offset) override;
		//void dispatchMeshIndirect(Buffer* buffer, uint32_t offset) override;

		//// Multi-draw indirect
		//void multiDrawIndirect(uint32_t maxCount, Buffer* argsBuffer, uint32_t argsOffset, Buffer* countBuffer, uint32_t countOffset) override;
		//void multiDrawIndexedIndirect(uint32_t maxCount, Buffer* argsBuffer, uint32_t argsOffset, Buffer* countBuffer, uint32_t countOffset) override;
		//void multiDispatchIndirect(uint32_t maxCount, Buffer* argsBuffer, uint32_t argsOffset, Buffer* countBuffer, uint32_t countOffset) override;
		//void multiDispatchMeshIndirect(uint32_t maxCount, Buffer* argsBuffer, uint32_t argsOffset, Buffer* countBuffer, uint32_t countOffset) override;

	private:
		void updateGraphicsDescriptorBuffer();
		void updateComputeDescriptorBuffer();

	private:
		VkQueue m_Queue = VK_NULL_HANDLE;
		VkCommandPool m_CommandPool = VK_NULL_HANDLE;
		VkCommandBuffer m_CommandBuffer = VK_NULL_HANDLE;

		std::vector<VkCommandBuffer> m_FreeCommandBuffers;
		std::vector<VkCommandBuffer> m_PendingCommandBuffers;

		std::vector<VkMemoryBarrier2> m_MemoryBarriers;
		std::vector<VkBufferMemoryBarrier2> m_BufferBarriers;
		std::vector<VkImageMemoryBarrier2> m_ImageBarriers;

		std::vector<std::pair<Fence*, uint64_t>> m_PendingWaits;
		std::vector<std::pair<Fence*, uint64_t>> m_PendingSignals;
		std::vector<Swapchain*> m_PendingSwapchains;

		struct ConstantData
		{
			uint32_t ubv0[SE_MAX_PUSH_CONSTANTS] = {};
			VkDescriptorAddressInfoEXT ubv1 = { VK_STRUCTURE_TYPE_DESCRIPTOR_ADDRESS_INFO_EXT };
			VkDescriptorAddressInfoEXT ubv2 = { VK_STRUCTURE_TYPE_DESCRIPTOR_ADDRESS_INFO_EXT };
			bool needsUpdate = false;
		};

		ConstantData m_GraphicsConstants;
		ConstantData m_ComputeConstants;
	};
}