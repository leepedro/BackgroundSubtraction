// Windows header files.
#include <Windows.h>
#include <wincodec.h>
#include <ShlObj.h>

// Standard C header files.
#include <ctime>

// Standard C++ header files.
#include <string>
#include <vector>
#include <iostream>
#include <algorithm>
#include <deque>

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
							::MessageBoxW(nullptr, path_folder, L"Selected Folder", MB_OK);//

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
								}									
								while (::FindNextFileW(hFindFile, &find_data));

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

// Load an image file into a std::vector<byte> where each pixel consists of FOUR continuous elements.
// This function interprets all compatible image files in 32bit BGRA.
void LoadImageFile(const std::wstring &pathSrc, std::vector<unsigned char> &data, ::IWICImagingFactory *wic_factory)
{
	// Decode a source image file.
	::IWICBitmapDecoder *decoder(nullptr);
	if (SUCCEEDED(wic_factory->CreateDecoderFromFilename(pathSrc.c_str(), nullptr, GENERIC_READ,
		::WICDecodeMetadataCacheOnDemand, &decoder)))
	{
		// Get a frame.
		::IWICBitmapFrameDecode *frame(nullptr);
		if (SUCCEEDED(decoder->GetFrame(0, &frame)))
		{
			// Convert the source image frame to 32bit BGRA.
			::IWICFormatConverter *format_converter(nullptr);
			if (SUCCEEDED(wic_factory->CreateFormatConverter(&format_converter)))
			{
				if (SUCCEEDED(format_converter->Initialize(frame, ::GUID_WICPixelFormat32bppPBGRA, ::WICBitmapDitherTypeNone,
					nullptr, 0.0, ::WICBitmapPaletteTypeCustom)))
				{
					unsigned int width, height;
					//std::vector<unsigned char> buffer;
					if (SUCCEEDED(format_converter->GetSize(&width, &height)))
					{
						// Set the size with unsigned int instead of ::size_t because ::size_t (== unsigned long) can be wider than unsigned int.
						unsigned int sz = width * height * 4;
						if (data.size() != sz)
							data.resize(sz);
						if (SUCCEEDED(format_converter->CopyPixels(nullptr, width * 4, sz, data.data())))
							std::wclog << sz << L" bytes are copied." << std::endl;
						else
							::MessageBoxW(nullptr, L"Failed to copy pixels from the source image frame.", L"Error", MB_OK);
					}
					else
						::MessageBoxW(nullptr, L"Failed to get the size of the source image frame.", L"Error", MB_OK);
				}
				else
					::MessageBoxW(nullptr, L"Failed to convert the source image frame.", L"Error", MB_OK);

				format_converter->Release();
			}
			else
				::MessageBoxW(nullptr, L"Failed to create a format converter.", L"Error", MB_OK);

			frame->Release();
		}
		else
			::MessageBoxW(nullptr, L"Failed to get an image frame from a WIC decoder.", L"Error", MB_OK);

		decoder->Release();
	}
	else
		::MessageBoxW(nullptr, L"Failed to create a decoder for a file.", L"Error", MB_OK);

}

void BGRAtoGray(const std::vector<unsigned char> &src, std::vector<float> &dst)
{
	if (dst.size() != (src.size() / 4))
		dst.resize(src.size() / 4, 0.0f);

	auto it_src = src.cbegin();
	for (auto it_dst = dst.begin(); it_dst != dst.end(); ++it_dst)
	{
		*it_dst = *it_src++;	// B
		*it_dst += *it_src++;	// G
		*it_dst += *it_src++;	// R
		*it_dst /= 3.0f;
		++it_src;				// Skip A
	}
	//std::for_each(dst.begin(), dst.end(), [&it_src](double &value)
	//{
	//	value = *it_src++;		// B
	//	value += *it_src++;		// G
	//	value += *it_src++;		// R
	//	value /= 3.0;
	//	++it_src;				// Skip A
	//});	
}

