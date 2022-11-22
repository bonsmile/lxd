#pragma once

#include "defines.h"
#include <string_view>
#include <vector>
#include <string>
#include <ctime>

namespace lxd {
	enum OpenMode {
		NotOpen = 0x0000,
		ReadOnly = 0x0001,
		WriteOnly = 0x0002,
		ReadWrite = ReadOnly | WriteOnly,
		Append = 0x0004,
		Truncate = 0x0008,
		Text = 0x0010,
		Unbuffered = 0x0020,
		NewOnly = 0x0040,
		ExistingOnly = 0x0080
	};

	enum SeekMode {
		FileBegin = 0,
		FileCurrent,
		FileEnd
	};

	union Handle {
		void* handle;
		int fd;
	};
	
	DLL_PUBLIC bool FileExists(const Char* path);
	DLL_PUBLIC Handle OpenFile(const Char* path, int mode);
	DLL_PUBLIC bool CloseFile(Handle handle);
	DLL_PUBLIC bool WriteFile(const Char* path, char const* buffer, size_t bufferSize);
	DLL_PUBLIC std::string ReadFile(const Char* path);
	DLL_PUBLIC bool RemoveFile(const Char* path);

	DLL_PUBLIC bool CreateDir(const Char* path);
	DLL_PUBLIC bool CreateDirRecursive(const String& path);
	DLL_PUBLIC int DeleteDir(StringView path, bool bDeleteSubdirectories = true);
	DLL_PUBLIC bool DirExists(StringView path);
	DLL_PUBLIC bool ListDir(StringView path, std::vector<String>& result, bool recursive = false, const Char* filter = nullptr);

	DLL_PUBLIC String GetExePath();

	class DLL_PUBLIC File {
	public:
		File(const StringView& path, int mode);
		~File();
		long long size() { return _size; }
		bool seek(long long distance, SeekMode mode, long long* newPtr = nullptr);
		bool read(void* buffer, unsigned long nNumberOfBytesToRead, unsigned long* lpNumberOfBytesRead = nullptr);
		bool write(const void* buffer, size_t size);
		bool write(std::string_view buffer);
		struct tm getLastWriteTime();
		bool isOlderThan(struct tm);
	private:
		Handle _handle{};
		long long _size{};
	};
}