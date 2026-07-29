#pragma once

#include "pvz/engine/EngineServices.h"
#include "pvz/engine/Input.h"
#include "pvz/engine/Render.h"
#include "pvz/engine/StateIO.h"
#include "pvz/engine/Types.h"

namespace pvz::engine
{

class IGame
{
public:
    virtual ~IGame() = default;

    [[nodiscard]] virtual LifecycleResult Initialize(
        IEngineServices& theServices) = 0;
    virtual void Update(
        const GameTick& theTick,
        const IInputFrame& theInput) = 0;
    virtual void Render(IRenderFrame& theFrame) const = 0;
    [[nodiscard]] virtual bool LoadState(IStateReader& theReader) = 0;
    [[nodiscard]] virtual bool SaveState(IStateWriter& theWriter) const = 0;
    virtual void Suspend() = 0;
    virtual void Resume() = 0;
    virtual void Shutdown() = 0;
};

} // namespace pvz::engine
