#include "fileio.h"
#ifdef _WIN32
#include <Windows.h>
#include <fileapi.h>
#include <pathcch.h>
#else
#include <unistd.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <fts.h>
#endif
#include <fmt/format.h>
#include <cassert>
#include <fmt/xchar.h>

namespace lxd {
	bool FileExists(const Char* path) {
#ifdef _WIN32
		return GetFileAttributesW(path) != INVALID_FILE_ATTRIBUTES;
#else
		return access(path, F_OK) == 0;
#endif
	}

	bool openModeCanCreate(int openMode) {
		// WriteOnly can create, but only when ExistingOnly isn't specified.
		// ReadOnly by itself never creates.
		return (openMode & OpenMode::WriteOnly) && !(openMode & OpenMode::ExistingOnly);
	}

	Handle OpenFile(const Char* path, int openMode) {
#ifdef _WIN32
		// All files are opened in share mode (both read and write).
		DWORD shareMode = FILE_SHARE_READ | FILE_SHARE_WRITE;
		int accessRights = 0;
		if(openMode & OpenMode::ReadOnly)
			accessRights |= GENERIC_READ;
		if(openMode & OpenMode::WriteOnly)
			accessRights |= GENERIC_WRITE;
		// WriteOnly can create files, ReadOnly cannot.
		DWORD creationDisp = (openMode & OpenMode::NewOnly)
			? CREATE_NEW
			: openModeCanCreate(openMode)
			? OPEN_ALWAYS
			: OPEN_EXISTING;

		CREATEFILE2_EXTENDED_PARAMETERS param = {};
		param.dwSize = sizeof(CREATEFILE2_EXTENDED_PARAMETERS);
		param.dwFileAttributes = FILE_ATTRIBUTE_NORMAL;
		param.dwFileFlags = FILE_FLAG_SEQUENTIAL_SCAN;
		param.dwSecurityQosFlags = SECURITY_ANONYMOUS;
		HANDLE hFile = CreateFile2(path, accessRights, shareMode, creationDisp, &param);
		// Append or Truncate
		if(openMode & OpenMode::Append) {
			LONG lFileHigh = 0;
			SetFilePointer(hFile, 0, &lFileHigh, FILE_END);
		} else if(openMode & OpenMode::Truncate) {
			SetFilePointer(hFile, 0, NULL, FILE_BEGIN);
			SetEndOfFile(hFile);
		}

		return hFile;
#else
		int accessRights = 0;
		if(openMode & OpenMode::ReadOnly)
			accessRights |= O_RDONLY;
		if(openMode & OpenMode::WriteOnly)
			accessRights |= O_WRONLY;
		if(openMode & OpenMode::ReadWrite)
			accessRights |= O_RDWR;
		if(openMode & OpenMode::Truncate)
			accessRights |= O_TRUNC;
		if(openMode & OpenMode::Append)
			accessRights |= O_APPEND;
		Handle handle;
		handle.fd = open(path, accessRights, S_IRUSR | S_IWUSR);
		return handle;
#endif
	}

	bool CloseFile(Handle handle) {
#ifdef _WIN32
		return CloseHandle(handle);
#else
		return close(handle.fd) == 0;
#endif
	}

	bool WriteFile(const Char* path, char const* buffer, size_t bufferSize) {
#ifdef _WIN32
		// open the file
		auto handle = OpenFile(path, OpenMode::WriteOnly | OpenMode::Truncate);
		if(INVALID_HANDLE_VALUE == handle) {
			return false;
		}
		// get file size
		LARGE_INTEGER size;
		if(!GetFileSizeEx(handle, &size)) {
			CloseHandle(handle);
			return false;
		}
		// write
		if(!::WriteFile(handle, buffer, static_cast<DWORD>(bufferSize), nullptr, nullptr)) {
			CloseHandle(handle);
			return false;
		}
		CloseFile(handle);
		return true;
#else
		bool ok = false;
		FILE* file = fopen(path, "w");
		if(file) {
			if(fwrite(buffer, 1, bufferSize, file))
				ok = true;
		}
		fclose(file);
		return ok;
#endif
	}

