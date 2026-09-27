#ifndef __SYSFONT_H__
#define __SYSFONT_H__

#include "Font.h"

namespace Sexy
{

class ImageFont;
class SexyAppBase;

// Fallback font for debug overlays and framework widgets that were not given
// a font. The Windows version rasterized a GDI font; the portable version
// draws an embedded 5x7 bitmap font, scaled to approximate the point size.
class SysFont : public _Font
{
public:	
	SexyAppBase*			mApp;
	bool					mDrawShadow;
	bool					mSimulateBold;
	int						mScale;
	bool					mBold;

	void Init(SexyAppBase* theApp, int thePointSize, bool bold);

public:
	enum { DEFAULT_SCRIPT = 0 };

	SysFont(const std::string& theFace, int thePointSize, bool bold = false, bool italics = false, bool underline = false);
	SysFont(SexyAppBase* theApp, const std::string& theFace, int thePointSize, int theScript = DEFAULT_SCRIPT, bool bold = false, bool italics = false, bool underline = false);
	SysFont(const SysFont& theSysFont);

	virtual ~SysFont();

	virtual int				StringWidth(const SexyString& theString);
	virtual int				CharWidth(SexyChar theChar);
	virtual void			DrawString(Graphics* g, int theX, int theY, const SexyString& theString, const Color& theColor, const Rect& theClipRect);

	virtual _Font*			Duplicate();
};

}

#endif //__SYSFONT_H__
