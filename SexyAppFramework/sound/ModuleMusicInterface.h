#ifndef __SEXY_MODULEMUSICINTERFACE_H__
#define __SEXY_MODULEMUSICINTERFACE_H__

#include "MusicInterface.h"
#include "AudioMixer.h"

#include <map>
#include <memory>

namespace openmpt
{
class module_ext;
namespace ext
{
class interactive;
class interactive3;
}
}

namespace Sexy
{

class OggStream;

// Music through the software mixer: tracker modules (MO3, IT, XM, ...) are
// rendered by libopenmpt, Ogg Vorbis files are streamed by Tremor. Replaces
// the BASS music backend, including the per-channel and tempo controls the
// game's adaptive soundtrack relies on.
class ModuleMusicInterface : public MusicInterface, public MixerSource
{
public:
	struct Track
	{
		std::unique_ptr<openmpt::module_ext>	mModule;
		openmpt::ext::interactive*				mInteractive = nullptr;
		openmpt::ext::interactive3*				mInteractive3 = nullptr;
		std::unique_ptr<OggStream>				mStream;
		double									mVolume = 0.0;
		double									mVolumeAdd = 0.0;
		double									mVolumeCap = 1.0;
		bool									mStopOnFade = false;
		bool									mPlaying = false;
		bool									mPaused = false;
		bool									mLoop = true;
		double									mStreamPosition = 0.0;	// fractional frame for resampling
		std::vector<std::int16_t>				mStreamBuffer;
		int										mStreamBufferFrames = 0;
		int										mStreamBufferPos = 0;

		Track();
		~Track();
	};

protected:
	AudioMixer*					mMixer;
	std::map<int, std::unique_ptr<Track>> mTracks;
	double						mMasterVolume;
	std::vector<float>			mRenderBuffer;

	Track*						FindTrack(int theSongId);
	void						StartTrack(Track* theTrack, int theOffset, bool noLoop);
	void						MixStream(Track* theTrack, float* theBuffer, int theFrames, int theSampleRate, float theGain);

public:
	ModuleMusicInterface(AudioMixer* theMixer);
	virtual ~ModuleMusicInterface();

	virtual bool				LoadMusic(int theSongId, const std::string& theFileName);
	virtual void				PlayMusic(int theSongId, int theOffset = 0, bool noLoop = false);
	virtual void				StopMusic(int theSongId);
	virtual void				PauseMusic(int theSongId);
	virtual void				ResumeMusic(int theSongId);
	virtual void				StopAllMusic();
	virtual void				UnloadMusic(int theSongId);
	virtual void				UnloadAllMusic();
	virtual void				PauseAllMusic();
	virtual void				ResumeAllMusic();
	virtual void				FadeIn(int theSongId, int theOffset = -1, double theSpeed = 0.002, bool noLoop = false);
	virtual void				FadeOut(int theSongId, bool stopSong = true, double theSpeed = 0.004);
	virtual void				FadeOutAll(bool stopSong = true, double theSpeed = 0.004);
	virtual void				SetSongVolume(int theSongId, double theVolume);
	virtual void				SetSongMaxVolume(int theSongId, double theMaxVolume);
	virtual bool				IsPlaying(int theSongId);
	virtual void				SetVolume(double theVolume);
	virtual void				SetMusicAmplify(int theSongId, double theAmp);
	virtual void				Update();

	virtual bool				IsMusicLoaded(int theSongId);
	virtual bool				IsModule(int theSongId);
	virtual void				PlayModule(int theSongId, int theOrder, double theVolume, bool theLoop);
	virtual void				SetModuleOrder(int theSongId, int theOrder);
	virtual int					GetMusicOrder(int theSongId);
	virtual void				SetModuleChannelVolume(int theSongId, int theChannel, double theVolume);
	virtual void				SetModuleTempo(int theSongId, float theBPM);
	virtual float				GetModuleTempo(int theSongId);
	virtual void				SetModuleSpeed(int theSongId, float theSpeed);
	virtual float				GetModuleSpeed(int theSongId);

	virtual void				MixInto(float* theBuffer, int theFrames, int theSampleRate);
};

}

#endif //__SEXY_MODULEMUSICINTERFACE_H__
