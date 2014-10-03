// Standard C++ header files.
#include <vector>
#include <iostream>
#include <algorithm>

// Windows header files.
#include <ShlObj.h>

// Custom header files.
#include "image_displayer.h"


void LoadFileList(std::wstring &pathFolder, std::vector<std::wstring> &filenames)
{
	::IFileDialog *file_dialog(nullptr);
	if (SUCCEEDED(::CoCreateInstance(__uuidof(::FileOpenDialog), nullptr, CLSCTX_ALL, IID_PPV_ARGS(&file_dialog))))
	{
		// Set the options for a file dialog to show only folders.
		::FILEOPENDIALOGOPTIONS options;
		if (SUCCEEDED(file_dialog->GetOptions(&options)))
		{
			if (SUCCEEDED(file_dialog->SetOptions(options | ::FOS_PICKFOLDERS | ::FOS_FORCEFILESYSTEM)))
			{
				// Open a file open dialog.
				HRESULT result = file_dialog->Show(nullptr);
				if (SUCCEEDED(result))
				{
					// Get the path of the selected folder.
					::IShellItem *selected_item(nullptr);
					if (SUCCEEDED(file_dialog->GetResult(&selected_item)))
					{
						wchar_t *path_folder(nullptr);
						// NOTE: if multi-threading option is selected for COM initialization,
						// the following function fails for the user library. Don't know why.
						if (SUCCEEDED(selected_item->GetDisplayName(::SIGDN_FILESYSPATH, &path_folder)))
						{
							pathFolder = path_folder;

							// Collected the list of files under the selected folder.
							::WIN32_FIND_DATAW find_data;
							std::wstring search_pattern = std::wstring(path_folder) + L"\\*";
							HANDLE hFindFile = ::FindFirstFileW(search_pattern.c_str(), &find_data);
							if (hFindFile != INVALID_HANDLE_VALUE)
							{
								do
								{
									// Skip {., .., sub-folders} for now, and collect only filenames at the selected folder.
									if (find_data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)
										std::wclog << L"Skipping " << find_data.cFileName << std::endl;//
									else
										filenames.push_back(std::wstring(find_data.cFileName));
								} while (::FindNextFileW(hFindFile, &find_data));

								::FindClose(hFindFile);

								// Sort the list of filenames.
								std::sort(filenames.begin(), filenames.end());
							}
							else
								::MessageBoxW(nullptr, L"Failed to find the first file in the selected folder.", L"Error", MB_OK);

							::CoTaskMemFree(path_folder);
						}
						else
							::MessageBoxW(nullptr, L"Failed to get the path of the selected folder.", L"Error", MB_OK);

						selected_item->Release();
					}
					else
						::MessageBoxW(nullptr, L"Failed to get the result from a file dialog.", L"Error", MB_OK);
				}
				else if (result == ::HRESULT_FROM_WIN32(ERROR_CANCELLED)) {}	// Do nothing if canceled.
				else
					::MessageBoxW(nullptr, L"Failed to get a response from a file dialog.", L"Error", MB_OK);
			}
			else
				::MessageBoxW(nullptr, L"Failed to set the options of a file dialog.", L"Error", MB_OK);
		}
		else
			::MessageBoxW(nullptr, L"Failed to get the options of a file dialog.", L"Error", MB_OK);

		file_dialog->Release();
	}
	else
		::MessageBoxW(nullptr, L"Failed to create a file dialog.", L"Error", MB_OK);
}


MainWindow::~MainWindow(void)
{
	this->ReleaseDeviceResources();
	this->ReleaseResources();
}

LRESULT MainWindow::HandleMessage(unsigned int msg, WPARAM wParam, LPARAM lParam)
{
	switch (msg)
	{
	case WM_COMMAND:
		switch (LOWORD(wParam))
		{
		case ID_FILE_OPEN:
		{
			auto count = this->DisplayImages();
			std::wstring msg = L"Total " + std::to_wstring(count) + L" files.";
			::MessageBoxW(nullptr, msg.c_str(), L"Completed", MB_OK | MB_ICONINFORMATION);
		}
			break;
		case ID_FILE_EXIT:
			::PostMessageW(this->hWnd, WM_CLOSE, 0, 0);
		}
		return 0;
	case WM_CREATE:
		if (!this->CreateResouces())
			return -1;	// Fail CreateWindowExW() if creating resources failed.
		else
			return 0;
	case WM_DESTROY:
		::PostQuitMessage(0);
		return 0;
	case WM_PAINT:
		this->OnPaint();
		return 0;
	case WM_SIZE:
		this->OnSize(lParam);
		return 0;
	default:
		return ::DefWindowProcW(this->hWnd, msg, wParam, lParam);
	}
}