void ComputeMean(const std::deque<std::vector<unsigned char>> &buffer, std::vector<double> &result)
{
	// Initialize the output data based on the size of the first vector in the input buffer.
	::size_t sz = buffer.cbegin()->size();
	if (result.size() != sz)
		result.resize(sz, 0.0);

	// Accumulate all vectors in the input buffer to the output data.
	for (const auto &data : buffer)
		std::transform(data.cbegin(), data.cend(), result.begin(), result.begin(), std::plus<double>());

	// Divide the output data by the length of the input buffer. 
	const double NUM_FRMS = static_cast<double>(buffer.size());
	std::for_each(result.begin(), result.end(), [NUM_FRMS](double &value) { value /= NUM_FRMS; });
}

void ComputeMean(const std::deque<std::vector<float>> &buffer, std::vector<float> &result)
{
	// Initialize the output data based on the size of the first vector in the input buffer.
	::size_t sz = buffer.cbegin()->size();
	if (result.size() != sz)
		result.resize(sz, 0.0f);

	// Accumulate all vectors in the input buffer to the output data.
	for (const auto &data : buffer)
		std::transform(data.cbegin(), data.cend(), result.begin(), result.begin(), std::plus<float>());

	// Divide the output data by the length of the input buffer. 
	const float NUM_FRMS = static_cast<float>(buffer.size());
	std::for_each(result.begin(), result.end(), [NUM_FRMS](float &value) { value /= NUM_FRMS; });
}


void ComputeDiff(const std::vector<unsigned char> &a, const std::vector<double> &b, std::vector<double> &result)
{
	if (result.size() != b.size())
		result.resize(b.size());
	//std::transform(a.cbegin(), a.cend(), b.cbegin(), result.begin(), std::minus<double>());
	std::transform(a.cbegin(), a.cend(), b.cbegin(), result.begin(), [](unsigned char a_, double b_) { return std::abs(a_ - b_); });
}

void ComputeDiff(const std::vector<float> &a, const std::vector<float> &b, std::vector<float> &result)
{
	if (result.size() != a.size())
		result.resize(a.size());
	//std::transform(a.cbegin(), a.cend(), b.cbegin(), result.begin(), std::minus<double>());
	std::transform(a.cbegin(), a.cend(), b.cbegin(), result.begin(), [](float a_, float b_) { return std::abs(a_ - b_); });
}

int main(void)
{
	// NOTE: if multi-threading option is selected for COM initialization, it can not recognize files in the user library. Don't know why.
	if (SUCCEEDED(::CoInitializeEx(nullptr, ::COINIT_APARTMENTTHREADED | ::COINIT_DISABLE_OLE1DDE)))
	//if (SUCCEEDED(::CoInitializeEx(nullptr, ::COINIT_MULTITHREADED | ::COINIT_DISABLE_OLE1DDE)))
	{
		::IWICImagingFactory *wic_factory(nullptr);	// __uuidof(IWICImagingFactory)
		if (SUCCEEDED(::CoCreateInstance(::CLSID_WICImagingFactory, nullptr, CLSCTX_ALL, IID_PPV_ARGS(&wic_factory))))
		{
			// Load which folder to read files.
			std::vector<std::wstring> filenames;
			std::wstring path_folder;
			LoadFileList(path_folder, filenames);

			auto t_start = ::clock();
			const ::size_t MAX_BUFFER_LENGTH(5);
			std::deque<std::vector<float>> buffer;		
			std::vector<unsigned char> src_data;
			std::vector<float> avg, diff;
			for (const auto &filename: filenames)
			{
				// Load an image file.
				std::wstring path_src = path_folder + L"\\" + filename;
				//std::vector<unsigned char> src_data;
				LoadImageFile(path_src, src_data, wic_factory);

				std::vector<float> data;
				BGRAtoGray(src_data, data);

				// Push the data to a buffer.
				if (buffer.size() == MAX_BUFFER_LENGTH)
					buffer.pop_front();
				buffer.push_back(std::move(data));

				// Do something.
				//std::vector<float> avg, diff;
				ComputeMean(buffer, avg);
				ComputeDiff(buffer.back(), avg, diff);
			}

			auto t_end = ::clock();
			auto sec_total = static_cast<double>(t_end - t_start) / CLOCKS_PER_SEC;
			std::clog << "Total computation time = " << sec_total << " (sec)" << std::endl;

			wic_factory->Release();
		}
		else
			::MessageBoxW(nullptr, L"Failed to instantiate a WIC factory.", L"Error", MB_OK);

		::CoUninitialize();
	}
}