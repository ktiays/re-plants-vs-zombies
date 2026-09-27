#ifndef __MUSICINTERFACE_H__
#define __MUSICINTERFACE_H__

#include "Common.h"

namespace Sexy
{

class MusicInterface
{
public:
	MusicInterface() {}
	virtual ~MusicInterface() {}

	virtual bool			LoadMusic(int theSongId, const std::string& theFileName) = 0;
	virtual void			PlayMusic(int theSongId, int theOffset = 0, bool noLoop = false) = 0;
	virtual void			StopMusic(int theSongId) = 0;
	virtual void			PauseMusic(int theSongId) = 0;
	virtual void			ResumeMusic(int theSongId) = 0;
	virtual void			StopAllMusic() = 0;

	virtual void			UnloadMusic(int theSongId) = 0;
	virtual void			UnloadAllMusic() = 0;
	virtual void			PauseAllMusic() = 0;
	virtual void			ResumeAllMusic() = 0;
	
	virtual void			FadeIn(int theSongId, int theOffset = -1, double theSpeed = 0.002, bool noLoop = false) = 0;
	virtual void			FadeOut(int theSongId, bool stopSong = true, double theSpeed = 0.004) = 0;
	virtual void			FadeOutAll(bool stopSong = true, double theSpeed = 0.004) = 0;
	virtual void			SetSongVolume(int theSongId, double theVolume) = 0;
	virtual void			SetSongMaxVolume(int theSongId, double theMaxVolume) = 0;
	virtual bool			IsPlaying(int theSongId) = 0;
	
	virtual void			SetVolume(double theVolume) = 0;
	virtual void			SetMusicAmplify(int theSongId, double theAmp) = 0;
	virtual void			Update() = 0;

	// Tracker-module controls used by the game's layered, tempo-synchronized
	// music. Streamed music ignores them. Positions are packed the way the
	// original BASS backend reported them with a position scaler of 4:
	// LOWORD is the order, HIWORD is the row multiplied by 4.
	virtual bool			IsMusicLoaded(int theSongId) { (void)theSongId; return false; }
	virtual bool			IsModule(int theSongId) { (void)theSongId; return false; }
	virtual void			PlayModule(int theSongId, int theOrder, double theVolume, bool theLoop) { (void)theSongId; (void)theOrder; (void)theVolume; (void)theLoop; }
	virtual void			SetModuleOrder(int theSongId, int theOrder) { (void)theSongId; (void)theOrder; }
	virtual int				GetMusicOrder(int theSongId) { (void)theSongId; return -1; }
	virtual void			SetModuleChannelVolume(int theSongId, int theChannel, double theVolume) { (void)theSongId; (void)theChannel; (void)theVolume; }
	virtual void			SetModuleTempo(int theSongId, float theBPM) { (void)theSongId; (void)theBPM; }
	virtual float			GetModuleTempo(int theSongId) { (void)theSongId; return 0; }
	virtual void			SetModuleSpeed(int theSongId, float theSpeed) { (void)theSongId; (void)theSpeed; }
	virtual float			GetModuleSpeed(int theSongId) { (void)theSongId; return 0; }
};
}

#endif //__MUSICINTERFACE_H__
