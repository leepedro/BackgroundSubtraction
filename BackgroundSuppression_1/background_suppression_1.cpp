// Windows header files.
#include <Windows.h>
#include <wincodec.h>
#include <ShlObj.h>

#include <string>
#include <vector>
#include <iostream>

void LoadFileList(std::wstring &pathFolder, std::vector<std::wstring> &filenames)
{
	::IFileDialog *file_dialog(nullptr);
	if (SUCCEEDED(::CoCreateInstance(__uuidof(::FileOpenDialog), nullptr, CLSCTX_ALL, IID_PPV_ARGS(&file_dialog))))
	{
		::FILEOPENDIALOGOPTIONS options;
		if (SUCCEEDED(file_dialog->GetOptions(&options)))
		{
			if (SUCCEEDED(file_dialog->SetOptions(options | ::FOS_PICKFOLDERS | ::FOS_FORCEFILESYSTEM)))
			{
				// Open a file open dialog.
				HRESULT result = file_dialog->Show(nullptr);
				if (SUCCEEDED(result))
				{
					::IShellItem *selected_item(nullptr);
					if (SUCCEEDED(file_dialog->GetResult(&selected_item)))
					{
						wchar_t *path_folder(nullptr);
						if (SUCCEEDED(selected_item->GetDisplayName(::SIGDN_FILESYSPATH, &path_folder)))
						{
							pathFolder = path_folder;
							::MessageBoxW(nullptr, path_folder, L"Selected Folder", MB_OK);//
							::WIN32_FIND_DATAW find_data;
							std::wstring search_pattern = std::wstring(path_folder) + L"\\*";
							HANDLE hFindFile = ::FindFirstFileW(search_pattern.c_str(), &find_data);
							if (hFindFile != INVALID_HANDLE_VALUE)
							{
								do
								{
									if (find_data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)
									{
										std::wclog << L"Skipping " << find_data.cFileName << std::endl;//
										// Skip {., .., sub-directories} for now.
									}
									else
										filenames.push_back(std::wstring(find_data.cFileName));
								}									
								while (::FindNextFileW(hFindFile, &find_data));
								::FindClose(hFindFile);
							}
							else
								::MessageBoxW(nullptr, L"Failed to get the result from a file dialog.", L"Error", MB_OK);
							
							::CoTaskMemFree(path_folder);
						}
						else
							::MessageBoxW(nullptr, L"Failed to get the path of the selected folder.", L"Error", MB_OK);
						selected_item->Release();
					}
					else
						::MessageBoxW(nullptr, L"Failed to get the result from a file dialog.", L"Error", MB_OK);
				}
				else if (result == ::HRESULT_FROM_WIN32(ERROR_CANCELLED)) {}	// Do nothing if cancelled.
				else
					::MessageBoxW(nullptr, L"Failed to get a response from a file dialog.", L"Error", MB_OK);
			}
			else
				::MessageBoxW(nullptr, L"Failed to set the options of a file dialog.", L"Error", MB_OK);
		}
		else
			::MessageBoxW(nullptr, L"Failed to get the options of a file dialog.", L"Error", MB_OK);
	}
	else
		::MessageBoxW(nullptr, L"Failed to create a file dialog.", L"Error", MB_OK);
}

int main(void)
{
	// NOTE: if multi-threading option is selected for COM initialization, it can not recognize files in user library. Don't know why.
	if (SUCCEEDED(::CoInitializeEx(nullptr, ::COINIT_APARTMENTTHREADED | ::COINIT_DISABLE_OLE1DDE)))
	//if (SUCCEEDED(::CoInitializeEx(nullptr, ::COINIT_MULTITHREADED | ::COINIT_DISABLE_OLE1DDE)))
	{
		std::vector<std::wstring> filenames;
		std::wstring path_folder;
		LoadFileList(path_folder, filenames);
		::CoUninitialize();
	}
}