#include "utils.h"
#include <cassert>
#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <Windows.h> // For Win32 API
#include <Psapi.h>
#include <shlobj_core.h>
#include <PathCch.h>
#else
#include <unistd.h>
#include <libproc.h>
#endif

namespace lxd {
    String GetDirOfExe() {
#ifdef _WIN32
        Char filename[MAX_PATH];
        HANDLE process = GetCurrentProcess();
        auto size = GetModuleFileNameExW(process, NULL, filename, MAX_PATH);
        [[maybe_unused]] auto ok = PathCchRemoveFileSpec(filename, size);
        assert(ok == S_OK);
        return filename;
#else
        int ret;
        pid_t pid = getpid();
        char pathbuf[PROC_PIDPATHINFO_MAXSIZE];
        ret = proc_pidpath(pid, pathbuf, sizeof(pathbuf));
        if(ret <= 0) {
            fprintf(stderr, "PID %d: proc_pidpath ();\n", pid);
            fprintf(stderr, "    %s\n", strerror(errno));
        } else {
            printf("proc %d: %s\n", pid, pathbuf);
        }
        return String(pathbuf);
#endif
    }

    void MsgBox(MsgType msgType, const Char* title, const Char* message) {
#ifdef _WIN32
        UINT type = MB_OK;
        switch(msgType)
        {
            case lxd::Info:
                type |= MB_ICONINFORMATION;
                break;
            case lxd::Warn:
                type |= MB_ICONWARNING;
                break;
            case lxd::Error:
                type |= MB_ICONERROR;
                break;
            default:
                break;
        }
        MessageBox(
            NULL,
            (LPCWSTR)message,
            (LPCWSTR)title,
            type
        );
#else
        // TODO: https://developer.apple.com/documentation/corefoundation/cfusernotification?language=objc
#endif
    }

#ifdef _WIN32
	std::wstring GetPathOfExe() {
        Char filename[MAX_PATH];
        HANDLE process = GetCurrentProcess();
        std::ignore = GetModuleFileNameExW(process, NULL, filename, MAX_PATH);
        return filename;
	}

    std::wstring GetExeName() {
        auto exe = GetPathOfExe();
        auto slash = exe.rfind('\\');
        auto dot = exe.rfind(L".exe");
        assert(slash && dot);
        assert(slash < dot && dot < exe.size());
        return exe.substr(slash + 1, dot - slash - 1);
    }

    std::wstring GetPathOfAppData() {
        PWSTR path = NULL;
        [[maybe_unused]] HRESULT ok = SHGetKnownFolderPath(FOLDERID_LocalAppData, 0, NULL, &path);
        assert(S_OK == ok);
        std::wstring result(path);
        CoTaskMemFree(path);

        return result;
    }

    void SetEnv(std::wstring_view name, int value) {
        HKEY subKey;
        int _value = value;
        RegOpenKeyEx(HKEY_CURRENT_USER, L"Environment", 0, KEY_SET_VALUE, &subKey);
        RegSetValueEx(subKey,
                      name.data(),
                      0,
                      REG_DWORD,
                      reinterpret_cast<const BYTE*>(&_value),
                      sizeof(int));
        RegCloseKey(HKEY_CURRENT_USER);
    }

    void SetEnv(std::wstring_view name, std::wstring_view value) {
        HKEY subKey;
        RegOpenKeyEx(HKEY_CURRENT_USER, L"Environment", 0, KEY_SET_VALUE, &subKey);
        [[maybe_unused]] auto status = RegSetValueEx(subKey,
                                    name.data(),
                                    0,
                                    REG_SZ,
                                    reinterpret_cast<const BYTE*>(value.data()),
                                    static_cast<ULONG>(value.size() * sizeof(wchar_t)));
        assert(status == ERROR_SUCCESS);
        RegCloseKey(HKEY_CURRENT_USER);
    }

    std::variant<int, std::wstring> GetEnv(std::wstring_view name) {
        HKEY subKey;
        DWORD size;
        DWORD type;
        std::variant<int, std::wstring> result;
        RegOpenKeyEx(HKEY_CURRENT_USER, L"Environment", 0, KEY_QUERY_VALUE, &subKey);
        auto status = RegQueryValueEx(subKey, name.data(), 0, NULL, NULL, &size);
        if(ERROR_SUCCESS != status) {

        } else {
            // This environment variable already exists. Assign its value to this
            // EnvVar object.
            auto data = new BYTE[size];
            RegQueryValueEx(subKey, name.data(), 0, &type, data, &size);
            switch(type) {
                case REG_DWORD:
                    result = *reinterpret_cast<int*>(data);
                    break;
                case REG_SZ:
                {
                    std::wstring value = reinterpret_cast<wchar_t*>(data);
                    result = value;
                }
                break;
                default:
                    break;
            }
            delete[] data;
        }
        RegCloseKey(HKEY_CURRENT_USER);

        return result;
    }
#endif
}