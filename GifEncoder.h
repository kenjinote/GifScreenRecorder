#pragma once

#include <vector>
#include <string>

class CGifEncoder
{
public:
	CGifEncoder();
	~CGifEncoder();

public:
	bool StartEncoder(std::wstring &saveFilePath);
	bool AddFrame(Gdiplus::Image *pImage);
	bool AddFrame(std::wstring &framePath);
	bool FinishEncoder();
	void SetDelayTime(int ms);
	void SetRepeatNum(int num);
	void SetFrameRate(float fps);
	void SetFrameSize(int width, int height);

private:
	void SetImagePropertyItem();
	bool GetEncoderClsid(const WCHAR* format, CLSID* pClsid);

private:
	int					m_width;
	int					m_height;
	int					m_repeatNum;
	int					m_delayTime;
	bool				m_started;
	bool				m_haveFrame;

	std::wstring *m_pStrSavePath;
	Gdiplus::Bitmap *m_pImage;
	std::vector<Gdiplus::Bitmap *> m_pBitMapVec;
};

