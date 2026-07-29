#pragma once

#include "pvz/engine/Runtime.h"

#include <memory>
#include <string_view>

namespace pvz::platform::macos
{

class MetalRenderDevice final : public engine::IRenderDevice
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

private:
    struct Implementation;
    std::unique_ptr<Implementation> mImplementation;
};

} // namespace pvz::platform::macos