	std::string ReadFile(const Char* path) {
#ifdef _WIN32
		// open the file
		auto handle = OpenFile(path, OpenMode::ReadOnly | OpenMode::ExistingOnly);
		if(INVALID_HANDLE_VALUE == handle) {
			return {};
		}
		// get file size
		LARGE_INTEGER size;
		if(!GetFileSizeEx(handle, &size)) {
			CloseHandle(handle);
			return {};
		}
		// read
		std::string buffer;
		buffer.resize(size.QuadPart);
		if(!::ReadFile(handle, buffer.data(), static_cast<DWORD>(buffer.size()), nullptr, nullptr)) {
			CloseHandle(handle);
			return {};
		}

		CloseFile(handle);
		return buffer;
#else
		std::string buffer;
		FILE* file = fopen(path, "r");
		if(file) {
			struct stat info;
			if(fstat(fileno(file), &info) == 0) {
				buffer.resize(info.st_size);
				fread((void*)buffer.data(), buffer.size(), 1, file);
			}
		}
		fclose(file);
		return buffer;
#endif
	}

	bool RemoveFile(const Char* path) {
#ifdef _WIN32
		return DeleteFile(path) != 0;
#else
		return remove(path) == 0;
#endif
	}

	bool CreateDir(const Char* path) {
#ifdef _WIN32
		return ::CreateDirectoryW(path, nullptr);
#else
		return mkdir(path, S_IRWXU | S_IRWXG) == 0;
#endif
	}

	bool CreateDirRecursive(const String& path) {
#ifdef _WIN32
		if (DirExists(path)) {
			return true;
		}

		if (path.empty()) {
			return false;
		}

		size_t searchOffset = 0;
		do {
			auto tokenPos = path.find_first_of(L"\\/", searchOffset);
			// treat the entire path as a folder if no folder separator not found
			if (tokenPos == std::wstring::npos) {
				tokenPos = path.size();
			}

			std::wstring subdir = path.substr(0, tokenPos);

			if (!subdir.empty() && !DirExists(subdir.c_str()) && !CreateDirectory(subdir.c_str(), nullptr)) {
				return false; // return error if failed creating dir
			}
			searchOffset = tokenPos + 1;
		} while (searchOffset < path.size());

		return true;
#else
		if(DirExists(path))
			return true;

		if(path.empty())
			return false;

		size_t searchOffset = 0;
		do {
			auto tokenPos = path.find_first_of('/', searchOffset);
			// treat the entire path as a folder if no folder separator not found
			if(tokenPos == std::string::npos) {
				tokenPos = path.size();
			}

			std::string subdir = path.substr(0, tokenPos);

			if(!subdir.empty() && !DirExists(subdir.c_str()) && !CreateDir(subdir.c_str())) {
				return false; // return error if failed creating dir
			}
			searchOffset = tokenPos + 1;
		} while(searchOffset < path.size());

		return true;
#endif
	}

	int DeleteDir(StringView path, bool bDeleteSubdirectories) {
#ifdef _WIN32
		bool				bSubdirectory = false; // Flag, indicating whether
												   // subdirectories have been found
		HANDLE				hFile;                 // Handle to directory
		std::wstring		strFilePath;           // Filepath
		std::wstring		strPattern;            // Pattern
		WIN32_FIND_DATAW	FileInformation;       // File information


		strPattern = fmt::format(L"{0}{1}", path, L"\\*.*");
		hFile = ::FindFirstFileW(strPattern.c_str(), &FileInformation);
		if(hFile != INVALID_HANDLE_VALUE) {
			do {
				if(FileInformation.cFileName[0] != '.') {
					strFilePath.erase();
					strFilePath = fmt::format(L"{0}{1}{2}", path, L"\\", FileInformation.cFileName);

					if(FileInformation.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
						if(bDeleteSubdirectories) {
							// Delete subdirectory
							int iRC = DeleteDir(strFilePath, bDeleteSubdirectories);
							if(iRC)
								return iRC;
						} else
							bSubdirectory = true;
					} else {
						// Set file attributes
						if(::SetFileAttributesW(strFilePath.c_str(),
											   FILE_ATTRIBUTE_NORMAL) == FALSE)
							return ::GetLastError();

						// Delete file
						if(::DeleteFileW(strFilePath.c_str()) == FALSE)
							return ::GetLastError();
					}
				}
			} while(::FindNextFileW(hFile, &FileInformation) == TRUE);

			// Close handle
			::FindClose(hFile);

			DWORD dwError = ::GetLastError();
			if(dwError != ERROR_NO_MORE_FILES)
				return dwError;
			else {
				if(!bSubdirectory) {
					// Set directory attributes
					if(::SetFileAttributesW(path.data(),
										   FILE_ATTRIBUTE_NORMAL) == FALSE)
						return ::GetLastError();

					// Delete directory
					if(::RemoveDirectoryW(path.data()) == FALSE)
						return ::GetLastError();
				}
			}
		}

		return 0;
#else
		if(!bDeleteSubdirectories)
			return rmdir(path.data()) == 0;
		else {
			
		}
		return -1;
#endif
	}

