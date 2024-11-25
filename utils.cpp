#include "utils.h"
#include <cassert>
#ifdef _WIN32
#include <Windows.h> // For Win32 API
#include <Psapi.h>
#include <shlobj_core.h>
#include <atomic>
#else
#include <unistd.h>
#include <libproc.h>
#endif

namespace lxd {
    String GetDirOfExe() {
#ifdef _WIN32
        Char filename[MAX_PATH];
        HANDLE process = GetCurrentProcess();
        GetModuleFileNameExW(process, NULL, filename, MAX_PATH);
        String result = filename;
        result.resize(result.find_last_of(L'\\'));
        return result;
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

    struct TPContext {
	    std::function<void(uint32_t)> func;
	    std::atomic<uint32_t> id;
    };

    static void CALLBACK TPCallback(PTP_CALLBACK_INSTANCE, PVOID context, PTP_WORK) {
	    TPContext* ctxt = (TPContext*)context;
	    const uint32_t id = ctxt->id.fetch_add(1, std::memory_order_relaxed);
	    ctxt->func(id);
    }
#endif

    void RunParallel(uint32_t times, std::function<void(uint32_t)> func) noexcept {
#ifdef _WIN32
	    if (times == 0) {
		    return;
	    }

	    if (times == 1) {
		    return func(0);
	    }

	    TPContext ctxt = {func, 0};
	    PTP_WORK work = CreateThreadpoolWork(TPCallback, &ctxt, nullptr);
	    if (work) {
		    // 在线程池中执行
		    for (uint32_t i = 0; i < times; ++i) {
			    SubmitThreadpoolWork(work);
		    }

		    WaitForThreadpoolWorkCallbacks(work, FALSE);
		    CloseThreadpoolWork(work);
	    } else {
		    // 回退到单线程
		    for (uint32_t i = 0; i < times; ++i) {
			    func(i);
		    }
	    }
#else
	    static_assert(false, "Not implemented!");
#endif // _WIN32
    }

}