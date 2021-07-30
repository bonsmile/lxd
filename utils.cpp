#include "utils.h"
#include <cassert>
#include <Windows.h> // For Win32 API
#include <shlobj_core.h>

namespace lxd {
	std::wstring GetPathOfExe() {
        // Get filename with full path for current process EXE
        wchar_t filename[MAX_PATH];
        DWORD result = ::GetModuleFileName(
            nullptr,    // retrieve path of current process .EXE
            filename,
            _countof(filename)
        );
        assert(result);

        return filename;
	}

	std::wstring GetDirOfExe() {
        auto exe = GetPathOfExe();
        auto offset = exe.rfind('\\');
        assert(offset);

        return exe.substr(0, offset);
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
        HRESULT ok = SHGetKnownFolderPath(FOLDERID_LocalAppData, 0, NULL, &path);
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
        auto status = RegSetValueEx(subKey,
                                    name.data(),
                                    0,
                                    REG_SZ,
                                    reinterpret_cast<const BYTE*>(value.data()),
                                    value.size() * sizeof(wchar_t));
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
}