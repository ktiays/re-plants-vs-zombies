#ifndef __ZOMBATARITEMS_H__
#define __ZOMBATARITEMS_H__
// @Patoke: implement file

#include "GameButton.h"

class ZombatarItem
{
public:
	struct Image* mImage;
	struct Image* mColorMask;
	int unk1;
	int unk2;
	int unk3;
	int unk4;
	int unk5;
	int unk6;
	const char* mTrackName;
	int mRenderGroup;
};

class ZombatarItems : public Widget {
public:
	LawnApp* mApp;
	GameButton* mPrevButton;
	GameButton* mNextButton;
	GameButton* mSkinButton;
	GameButton* mHairButton;
	GameButton* mFacialHairButton;
	GameButton* mTidbitsButton;
	GameButton* mEyeWearButton;
	GameButton* mClothesButton;
	GameButton* mAccessoryButton;
	GameButton* mHatsButton;
	GameButton* mBackdropsButton;
	GameButton* mBackButton;
	GameButton* mFinishedButton;
	GameButton* mNewZombieButton;
	GameButton* dwordDC;
	GameButton* dwordE0;
	GameButton* mMainMenuButton;
	GameButton* mViewButton;
	DWORD mItemColorContainer;
	DWORD dwordF0;
	DWORD dwordF4;
	DWORD dwordF8;
	DWORD dwordFC;
	DWORD dword100;
	BYTE gap104[3584];
	DWORD dwordF04;
	__declspec(align(16)) DWORD dwordF10;
	DWORD theZombatar[18];
	DWORD n3;
	DWORD dwordF60;
	DWORD mNumZombatars;
	BYTE gapF68[4];
	BYTE byteF6C;
	BYTE byteF6D;
	BYTE mShouldSaveZombatar;
	BYTE byteF6F;
	DWORD dwordF70;
	DWORD dwordF74;
	DWORD dwordF78;
	DWORD dwordF7C;

	ZombatarItems(LawnApp* theApp);
	virtual ~ZombatarItems();

};


#endif