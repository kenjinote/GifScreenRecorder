#include <vector>
#include <windows.h>
#include <gdiplus.h>
#include "GifEncoder.h"

CGifEncoder::CGifEncoder()
{
	m_started = false;
	m_width = 500;
	m_height = 500;
	m_delayTime = 100;
	m_repeatNum = 0;
	m_haveFrame = false;
	m_pStrSavePath = NULL;
	m_pImage = NULL;
}

CGifEncoder::~CGifEncoder()
{
	if (NULL != m_pStrSavePath)
	{
		delete m_pStrSavePath;
	}
	if (NULL != m_pImage)
	{
		delete m_pImage;
	}
	const size_t size = m_pBitMapVec.size();
	for (size_t ix = 0; ix<size; ix++)
	{
		delete m_pBitMapVec[ix];
		m_pBitMapVec[ix] = NULL;
	}
}

bool CGifEncoder::StartEncoder(std::wstring &saveFilePath)
{
	bool flag = true;
	if (NULL != m_pStrSavePath)
	{
		delete m_pStrSavePath;
		m_pStrSavePath = NULL;
	}
	m_pStrSavePath = new std::wstring(saveFilePath);
	m_started = true;
	return flag;
}

bool CGifEncoder::AddFrame(Gdiplus::Image *pImage)
{
	if (!m_started || NULL == pImage)
	{
		return(false);
	}
	bool flag = true;
	if (!m_haveFrame)
	{
		m_pImage = new Gdiplus::Bitmap(m_width, m_height);
		Gdiplus::Graphics g(m_pImage);
		g.DrawImage(pImage, 0, 0, m_width, m_height);
		m_haveFrame = true;
		return(true);
	}
	Gdiplus::Bitmap *pBitMap = new Gdiplus::Bitmap(m_width, m_height);
	Gdiplus::Graphics g(pBitMap);
	g.DrawImage(pImage, 0, 0, m_width, m_height);
	m_pBitMapVec.push_back(pBitMap);
	return flag;
}

bool CGifEncoder::AddFrame(std::wstring &framePath)
{
	if (!m_started)
	{
		return(false);
	}
	bool flag = true;
	Gdiplus::Status statue;
	if (!m_haveFrame)
	{
		m_pImage = new Gdiplus::Bitmap(m_width, m_height);
		Gdiplus::Graphics g(m_pImage);
		Gdiplus::Bitmap bitmap(framePath.c_str());
		g.DrawImage(&bitmap, 0, 0, m_width, m_height);
		m_haveFrame = true;
		return true;
	}
	Gdiplus::Bitmap *pBitMap = new Gdiplus::Bitmap(m_width, m_height);
	Gdiplus::Graphics g(pBitMap);
	Gdiplus::Bitmap bitmap(framePath.c_str());
	statue = g.DrawImage(&bitmap, 0, 0, m_width, m_height);
	m_pBitMapVec.push_back(pBitMap);
	return flag;
}

bool CGifEncoder::FinishEncoder()
{
	if (!m_started || !m_haveFrame)
	{
		return false;
	}
	bool	flag = true;
	Gdiplus::Status statue;
	SetImagePropertyItem();
	GUID gifGUID;
	Gdiplus::EncoderParameters encoderParams;
	GetEncoderClsid(L"image/gif", &gifGUID);
	encoderParams.Count = 1;
	encoderParams.Parameter[0].Guid = Gdiplus::EncoderSaveFlag;
	encoderParams.Parameter[0].NumberOfValues = 1;
	encoderParams.Parameter[0].Type = Gdiplus::EncoderParameterValueTypeLong;
	long firstValue = Gdiplus::EncoderValueMultiFrame;
	encoderParams.Parameter[0].Value = &firstValue;
	m_pImage->Save(m_pStrSavePath->c_str(), &gifGUID, &encoderParams);
	const size_t size = m_pBitMapVec.size();
	firstValue = Gdiplus::EncoderValueFrameDimensionTime;
	encoderParams.Parameter[0].Value = &firstValue;
	for (size_t ix = 0; ix <size; ix++)
	{
		statue = m_pImage->SaveAdd(m_pBitMapVec[ix], &encoderParams);
	}
	m_started = false;
	m_haveFrame = false;
	return(flag);
}

void CGifEncoder::SetDelayTime(int ms)
{
	if (ms > 0)
	{
		m_delayTime = (int)(ms / 10.0f);
	}
}

void CGifEncoder::SetRepeatNum(int num)
{
	if (num >= 0)
	{
		m_repeatNum = num;
	}
}

void CGifEncoder::SetFrameRate(float fps)
{
	if (fps > 0)
	{
		m_delayTime = (int)(100.0f / fps);
	}
}

void CGifEncoder::SetFrameSize(int width, int height)
{
	if (!m_haveFrame)
	{
		m_width = width;
		m_height = height;
		if (m_width < 1)
			m_width = 1;
		if (m_height < 1)
			m_height = 1;
	}
}

void CGifEncoder::SetImagePropertyItem()
{
	if (!m_started || NULL == m_pImage)
	{
		return;
	}
	Gdiplus::PropertyItem propertyItem;
	short sValue = m_repeatNum;
	propertyItem.id = PropertyTagLoopCount;
	propertyItem.length = 2;
	propertyItem.type = PropertyTagTypeShort;
	propertyItem.value = &sValue;
	m_pImage->SetPropertyItem(&propertyItem);
	const long lImageNum = 1 + m_pBitMapVec.size();
	long *plValue = new long[lImageNum];
	for (int ix = 0; ix<lImageNum; ix++)
	{
		plValue[ix] = m_delayTime;
	}
	propertyItem.id = PropertyTagFrameDelay;
	propertyItem.length = 4 * lImageNum;
	propertyItem.type = PropertyTagTypeLong;
	propertyItem.value = plValue;
	m_pImage->SetPropertyItem(&propertyItem);
	delete[]plValue;
	plValue = NULL;
}

bool CGifEncoder::GetEncoderClsid(const WCHAR* format, CLSID* pClsid)
{
	UINT num = 0, size = 0;
	Gdiplus::GetImageEncodersSize(&num, &size);
	if (size == 0)
		return false;
	Gdiplus::ImageCodecInfo* pImageCodecInfo = (Gdiplus::ImageCodecInfo*)(malloc(size));
	Gdiplus::GetImageEncoders(num, size, pImageCodecInfo);
	bool found = false;
	for (UINT ix = 0; !found && ix < num; ++ix)
	{
		if (_wcsicmp(pImageCodecInfo[ix].MimeType, format) == 0)
		{
			*pClsid = pImageCodecInfo[ix].Clsid;
			found = true;
			break;
		}
	}
	free(pImageCodecInfo);
	return found;
}