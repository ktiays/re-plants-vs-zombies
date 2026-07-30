#pragma once

#include "pvz/engine/Runtime.h"

#include <memory>
#include <string_view>

namespace pvz::platform::macos
{

class MetalRenderDevice final
    : public engine::IRenderDevice,
      public engine::IImageStore
{
public:
    MetalRenderDevice();
    ~MetalRenderDevice() override;

    MetalRenderDevice(const MetalRenderDevice&) = delete;
    MetalRenderDevice& operator=(const MetalRenderDevice&) = delete;

    [[nodiscard]] bool Initialize(void* theMetalLayer);
    [[nodiscard]] std::string_view GetLastError() const;

    [[nodiscard]] engine::RenderFrameResult BeginFrame(
        engine::IRenderFrame*& theFrame) override;
    [[nodiscard]] bool EndFrame() override;

    [[nodiscard]] bool CreateImage(
        const engine::ImageDescriptor& theDescriptor,
        std::span<const std::byte> theInitialPixels,
        std::uint32_t theSourceBytesPerRow,
        engine::ImageHandle& theImage) override;
    [[nodiscard]] bool UpdateImage(
        engine::ImageHandle theImage,
        const engine::ImageUpdate& theUpdate) override;
    void DestroyImage(engine::ImageHandle theImage) override;
    [[nodiscard]] bool GetImageSize(
        engine::ImageHandle theImage,
        engine::SizeI& theSize) const override;

private:
    struct Implementation;
    std::unique_ptr<Implementation> mImplementation;
};

} // namespace pvz::platform::macos