int MainWindow::DisplayImages(void)
{
	// Get the list of image files.
	// Load which folder to read files.
	::LoadFileList(this->folder, this->filenames);

	// Load an image file from the list.
	std::vector<unsigned char> dst;
	::size_t width, height;
	int count(0);
	for (const auto &filename : this->filenames)
	{
		// Load an image file.
		std::wstring path_src = this->folder + L"\\" + filename;
		if (LoadImageFile(path_src, dst, width, height))
			::InvalidateRect(this->hWnd, nullptr, TRUE);
		::Sleep(10);
		++count;
	}
	return count;
}

bool MainWindow::LoadImageFile(const std::wstring &pathSrc, std::vector<unsigned char> &dst, ::size_t &width, ::size_t &height)
{
	// Decode a source image file.
	::IWICBitmapDecoder *decoder(nullptr);
	if (SUCCEEDED(this->WicFactory->CreateDecoderFromFilename(pathSrc.c_str(), nullptr, GENERIC_READ,
		::WICDecodeMetadataCacheOnDemand, &decoder)))
	{
		// Get the frame.
		::IWICBitmapFrameDecode *frame(nullptr);
		if (SUCCEEDED(decoder->GetFrame(0, &frame)))
		{
			// Convert the source image frame to 32bit BGRA.
			SafeRelease(this->BmpSrc);
			if (SUCCEEDED(this->WicFactory->CreateFormatConverter(&this->BmpSrc)))
			{
				if (SUCCEEDED(this->BmpSrc->Initialize(frame, ::GUID_WICPixelFormat32bppPBGRA,
					::WICBitmapDitherTypeNone, nullptr, 0.0, ::WICBitmapPaletteTypeCustom)))
				{
					// Create a bitmap for screen from the stored bitmap.
					if (SUCCEEDED(this->CreateDeviceResources()))
					{
						SafeRelease(this->Bmp);
						if (SUCCEEDED(this->RenderTarget->CreateBitmapFromWicBitmap(this->BmpSrc,
							nullptr, &this->Bmp)))
						{
							unsigned int w, h;
							if (SUCCEEDED(this->BmpSrc->GetSize(&w, &h)))
							{
								width = w;
								height = h;

								// Set the size with unsigned int instead of ::size_t because ::size_t (== unsigned long) can be wider than unsigned int.
								unsigned int sz = w * h * 4;
								if (dst.size() != sz)
									dst.resize(sz);
								if (SUCCEEDED(this->BmpSrc->CopyPixels(nullptr, w * 4, sz, dst.data())))
									return true;
								else
									::MessageBoxW(this->hWnd, L"Failed to copy pixels from the source image frame.", L"Error", MB_OK);
							}
							else
								::MessageBoxW(this->hWnd, L"Failed to get the size of the source image frame.", L"Error", MB_OK);
						}
						else
							::MessageBoxW(this->hWnd, L"Failed to create a bitmap from a WIC bitmap.",
							L"Error", MB_OK);
					}
					else
						::MessageBoxW(this->hWnd, L"Failed to create device dependent resources.",
						L"Error", MB_OK);
				}
				else
					::MessageBoxW(this->hWnd, L"Failed to convert the source image frame.", L"Error", MB_OK);
			}
			else
				::MessageBoxW(this->hWnd, L"Failed to create a format converter.", L"Error", MB_OK);
		}
		else
			::MessageBoxW(this->hWnd, L"Failed to get an image frame from a WIC decoder.", L"Error", MB_OK);
	}
	else
		::MessageBoxW(this->hWnd, L"Failed to create a decoder for a file.", L"Error", MB_OK);
	return false;
}

