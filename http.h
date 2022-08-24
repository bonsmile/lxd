#pragma once

#include "defines.h"
#include <string_view>
#include <vector>

namespace lxd {
    class DLL_PUBLIC HttpRequestSync {
    public:
        HttpRequestSync(const wchar_t* host, unsigned short port, const wchar_t* path, std::string& result, const std::string& post = {},
            const std::vector<std::pair<std::wstring_view, std::wstring_view>>& headers = {});
        ~HttpRequestSync();
    private:
        unsigned long dwSize = 0;
        unsigned long dwDownloaded = 0;
        bool  bResults = false;
        void* hSession = nullptr;
        void* hConnect = nullptr;
        void* hRequest = nullptr;
    };

    class DLL_PUBLIC PostFormdata {
    public:
        PostFormdata(const wchar_t* host, const wchar_t* path, bool https,
                     std::string& result, const std::vector<std::pair<std::string_view, std::string_view>>& pairs,
                     const std::vector<std::pair<std::string_view, std::string_view>>& files = {},
                     const char* authID = nullptr, const char* authSecret = nullptr);
        ~PostFormdata();
    private:
        unsigned long dwSize = 0;
        unsigned long dwDownloaded = 0;
        bool  bResults = false;
        void* hSession = nullptr;
        void* hConnect = nullptr;
        void* hRequest = nullptr;
    };

    class DLL_PUBLIC PostFormUrlencoded {
    public:
        PostFormUrlencoded(const wchar_t* host, unsigned short port, const wchar_t* path, bool https,
                     std::string& result, const std::vector<std::pair<std::string, std::string>>& pairs);
        ~PostFormUrlencoded();
    private:
        unsigned long dwSize = 0;
        unsigned long dwDownloaded = 0;
        bool  bResults = false;
        void* hSession = nullptr;
        void* hConnect = nullptr;
        void* hRequest = nullptr;
    };

    class DLL_PUBLIC PostJson {
    public:
        PostJson(const wchar_t* host, const wchar_t* path, bool https,
                 std::string& result, const std::vector<std::pair<std::string, std::string>>& pairs);
        ~PostJson();
    private:
        unsigned long dwSize = 0;
        unsigned long dwDownloaded = 0;
        void* hSession = nullptr;
        void* hConnect = nullptr;
        void* hRequest = nullptr;
    };

    class DLL_PUBLIC GetJson {
    public:
        GetJson(const wchar_t* host, const wchar_t* path, bool https,
                 std::string& result, const std::vector<std::pair<std::string, std::string>>& pairs,
                 const char* authID = nullptr, const char* authSecret = nullptr);
        ~GetJson();
    private:
        unsigned long dwSize = 0;
        unsigned long dwDownloaded = 0;
        void* hSession = nullptr;
        void* hConnect = nullptr;
        void* hRequest = nullptr;
    };

    std::string DLL_PUBLIC Post(
        const wchar_t* host,
        const wchar_t* path,
        bool https,
        const std::vector<std::pair<std::wstring, std::wstring>>& headers,
        std::string_view body
    );

    std::string DLL_PUBLIC Get(
        const wchar_t* host,
        const wchar_t* path,
        bool https,
        const std::vector<std::pair<std::wstring, std::wstring>>& headers
    );
}
