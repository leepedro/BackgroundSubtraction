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
#include <numeric>

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
							//::MessageBoxW(nullptr, path_folder, L"Selected Folder", MB_OK);

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
void LoadImageFile(const std::wstring &pathSrc, std::vector<unsigned char> &dst, ::size_t &width, ::size_t &height, ::IWICImagingFactory *wicFactory)
{
	// Decode a source image file.
	::IWICBitmapDecoder *decoder(nullptr);
	if (SUCCEEDED(wicFactory->CreateDecoderFromFilename(pathSrc.c_str(), nullptr, GENERIC_READ,
		::WICDecodeMetadataCacheOnDemand, &decoder)))
	{
		// Get a frame.
		::IWICBitmapFrameDecode *frame(nullptr);
		if (SUCCEEDED(decoder->GetFrame(0, &frame)))
		{
			// Convert the source image frame to 32bit BGRA.
			::IWICFormatConverter *format_converter(nullptr);
			if (SUCCEEDED(wicFactory->CreateFormatConverter(&format_converter)))
			{
				if (SUCCEEDED(format_converter->Initialize(frame, ::GUID_WICPixelFormat32bppPBGRA, ::WICBitmapDitherTypeNone,
					nullptr, 0.0, ::WICBitmapPaletteTypeCustom)))
				{
					unsigned int w, h;
					if (SUCCEEDED(format_converter->GetSize(&w, &h)))
					{
						// Set the size with unsigned int instead of ::size_t because ::size_t (== unsigned long) can be wider than unsigned int.
						unsigned int sz = w * h * 4;
						if (dst.size() != sz)
							dst.resize(sz);
						if (FAILED(format_converter->CopyPixels(nullptr, w * 4, sz, dst.data())))
							::MessageBoxW(nullptr, L"Failed to copy pixels from the source image frame.", L"Error", MB_OK);
						width = w;
						height = h;
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

void SaveImageFile(const std::wstring &pathDst, std::vector<unsigned char> &src, unsigned int width, unsigned int height, ::IWICImagingFactory *wicFactory)
{
	::IWICStream *stream(nullptr);
	if (SUCCEEDED(wicFactory->CreateStream(&stream)))
	{
		if (SUCCEEDED(stream->InitializeFromFilename(pathDst.c_str(), GENERIC_WRITE)))
		{
			::IWICBitmapEncoder *encoder(nullptr);
			if (SUCCEEDED(wicFactory->CreateEncoder(::GUID_ContainerFormatBmp, nullptr, &encoder)))
			{
				if (SUCCEEDED(encoder->Initialize(stream, ::WICBitmapEncoderNoCache)))
				{
					::IWICBitmapFrameEncode *bitmapFrame(nullptr);
					//::IPropertyBag2 *propertybag(nullptr);
					if (SUCCEEDED(encoder->CreateNewFrame(&bitmapFrame, nullptr)))
					{
						if (SUCCEEDED(bitmapFrame->Initialize(nullptr)))
						{
							::WICPixelFormatGUID format_guid = ::GUID_WICPixelFormat24bppBGR;
							if (SUCCEEDED(bitmapFrame->SetPixelFormat(&format_guid)))
							{
								if (::IsEqualGUID(format_guid, ::GUID_WICPixelFormat24bppBGR))
								{
									unsigned int stride = (width * 24 + 7) / 8;
									unsigned int sz_buffer = height * stride;
									// TODO: Following line fails.
									if (SUCCEEDED(bitmapFrame->WritePixels(height, stride, sz_buffer, src.data())))
									{
										if (SUCCEEDED(bitmapFrame->Commit()))
										{
											if (FAILED(encoder->Commit()))
												::MessageBoxW(nullptr, L"Failed to commit an encoder.", L"Error", MB_OK);
										}
										else
											::MessageBoxW(nullptr, L"Failed to commit a frame.", L"Error", MB_OK);
									}
									else
										::MessageBoxW(nullptr, L"Failed to write pixels.", L"Error", MB_OK);
								}
								else
									::MessageBoxW(nullptr, L"8bit gray pixel format is not supported.", L"Error", MB_OK);
							}
							else
								::MessageBoxW(nullptr, L"Failed to set pixel format.", L"Error", MB_OK);
						}
						else
							::MessageBoxW(nullptr, L"Failed to initialize a frame.", L"Error", MB_OK);

						bitmapFrame->Release();
						//propertybag->Release();
					}
					else
						::MessageBoxW(nullptr, L"Failed to create a frame.", L"Error", MB_OK);
				}
				else
					::MessageBoxW(nullptr, L"Failed to initialize an encoder for a stream.", L"Error", MB_OK);

				encoder->Release();
			}
			else
				::MessageBoxW(nullptr, L"Failed to create an encoder.", L"Error", MB_OK);
		}
		else
			::MessageBoxW(nullptr, L"Failed to initialize a stream from filename.", L"Error", MB_OK);

		stream->Release();
	}
	else
		::MessageBoxW(nullptr, L"Failed to create a stream for a file.", L"Error", MB_OK);
}

// Converts byte BGRA image into a single-channel image by copying only blue channel. (kind of cheating)
void BGRAtoGray_(const std::vector<unsigned char> &src, std::vector<float> &dst)
{
	if (dst.size() != (src.size() / 4))
		dst.resize(src.size() / 4);

	auto it_src = src.cbegin();
	for (auto it_dst = dst.begin(), it_end = dst.end(); it_dst != it_end; ++it_dst, it_src += 4)
		*it_dst = *it_src;	// B
}

// Converts byte BGRA image into a single-channel image by averaging BGR channels. (quite expensive)
void BGRAtoGray(const std::vector<unsigned char> &src, std::vector<float> &dst)
{
	if (dst.size() != (src.size() / 4))
		dst.resize(src.size() / 4);

	auto it_src = src.cbegin();
	// NOTE: Following logic using std::for_each() runs faster than for loop. (GOOD!)
	std::for_each(dst.begin(), dst.end(), [&it_src](float &value)
	{
		value = *it_src++;		// B
		value += *it_src++;		// G
		value += *it_src++;		// R
		value /= 3;
		++it_src;				// Skip A
	});

	//for (auto it_dst = dst.begin(), it_end = dst.end(); it_dst != it_end; ++it_dst)
	//{
	//	*it_dst = *it_src++;	// B
	//	*it_dst += *it_src++;	// G
	//	*it_dst += *it_src++;	// R
	//	*it_dst /= 3;
	//	++it_src;				// Skip A
	//}
}

void GrayToBGR(const std::vector<float> &src, std::vector<unsigned char> &dst)
{
	if (dst.size() != (src.size() * 3))
		dst.resize(src.size() * 3);

	auto it_dst = dst.begin();
	std::for_each(src.cbegin(), src.cend(), [&it_dst](float value)
	{
		auto temp = static_cast<unsigned char>(value);
		*it_dst++ = temp;
		*it_dst++ = temp;
		*it_dst++ = temp;
	});
}

//void ComputeMean(const std::deque<std::vector<unsigned char>> &buffer, std::vector<double> &result)
//{
//	// Initialize the output data based on the size of the first vector in the input buffer.
//	::size_t sz = buffer.cbegin()->size();
//	if (result.size() != sz)
//		result.resize(sz, 0.0);
//
//	// Accumulate all vectors in the input buffer to the output data.
//	for (const auto &data : buffer)
//		std::transform(data.cbegin(), data.cend(), result.begin(), result.begin(), std::plus<double>());
//
//	// Divide the output data by the length of the input buffer. 
//	const double NUM_FRMS = static_cast<double>(buffer.size());
//	std::for_each(result.begin(), result.end(), [NUM_FRMS](double &value) { value /= NUM_FRMS; });
//}

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


//void ComputeDiff(const std::vector<unsigned char> &a, const std::vector<double> &b, std::vector<double> &result)
//{
//	if (result.size() != b.size())
//		result.resize(b.size());
//	//std::transform(a.cbegin(), a.cend(), b.cbegin(), result.begin(), std::minus<double>());
//	std::transform(a.cbegin(), a.cend(), b.cbegin(), result.begin(), [](unsigned char a_, double b_) { return std::abs(a_ - b_); });
//}

void ComputeDiff(const std::vector<float> &a, const std::vector<float> &b, std::vector<float> &result)
{
	if (result.size() != a.size())
		result.resize(a.size());
	//std::transform(a.cbegin(), a.cend(), b.cbegin(), result.begin(), std::minus<double>());
	std::transform(a.cbegin(), a.cend(), b.cbegin(), result.begin(), [](float a_, float b_) { return std::abs(a_ - b_); });
}

void ComputeDiffSq(const std::vector<float> &a, const std::vector<float> &b, std::vector<float> &result)
{
	if (result.size() != a.size())
		result.resize(a.size());
	std::transform(a.cbegin(), a.cend(), b.cbegin(), result.begin(), [](float a_, float b_) { auto temp = a_ - b_; return temp * temp; });
}

void ComputeVar(const std::deque<std::vector<float>> &buffer, const std::vector<float> &mean, std::vector<float> &result)
{
	// Initialize the output data based on the size of the mean vector.
	if (result.size() != mean.size())
		result.resize(mean.size(), 0.0f);

	// Accumulate the squared difference along all vectors in the input buffer.
	std::vector<float> temp;
	for (const auto &data : buffer)
	{
		ComputeDiffSq(data, mean, temp);
		std::transform(temp.cbegin(), temp.cend(), result.begin(), result.begin(), std::plus<float>());
	}

	// Divide the output data by the length of the input buffer. 
	const float NUM_FRMS = static_cast<float>(buffer.size());
	std::for_each(result.begin(), result.end(), [NUM_FRMS](float &value) { value /= NUM_FRMS; });
}

void ComputeStd(const std::deque<std::vector<float>> &buffer, const std::vector<float> &mean, std::vector<float> &result)
{
	// Initialize the output data based on the size of the mean vector.
	if (result.size() != mean.size())
		result.resize(mean.size(), 0.0f);

	// Accumulate the squared difference along all vectors in the input buffer.
	std::vector<float> temp;
	for (const auto &data : buffer)
	{
		ComputeDiffSq(data, mean, temp);
		std::transform(temp.cbegin(), temp.cend(), result.begin(), result.begin(), std::plus<float>());
	}

	// Divide the output data by the length of the input buffer. 
	const float NUM_FRMS = static_cast<float>(buffer.size());
	std::for_each(result.begin(), result.end(), [NUM_FRMS](float &value) { value = std::sqrtf(value / NUM_FRMS); });
}

void Mark(const std::vector<float> &data, const std::vector<float> &mean, const std::vector<float> &std, float th, std::vector<unsigned char> &result)
{
	// Initialize the output data based on the size of the mean vector.
	if (result.size() != data.size())
		result.resize(data.size());

	// Mark elements.
	auto it_data = data.cbegin();
	auto it_mean = mean.cbegin();
	auto it_std = std.cbegin();
	for (auto it_dst = result.begin(), it_end = result.end(); it_dst != it_end; ++it_dst, ++it_data, ++it_mean, ++it_std)
		*it_dst = (std::abs(*it_data - *it_mean) / *it_std) > th ? 0xFF : 0x00;
}

// Load image files, and do nothing else.
void Test0(::IWICImagingFactory *wicFactory, const std::wstring &pathFolder, const std::vector<std::wstring> &filenames)
{
	std::vector<unsigned char> src_data;
	std::vector<float> data;
	::size_t width, height;
	std::vector<unsigned char> out_temp;
	for (const auto &filename : filenames)
	{
		// Load an image file.
		std::wstring path_src = pathFolder + L"\\" + filename;
		LoadImageFile(path_src, src_data, width, height, wicFactory);
		BGRAtoGray_(src_data, data);		
		//GrayToBGR(data, out_temp);
		//SaveImageFile(L"Test.bmp", out_temp, static_cast<unsigned int>(width), static_cast<unsigned int>(height), wicFactory);
	}
}

// 
void Test1(::IWICImagingFactory *wicFactory, const std::wstring &pathFolder, const std::vector<std::wstring> &filenames)
{
	const ::size_t MAX_BUFFER_LENGTH(5);
	std::deque<std::vector<float>> buffer;
	::size_t width, height;
	std::vector<unsigned char> src_data, dst;
	std::vector<float> avg, std;
	for (const auto &filename : filenames)
	{
		// Load an image file.
		std::wstring path_src = pathFolder + L"\\" + filename;
		LoadImageFile(path_src, src_data, width, height, wicFactory);
		std::vector<float> data;
		BGRAtoGray_(src_data, data);

		// Push the data to a buffer.
		if (buffer.size() == MAX_BUFFER_LENGTH)
			buffer.pop_front();
		buffer.push_back(std::move(data));

		// Do something.
		ComputeMean(buffer, avg);
		ComputeStd(buffer, avg, std);
		Mark(buffer.back(), avg, std, 3.5f, dst);
	}

}

// Report total computation time as a log message and a message box.
void ReportTime(::clock_t tStart, ::clock_t tEnd)
{
	auto sec_total = static_cast<double>(tEnd - tStart) / CLOCKS_PER_SEC;
	std::wstring msg_time = L"Total computation time = " + std::to_wstring(sec_total) + L" (sec)";
	std::wclog << msg_time << std::endl;
	::MessageBoxW(nullptr, msg_time.c_str(), L"Completed", MB_OK | MB_ICONINFORMATION);
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

			::clock_t t_start, t_end;

			t_start = ::clock();
			Test0(wic_factory, path_folder, filenames);			
			t_end = ::clock();
			ReportTime(t_start, t_end);

			t_start = ::clock();
			Test1(wic_factory, path_folder, filenames);
			t_end = ::clock();
			ReportTime(t_start, t_end);

			wic_factory->Release();
		}
		else
			::MessageBoxW(nullptr, L"Failed to instantiate a WIC factory.", L"Error", MB_OK);

		::CoUninitialize();
	}
}