bool MainWindow::CreateResouces(void)
{
	HRESULT result = ::D2D1CreateFactory(::D2D1_FACTORY_TYPE_SINGLE_THREADED, &this->D2Factory);
	if (SUCCEEDED(result))
	{
		result = ::CoCreateInstance(::CLSID_WICImagingFactory, nullptr, CLSCTX_ALL, IID_PPV_ARGS(&this->WicFactory));
		return SUCCEEDED(result) ? true : false;
	}
	else
		return false;
}

bool MainWindow::CreateDeviceResources(void)
{
	if (this->RenderTarget != nullptr)
		return true;	// If render target is already created, then just return true.
	else
	{
		// Create render target for the given client area.
		::RECT rect;
		if (::GetClientRect(this->hWnd, &rect))
		{
			::D2D1_SIZE_U sz = D2D1::SizeU(rect.right - rect.left, rect.bottom - rect.top);
			HRESULT result = this->D2Factory->CreateHwndRenderTarget(
				D2D1::RenderTargetProperties(),
				D2D1::HwndRenderTargetProperties(this->hWnd, sz),
				&this->RenderTarget);
			if (SUCCEEDED(result))
				return true;
		}
		return false;
	}
}

void MainWindow::ReleaseResources(void)
{
	SafeRelease(this->D2Factory);
	SafeRelease(this->BmpSrc);
	SafeRelease(this->WicFactory);
}

void MainWindow::ReleaseDeviceResources(void)
{
	SafeRelease(this->RenderTarget);
	SafeRelease(this->Bmp);
}

void MainWindow::OnPaint(void)
{
	if (this->CreateDeviceResources())
	{
		::PAINTSTRUCT ps;
		::BeginPaint(this->hWnd, &ps);

		if (!(this->RenderTarget->CheckWindowState() & ::D2D1_WINDOW_STATE_OCCLUDED))
		{
			this->RenderTarget->BeginDraw();

			// If Direct2D bitmap had been released due to divice loss, recreate it
			// from source bitmap.
			HRESULT result(S_OK);
			if (this->BmpSrc != nullptr && this->Bmp == nullptr)
				result = this->RenderTarget->CreateBitmapFromWicBitmap(this->BmpSrc, &this->Bmp);
			if (SUCCEEDED(result) && this->Bmp != nullptr)
			{
				// Draw an image and scale it to the current window size.
				const ::D2D1_SIZE_F sz = this->RenderTarget->GetSize();
				::D2D1_RECT_F rect = D2D1::RectF(0.0f, 0.0f, sz.width, sz.height);
				this->RenderTarget->DrawBitmap(this->Bmp, rect);
			}
			result = this->RenderTarget->EndDraw();
			if (FAILED(result) || result == D2DERR_RECREATE_TARGET)
				this->ReleaseDeviceResources();
		}

		::EndPaint(this->hWnd, &ps);
	}
}

void MainWindow::OnSize(LPARAM lParam)
{
	if (this->RenderTarget != nullptr)
	{
		::D2D1_SIZE_U sz = D2D1::SizeU(LOWORD(lParam), HIWORD(lParam));
		if (FAILED(this->RenderTarget->Resize(sz)))
			this->ReleaseDeviceResources();
		else
			// Triggers WM_PAINT for the entire client area.
			::InvalidateRect(this->hWnd, nullptr, FALSE);
	}
	// If render target is not already created, then just silently finish the function.
}


int WINAPI wWinMain(HINSTANCE hInstance, HINSTANCE, wchar_t *cmdLine, int cmdShow)
{
	// Initialize COM library.
	if (SUCCEEDED(::CoInitializeEx(nullptr, ::COINIT_APARTMENTTHREADED | ::COINIT_DISABLE_OLE1DDE)))
	{
		// NOTE: Wrap MainWindow object with {}, so all member sources are already released
		// before calling CoUninitialize().
		{
			MainWindow win;
			if (!win.Create(L"Image viewer with Direct2D and WIC technology", WS_OVERLAPPEDWINDOW))
				return 0;
			::ShowWindow(win.Window(), cmdShow);
			RunMessageLoop();
		}
		::CoUninitialize();
	}

	return 0;
}