	bool DirExists(StringView path) {
#ifdef _WIN32
		DWORD attrib = GetFileAttributesW(path.data());
		return (attrib != INVALID_FILE_ATTRIBUTES) && (attrib & FILE_ATTRIBUTE_DIRECTORY);
#else
		struct stat statbuf;
		bool isDir = false;

		if(stat(path.data(), &statbuf) != -1) {
			if(S_ISDIR(statbuf.st_mode)) {
				isDir = true;
			}
		} else {
			/*
				here you might check errno for the reason, ENOENT will mean the path
				was bad, ENOTDIR means part of the path is not a directory, EACCESS
				would mean you can't access the path. Regardless, from the point of
				view of your app, the path is not a directory if stat fails.
			*/
		}
		return isDir;
#endif
	}

	 bool ListDir(StringView path, std::vector<String>& result, bool recursive, const Char* filter) {
#ifdef _WIN32
		auto searchPath = fmt::format(L"{}\\*", path);
		WIN32_FIND_DATAW FindFileData;
		HANDLE hFind = FindFirstFileW(searchPath.data(), &FindFileData);
		if(hFind == INVALID_HANDLE_VALUE) {
			return false;
		}

		static std::wstring currentPath;

		do {
			if(FindFileData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
				if(!wcscmp(L".", FindFileData.cFileName) || !wcscmp(L"..", FindFileData.cFileName)) {
					continue;
				}
				if(recursive) {
					auto searchPath2 = fmt::format(L"{}\\{}", path, FindFileData.cFileName);
					currentPath = FindFileData.cFileName;
					ListDir(searchPath2.data(), result, recursive, filter);
				}
			} else {
				if(nullptr == filter) {
					if(currentPath.empty()) {
						result.push_back(FindFileData.cFileName);
					} else {
						result.push_back(fmt::format(L"{}\\{}", currentPath, FindFileData.cFileName));
					}
				} else {
					int lenFilter = lstrlenW(filter);
					int lenName = lstrlenW(FindFileData.cFileName);
					if(lenFilter < lenName) {
						if(lstrcmpiW(filter, FindFileData.cFileName + (lenName - lenFilter)) == 0) {
							if(currentPath.empty()) {
								result.push_back(FindFileData.cFileName);
							} else {
								result.push_back(fmt::format(L"{}\\{}", currentPath, FindFileData.cFileName));
							}
						}
					}
				}
			}
		} while(FindNextFileW(hFind, &FindFileData) != 0);

		currentPath.clear();
		FindClose(hFind);

		return true;
#else
		 // open
		 char* pathes[] {(char*)path.data(), nullptr};
		 FTS* fts = fts_open(pathes, FTS_PHYSICAL | FTS_NOCHDIR | FTS_XDEV, nullptr);
		 if(!fts)
			 return false;
		 // iterate
		 while(FTSENT* p = fts_read(fts)) {
			 switch(p->fts_info) {
				 case FTS_DP:
					 break;
				 case FTS_F:
					 result.push_back(p->fts_path);
					 break;
				 case FTS_SL:
				 case FTS_SLNONE:
					 break;
			 }
		 }
		 fts_close(fts);
		 return true;
#endif
	 }

	 String GetExePath() {
#ifdef _WIN32
		 // Retrieve fully qualified module pathname
		 std::wstring buffer(MAX_PATH, 0);
		 DWORD cbSize = ::GetModuleFileNameW(nullptr, buffer.data(),
											 static_cast<DWORD>(buffer.size()));
		 assert(cbSize < MAX_PATH);

		 // Remove filename from fully qualified pathname
		 auto ok = ::PathCchRemoveFileSpec(buffer.data(), buffer.size());
		 assert(S_OK == ok);
		 wchar_t* pEnd;
		 ok = PathCchAddBackslashEx(buffer.data(), cbSize, &pEnd, nullptr);
		 assert(S_OK == ok);
		 buffer.resize(pEnd - buffer.data());
		 return buffer;
#else
		 char buffer[PATH_MAX];
		 if(getcwd(buffer, PATH_MAX)) {
			 return String(buffer);
		 }
		 return {};
#endif
	 }

