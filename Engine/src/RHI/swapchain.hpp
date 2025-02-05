#pragma once
#include "resource.hpp"
#include "types.hpp"
#include <cstdint>

namespace rhi {
	class ITexture;

	class ISwapchain : public IResource {
	public:
		virtual ITexture* getCurrentSwapchainImage() = 0;
		virtual bool acquireNextImage() = 0;
		virtual bool resize(uint32_t width, uint32_t height) = 0;
		virtual void setVSync(bool enabled) = 0;

		const SwapchainDescription& getDescription() const { return m_Description; }

	protected:
		SwapchainDescription m_Description{};
	};
}