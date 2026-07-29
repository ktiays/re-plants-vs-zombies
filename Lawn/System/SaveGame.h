#ifndef __SAVEGAMECONTEXT_H__
#define __SAVEGAMECONTEXT_H__

#include "LegacySaveFormat.h"

#include <bit>
#include <cstdint>
#include <string>

class Board;
class Trail;
enum GameMode;
class Reanimation;
class TodParticleSystem;
class TodParticleEmitter;
class ReanimatorDefinition;
class TodParticleDefinition;
class TrailDefinition;
struct TodAllocator;
template <typename T> class TodList;
namespace Sexy
{
    class Image;
}
using namespace Sexy;

class SaveGameContext
{
public:
    bool            mFailed{};
    bool            mReading{};

public:
    void            OpenRead(std::span<const std::byte> theBytes);
    void            OpenWrite();
    [[nodiscard]] std::uint64_t ByteLeftToRead() const;
    [[nodiscard]] std::span<const std::byte> GetWrittenBytes() const;
    void            SyncBytes(void* theDest, std::int32_t theReadSize);
    void            SyncInt(std::int32_t& theInt);
    inline void     SyncUint(std::uint32_t& theInt)
    {
        std::int32_t aSignedValue{};
        if (!mReading)
            aSignedValue = std::bit_cast<std::int32_t>(theInt);
        SyncInt(aSignedValue);
        theInt = std::bit_cast<std::uint32_t>(aSignedValue);
    }
    void            SyncReanimationDef(ReanimatorDefinition*& theDefinition);
    void            SyncParticleDef(TodParticleDefinition*& theDefinition);
    void            SyncTrailDef(TrailDefinition*& theDefinition);
    void            SyncImage(Image*& theImage);

private:
    LegacySaveReader mReader;
    LegacySaveWriter mWriter;
};

void                SyncDataIDList(TodList<unsigned int>* theDataIDList, SaveGameContext& theContext, TodAllocator* theAllocator);
void                SyncParticleEmitter(TodParticleSystem* theParticleSystem, TodParticleEmitter* theParticleEmitter, SaveGameContext& theContext);
void                SyncParticleSystem(Board* theBoard, TodParticleSystem* theParticleSystem, SaveGameContext& theContext);
void                SyncReanimation(Board* theBoard, Reanimation* theReanimation, SaveGameContext& theContext);
void                SyncTrail(Board* theBoard, Trail* theTrail, SaveGameContext& theContext);
void                SyncBoard(SaveGameContext& theContext, Board* theBoard);
void				FixBoardAfterLoad(Board* theBoard);
bool				LawnLoadGame(Board* theBoard, const std::string& theFilePath);
bool				LawnSaveGame(Board* theBoard, const std::string& theFilePath);

#endif
