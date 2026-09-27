#ifndef __POPDRMCOMM_H__
#define __POPDRMCOMM_H__

// Communication with the PopCap DRM wrapper that launched the Windows game.
// The portable build has no DRM wrapper; the protocol reports an unlocked,
// registered game.
class PopDRMComm
{
public:
	enum
	{
		PopCapDRM_EnableLocking,
		PopCapDRM_EnableTiming,
		PopCapDRM_QueryData,
		PopCapDRM_IPC,
		PopCapDRM_IPC_Response,
		PopCapDRM_ForceLock,
		PopCapDRM_BuyGame,
		PopCapDRM_ShowScreen,
		PopCapDRM_WaitUI,
		PopCapDRM_NumMessages
	};

	bool					mIsWindow;

public:
	PopDRMComm();  //0x5D5AC0
	~PopDRMComm();  //0x5D5B60

	void					Start();  //0x5D5BC0
	void					EnableLocking();  //0x5D5C80
	void					BuyGame();
	bool					DoIPC();  //0x5D5D60
	bool					QueryData();  //0x5D5CB0
};

#endif
