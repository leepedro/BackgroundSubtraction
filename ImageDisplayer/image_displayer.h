#if !defined(IMAGE_DISPLAYER_H)
#define IMAGE_DISPLAYER_H

// Standard C++ header files.
#include <string>
#include <deque>
#include <vector>

// Windows header files.
#include <Windows.h>
#include <wincodec.h>
#include <d2d1.h>
#include "resource.h"

template <typename I>
void SafeRelease(I *&rc)
{
	if (rc != nullptr)
	{
		rc->Release();
		rc = nullptr;
	}
}

void RunMessageLoop(void)
{
	BOOL result(0);
	MSG msg;
	while ((result = ::GetMessageW(&msg, nullptr, 0, 0)) != 0)
	{
		if (result == -1)
			break;
		else
		{
			::TranslateMessage(&msg);
			::DispatchMessageW(&msg);
		}
	}
}


// Base abstract class to create a window.
// This class should be maintained without any change.
template <class DERIVED_TYPE>
class BaseWindow
{
public:
	static LRESULT CALLBACK WindowProc(HWND hWnd, unsigned int msg, WPARAM wParam, LPARAM lParam);
	BaseWindow(void) = default;
	// NOTE: cannot name this function as CreateWindow because it is a macro in Windows.h.
	bool Create(wchar_t *wndName, unsigned long style, unsigned long extraStyle = 0, int x = CW_USEDEFAULT, int y = CW_USEDEFAULT, int width = CW_USEDEFAULT, int height = CW_USEDEFAULT, HWND hWndParent = NULL, HMENU hMenu = NULL);
	HWND Window() const { return this->hWnd; }

protected:
	virtual wchar_t *ClassName() const = 0;
	virtual LRESULT HandleMessage(unsigned int msf, WPARAM wParam, LPARAM lParam) = 0;
	HWND hWnd = nullptr;	// Default value for HANDLE data type doesn't really make sense as it presents somewhat like a reference.
};

template <class DERIVED_TYPE>
LRESULT CALLBACK BaseWindow<DERIVED_TYPE>::WindowProc(HWND hWnd, unsigned int msg, WPARAM wParam, LPARAM lParam)
{
	DERIVED_TYPE *self(nullptr);
	if (msg == WM_NCCREATE)
	{
		::CREATESTRUCTW *create = reinterpret_cast<CREATESTRUCTW *>(lParam);
		self = reinterpret_cast<DERIVED_TYPE *>(create->lpCreateParams);
		::SetWindowLongPtrW(hWnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(self));
		self->hWnd = hWnd;
	}
	else
		self = reinterpret_cast<DERIVED_TYPE *>(::GetWindowLongPtrW(hWnd, GWLP_USERDATA));
	if (self != nullptr)
		return self->HandleMessage(msg, wParam, lParam);
	else
		return ::DefWindowProcW(hWnd, msg, wParam, lParam);
}

template <class DERIVED_TYPE>
bool BaseWindow<DERIVED_TYPE>::Create(wchar_t *wndName, unsigned long style, unsigned long extraStyle, int x, int y, int width, int height, HWND hWndParent, HMENU hMenu)
{
	// Register window class.
	::WNDCLASSW wc = {};	// NOTE: '= {}' is mandatory.
	wc.lpfnWndProc = DERIVED_TYPE::WindowProc;
	wc.hInstance = ::GetModuleHandleW(nullptr);
	wc.lpszClassName = this->ClassName();
	wc.lpszMenuName = MAKEINTRESOURCEW(IDR_MENU1);
	::RegisterClassW(&wc);

	// Create the window.
	this->hWnd = ::CreateWindowExW(extraStyle, this->ClassName(), wndName, style, x, y, width, height, hWndParent, hMenu, wc.hInstance, this);
	return this->hWnd == NULL ? false : true;
}


// Class implementing the window. 
class MainWindow : public BaseWindow <MainWindow>
{
public:
	MainWindow(void) = default;
	~MainWindow(void);
	wchar_t *ClassName() const { return L"Image Viewer using Direct2D and WIC"; }
	LRESULT HandleMessage(unsigned int msg, WPARAM wParam, LPARAM lParam);

protected:
	int DisplayImages(void);
	bool LoadImageFile(const std::wstring &pathSrc, std::vector<unsigned char> &dst, ::size_t &width, ::size_t &height);
	//bool CreateD2DBmpFromFile(void);
	//bool LocateImagefile(std::wstring &path);

	// Resource handlers.
	bool CreateResouces(void);	// device independent resources
	bool CreateDeviceResources(void);	// device dependent resources
	void ReleaseResources(void);	// device independent resources
	void ReleaseDeviceResources(void);	// device dependent resources

	// Message handlers.
	void OnPaint(void);
	void OnSize(LPARAM lParam);

	// Device independent resources.
	::ID2D1Factory *D2Factory = nullptr;
	::IWICFormatConverter *BmpSrc = nullptr;
	::IWICImagingFactory *WicFactory = nullptr;

	// Device dependent resources.
	::ID2D1HwndRenderTarget *RenderTarget = nullptr;
	::ID2D1Bitmap *Bmp = nullptr;

	// Variables.
	//const ::size_t MAX_BUFFER_LENGTH = 5;
	//std::deque<std::vector<float>> buffer;
	std::vector<std::wstring> filenames;
	std::wstring folder;

};

#endif