	 File::File(const StringView& path, int mode) {
#ifdef _WIN32
		 assert(path.size() > 3); // D:\\1
		 auto offset = path.find_last_of(L"\\");
		 if(offset != std::wstring::npos) {
			 auto middleDir = path.substr(0, offset);
			 CreateDirRecursive(middleDir);
		 }
		 _handle = OpenFile(path.data(), mode);
		 assert(INVALID_HANDLE_VALUE != _handle);
		 // get file size
		 [[maybe_unused]] bool ok = GetFileSizeEx(_handle, reinterpret_cast<PLARGE_INTEGER>(&_size));
		 assert(ok);
#else
		 auto offset = path.find_last_of('/');
		 if(offset != std::string_view::npos) {
			 std::string dir(path.begin(), path.begin() + offset);
			 CreateDirRecursive(dir);
		 }
		 _handle = OpenFile(path.data(), mode);
		 struct stat st;
		 stat(path.data(), &st);
		 _size = st.st_size;
#endif
	 }

	 File::~File() {
		 CloseFile(_handle);
	 }

	 bool File::seek(long long distance, SeekMode mode, long long* newPtr) {
#ifdef _WIN32
		 _LARGE_INTEGER _distance;
		 _distance.QuadPart = distance;
		 return SetFilePointerEx(_handle, _distance, reinterpret_cast<PLARGE_INTEGER>(newPtr), mode);
#else

		 return false;
#endif
	 }

	 bool File::read(void* buffer, unsigned long nNumberOfBytesToRead, unsigned long* lpNumberOfBytesRead) {
#ifdef _WIN32
		 return ::ReadFile(_handle, buffer, nNumberOfBytesToRead, lpNumberOfBytesRead, nullptr);
#else
		 *lpNumberOfBytesRead = ::read(_handle.fd, buffer, nNumberOfBytesToRead);
		 return *lpNumberOfBytesRead > 0;
#endif
	 }

	 bool File::write(const void* buffer, size_t bufferSize) {
#ifdef _WIN32
		 DWORD bytesWritten = 0;
		 if (::WriteFile(_handle, buffer, static_cast<DWORD>(bufferSize), &bytesWritten, nullptr)) {
			 _size += bytesWritten;
			return true;
		 }
		return false;
#else
		 return ::write(_handle.fd, buffer, bufferSize) > 0;
#endif
	 }

	 bool File::write(std::string_view buffer) {
#ifdef _WIN32
		 DWORD bytesWritten = 0;
		 if(::WriteFile(_handle, buffer.data(), static_cast<DWORD>(buffer.size()), &bytesWritten, nullptr)) {
			 _size += bytesWritten;
			 return true;
		 }
		 return false;
#else
		 return false;
#endif
	 }

	 struct tm File::getLastWriteTime() {
#ifdef _WIN32
		 FILETIME ftCreate, ftAccess, ftWrite;
		 SYSTEMTIME stUTC, stLocal;

		 // Retrieve the file times for the file.
		 [[maybe_unused]] bool ok = GetFileTime(_handle, &ftCreate, &ftAccess, &ftWrite);
		 assert(ok);

		 // Convert the last-write time to local time.
		 FileTimeToSystemTime(&ftWrite, &stUTC);
		 SystemTimeToTzSpecificLocalTime(NULL, &stUTC, &stLocal);

		 return tm {
			 stLocal.wSecond, stLocal.wMinute, stLocal.wHour, 
			 stLocal.wDay, stLocal.wMonth - 1, stLocal.wYear - 1900, 
			 stLocal.wDayOfWeek, 0, 0 
		 };
#else
		 return {};
#endif
	 }

	 bool File::isOlderThan(struct tm otherTM) {
#ifdef _WIN32
		 tm lastWT = getLastWriteTime();
		 return lastWT.tm_year < otherTM.tm_year ||
			 lastWT.tm_mon < otherTM.tm_mon ||
			 lastWT.tm_mday < otherTM.tm_mday ||
			 lastWT.tm_hour < otherTM.tm_hour ||
			 lastWT.tm_min < otherTM.tm_min ||
			 lastWT.tm_sec < otherTM.tm_sec;
#else
		 return false;
#endif
	 }
}