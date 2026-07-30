#include "pvz/engine/core/BinaryStateIO.h"
#include "pvz/engine/core/NullImageStore.h"
#include "pvz/game/GameModule.h"

#include <cstddef>
#include <cstdint>
#include <iostream>
#include <span>
#include <string_view>
#include <vector>

namespace
{

class HeadlessLogger final : public pvz::engine::ILogger
{
public:
    void Log(
        pvz::engine::LogLevel theLevel,
        std::string_view theMessage) override
    {
        static_cast<void>(theLevel);
        std::cout << theMessage << '\n';
    }
};

class HeadlessServices final : public pvz::engine::IEngineServices
{
public:
    [[nodiscard]] pvz::engine::ILogger& GetLogger() override
    {
        return mLogger;
    }

    [[nodiscard]] pvz::engine::IResourceStore& GetResources() override
    {
        return mResources;
    }

    [[nodiscard]] pvz::engine::IXmlDocumentLoader&
    GetXmlDocuments() override
    {
        return mXmlDocuments;
    }

    [[nodiscard]] pvz::engine::IImageStore& GetImages() override
    {
        return mImages;
    }

private:
    class EmptyResourceStore final : public pvz::engine::IResourceStore
    {
    public:
        [[nodiscard]] bool Contains(
            std::string_view thePath) const override
        {
            static_cast<void>(thePath);
            return false;
        }

        [[nodiscard]] bool GetSize(
            std::string_view thePath,
            std::uint64_t& theSize) const override
        {
            static_cast<void>(thePath);
            static_cast<void>(theSize);
            return false;
        }

        [[nodiscard]] bool ReadAll(
            std::string_view thePath,
            std::vector<std::byte>& theBytes) const override
        {
            static_cast<void>(thePath);
            static_cast<void>(theBytes);
            return false;
        }
    };

    class EmptyXmlDocumentLoader final
        : public pvz::engine::IXmlDocumentLoader
    {
    public:
        [[nodiscard]] bool Load(
            std::string_view thePath,
            pvz::engine::XmlDocumentMode theMode,
            std::vector<pvz::engine::XmlNode>& theRoots,
            pvz::engine::XmlDocumentDiagnostic& theDiagnostic)
            const override
        {
            static_cast<void>(thePath);
            static_cast<void>(theMode);
            static_cast<void>(theRoots);
            theDiagnostic.mError =
                pvz::engine::XmlDocumentError::ResourceReadFailed;
            theDiagnostic.mLine = 0;
            return false;
        }
    };

    HeadlessLogger mLogger;
    EmptyResourceStore mResources;
    EmptyXmlDocumentLoader mXmlDocuments;
    pvz::engine::core::NullImageStore mImages;
};

class EmptyInputFrame final : public pvz::engine::IInputFrame
{
public:
    [[nodiscard]] bool IsKeyDown(
        pvz::engine::KeyCode theKey) const override
    {
        static_cast<void>(theKey);
        return false;
    }

    [[nodiscard]] bool WasKeyPressed(
        pvz::engine::KeyCode theKey) const override
    {
        static_cast<void>(theKey);
        return false;
    }

    [[nodiscard]] bool IsPointerButtonDown(
        pvz::engine::PointerButton theButton) const override
    {
        static_cast<void>(theButton);
        return false;
    }

    [[nodiscard]] bool WasPointerButtonPressed(
        pvz::engine::PointerButton theButton) const override
    {
        static_cast<void>(theButton);
        return false;
    }

    [[nodiscard]] pvz::engine::PointerState GetPointerState()
        const override
    {
        return {};
    }

    [[nodiscard]] std::span<const char32_t> GetTextInput() const override
    {
        return {};
    }
};

class HeadlessRenderFrame final : public pvz::engine::IRenderFrame
{
public:
    [[nodiscard]] pvz::engine::SizeI GetLogicalSize() const override
    {
        return {800, 600};
    }

    void Clear(pvz::engine::ColorRgba8 theColor) override
    {
        mClearColor = theColor;
    }

    void SubmitSprites(
        std::span<const pvz::engine::SpriteDraw> theDraws) override
    {
        mSpriteCount += static_cast<std::uint64_t>(theDraws.size());
    }

private:
    pvz::engine::ColorRgba8 mClearColor{};
    std::uint64_t mSpriteCount{};
};

std::uint64_t CalculateFnv1a(std::span<const std::byte> theBytes)
{
    std::uint64_t aHash = 14'695'981'039'346'656'037ULL;
    for (const auto aByte : theBytes)
    {
        aHash ^= std::to_integer<std::uint8_t>(aByte);
        aHash *= 1'099'511'628'211ULL;
    }
    return aHash;
}

} // namespace

int main()
{
    HeadlessServices aServices;
    EmptyInputFrame anInput;
    HeadlessRenderFrame aFrame;
    pvz::game::GameModule aGame;

    if (aGame.Initialize(aServices) != pvz::engine::LifecycleResult::Success)
        return 1;

    for (pvz::engine::TickIndex aTick = 0; aTick < 100; ++aTick)
        aGame.Update(pvz::engine::GameTick{aTick}, anInput);

    aGame.Render(aFrame);

    pvz::engine::core::BinaryStateWriter aWriter;
    if (!aGame.SaveState(aWriter))
        return 1;

    std::cout << "ticks=" << aGame.GetUpdateCount()
              << " state-bytes=" << aWriter.GetBytesWritten()
              << " state-fnv1a=" << CalculateFnv1a(aWriter.GetBytes())
              << '\n';
    aGame.Shutdown();
    return 0;
}
