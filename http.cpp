#include "http.h"
#include <Windows.h>
#include <winhttp.h>
#include "debug.h"
#include "encoding.h"
#include "base64.h"
//#include <fmt/xchar.h>
#include <cassert>
#include <fmt/xchar.h>

namespace lxd {
	HttpRequestSync::HttpRequestSync(const wchar_t* host, unsigned short port, const wchar_t* path, std::string& result, const std::string& post,
        const std::vector<std::pair<std::wstring_view, std::wstring_view>>& headers) {
        // Specify an HTTP server.
        hSession = WinHttpOpen(L"lxd with WinHTTP Sync /1.0",
                                         WINHTTP_ACCESS_TYPE_DEFAULT_PROXY,
                                         WINHTTP_NO_PROXY_NAME,
                                         WINHTTP_NO_PROXY_BYPASS,
                                         0);

        // Connect
        if(hSession)
            hConnect = WinHttpConnect(hSession, host,
                                      port, 0);

        // Create an HTTP request handle.
        if(hConnect)
            hRequest = WinHttpOpenRequest(hConnect,
                                          post.empty() ? L"GET" : L"POST",
                                          path,
                                          NULL, WINHTTP_NO_REFERER,
                                          WINHTTP_DEFAULT_ACCEPT_TYPES,
                                          INTERNET_DEFAULT_HTTPS_PORT == port ? WINHTTP_FLAG_SECURE : 0);

        // Send a request.
        if(hRequest) {
            std::wstring header;
            for(auto pair : headers) {
                header.append(fmt::format(L"{}:{}\r\n", pair.first, pair.second));
            }
            bResults = WinHttpSendRequest(hRequest,
                header.empty() ? WINHTTP_NO_ADDITIONAL_HEADERS : header.c_str(),
                (DWORD)header.size(),
                post.empty() ? WINHTTP_NO_REQUEST_DATA : const_cast<char*>(post.c_str()),
                post.empty() ? 0 : static_cast<DWORD>(post.size()),
                post.empty() ? 0 : static_cast<DWORD>(post.size()),
                NULL);
        }

        // End the request.
        if(bResults)
            bResults = WinHttpReceiveResponse(hRequest, NULL);

        // Keep checking for data until there is nothing left.
        if(bResults) {
            do {
                // Check for available data.
                dwSize = 0;
                if(!WinHttpQueryDataAvailable(hRequest, &dwSize))
                    lxd::print("Error {} in WinHttpQueryDataAvailable.\n",
                               GetLastError());

                // Allocate space for the buffer.
                auto pszOutBuffer = new char[dwSize + 1];
                if(!pszOutBuffer) {
                    lxd::print("Out of memory\n");
                    dwSize = 0;
                } else {
                    // Read the data.
                    ZeroMemory(pszOutBuffer, dwSize + 1);
                    if(!WinHttpReadData(hRequest, (LPVOID)pszOutBuffer,
                                        dwSize, &dwDownloaded)) {
                        lxd::print("Error {} in WinHttpReadData.\n", GetLastError());
                    } else {
                        result += pszOutBuffer;
                    }
                    // Free the memory allocated to the buffer.
                    delete[] pszOutBuffer;
                }
            } while(dwSize > 0);
        }


        // Report any errors.
        if(!bResults)
            lxd::print("Error {} has occurred.\n", GetLastError());
	}

    HttpRequestSync::~HttpRequestSync() {
        // Close any open handles.
        if(hRequest) WinHttpCloseHandle(hRequest);
        if(hConnect) WinHttpCloseHandle(hConnect);
        if(hSession) WinHttpCloseHandle(hSession);
    }

    PostFormdata::PostFormdata(const wchar_t* host, const wchar_t* path, bool https,
                               std::string& result, const std::vector<std::pair<std::string_view, std::string_view>>& formData,
                               const std::vector<std::pair<std::string_view, std::string_view>>& files,
                               const char* authID, const char* authSecret) {
        std::wstring header;

        if (authID && authSecret) {
            auto idAndSecret = fmt::format("{}:{}", authID, authSecret);
            auto ecSize = lxd::Base64_EncodeSizeInBytes(idAndSecret.size());
            std::string base64;
            base64.resize(ecSize);
            lxd::Base64_Encode(base64.data(), reinterpret_cast<const unsigned char*>(idAndSecret.data()), idAndSecret.size());
            header = fmt::format(L"Authorization: Basic {}\r\n", lxd::utf8_decode(base64));
        }

        _Post(host, path, https, result, formData, files, header);
    }

    PostFormdata::PostFormdata(
        const wchar_t* host,
        const wchar_t* path,
        bool https,
        std::string& result,
        const std::vector<std::pair<std::string_view, std::string_view>>& formData,
        const std::vector<std::pair<std::string_view, std::string_view>>& files,
        const std::vector<std::pair<std::string_view, std::string_view>>& headers
    ) {
        std::wstring header;
        for (const auto& pair : headers) {
            header.append(fmt::format(L"{}: {}\r\n", lxd::utf8_decode(pair.first), lxd::utf8_decode(pair.second)));
        }

        _Post(host, path, https, result, formData, files, header);
    }

    PostFormdata::~PostFormdata() {
        // Close any open handles.
        if(hRequest) WinHttpCloseHandle(hRequest);
        if(hConnect) WinHttpCloseHandle(hConnect);
        if(hSession) WinHttpCloseHandle(hSession);
    }

    void PostFormdata::_Post(
        const wchar_t* host,
        const wchar_t* path,
        bool https,
        std::string& result,
        const std::vector<std::pair<std::string_view, std::string_view>>& formData,
        const std::vector<std::pair<std::string_view, std::string_view>>& files,
        std::wstring& header
    ) {
        // Fix `skipped by goto` error
        std::vector<std::pair<std::string_view, std::string_view>> additionals;
        
        std::vector<std::string> optionals;
        DWORD dwBytesWritten = 0;
        [[maybe_unused]] size_t checkSize = 0;
        const char* boundary = "--1SUB64X86GK5\r\n";
        const char* disposition = "Content-Disposition: form-data; ";
        const char* newLine = "\r\n";
        const char* finalBody = "--1SUB64X86GK5--\r\n";
        size_t totalSize{};
        // Specify an HTTP server.
        hSession = WinHttpOpen(L"lxd with WinHTTP Sync /1.0",
            WINHTTP_ACCESS_TYPE_DEFAULT_PROXY,
            WINHTTP_NO_PROXY_NAME,
            WINHTTP_NO_PROXY_BYPASS,
            0);

        // Connect
        if (hSession) {
            hConnect = WinHttpConnect(hSession, host,
                INTERNET_DEFAULT_HTTPS_PORT, 0);
        }
        else goto error;


        // Create an HTTP request handle.
        if (hConnect) {
            hRequest = WinHttpOpenRequest(hConnect,
                L"POST",
                path,
                NULL, WINHTTP_NO_REFERER,
                WINHTTP_DEFAULT_ACCEPT_TYPES,
                https ? WINHTTP_FLAG_SECURE : 0);
        }
        else goto error;

        
        // optional key/values
        for (auto const& pair : formData) {
            auto str = fmt::format("{}{}name=\"{}\"\r\n\r\n{}", boundary, disposition, pair.first, pair.second);
            totalSize += str.size();
            optionals.emplace_back(std::move(str));
        }
        //optional = fmt::format("{}{}", optional, boundary);
        // additional
        for (auto const& file : files) {
            auto str = fmt::format("{}{}name=\"StlBinary\"; filename=\"{}\"\r\nContent-Type: application/zip\r\n\r\n", boundary, disposition, file.first);
            totalSize += str.size();
            totalSize += file.second.size();
            additionals.emplace_back(std::make_pair(std::move(str), file.second));
        }
        totalSize += strlen(newLine) * optionals.size();
        totalSize += strlen(newLine) * additionals.size();
        totalSize += strlen(finalBody);

        // Send a request.
        header.append(L"Content-Type:multipart/form-data; boundary=1SUB64X86GK5");
        if (!WinHttpAddRequestHeaders(
            hRequest,
            header.c_str(),
            static_cast<DWORD>(header.size()),
            WINHTTP_ADDREQ_FLAG_ADD
        )) goto error;

        if (!WinHttpSendRequest(hRequest,
            WINHTTP_NO_ADDITIONAL_HEADERS,
            0,
            WINHTTP_NO_REQUEST_DATA,
            0,
            static_cast<DWORD>(totalSize),
            NULL)) goto error;

        for (auto const& optional : optionals) {
            if (!WinHttpWriteData(hRequest, optional.c_str(),
                static_cast<DWORD>(optional.size()),
                &dwBytesWritten)) goto error;
            checkSize += optional.size();
            if (!WinHttpWriteData(hRequest, newLine,
                static_cast<DWORD>(strlen(newLine)),
                &dwBytesWritten)) goto error;
            checkSize += strlen(newLine);
        }
        for (auto const& additional : additionals) {
            if (!WinHttpWriteData(hRequest, additional.first.data(),
                static_cast<DWORD>(additional.first.size()),
                &dwBytesWritten)) goto error;
            checkSize += additional.first.size();
            if (!WinHttpWriteData(hRequest, additional.second.data(),
                static_cast<DWORD>(additional.second.size()),
                &dwBytesWritten)) goto error;
            checkSize += additional.second.size();
            if (!WinHttpWriteData(hRequest, newLine,
                static_cast<DWORD>(strlen(newLine)),
                &dwBytesWritten)) goto error;
            checkSize += strlen(newLine);
        }
        if (!WinHttpWriteData(hRequest, finalBody,
            static_cast<DWORD>(strlen(finalBody)),
            &dwBytesWritten)) goto error;
        checkSize += strlen(finalBody);
        assert(totalSize == checkSize);

        // End the request.
        if (!WinHttpReceiveResponse(hRequest, NULL)) goto error;

        // Keep checking for data until there is nothing left.
        do {
            // Check for available data.
            dwSize = 0;
            if (!WinHttpQueryDataAvailable(hRequest, &dwSize))
                lxd::print("Error {} in WinHttpQueryDataAvailable.\n",
                    GetLastError());

            // Allocate space for the buffer.
            auto pszOutBuffer = new char[dwSize + 1];
            if (!pszOutBuffer) {
                lxd::print("Out of memory\n");
                dwSize = 0;
            }
            else {
                // Read the data.
                ZeroMemory(pszOutBuffer, dwSize + 1);
                pszOutBuffer[dwSize] = '\0';
                if (!WinHttpReadData(hRequest, (LPVOID)pszOutBuffer,
                    dwSize, &dwDownloaded)) {
                    lxd::print("Error {} in WinHttpReadData.\n", GetLastError());
                }
                else {
                    result += pszOutBuffer;
                }
                // Free the memory allocated to the buffer.
                delete[] pszOutBuffer;
            }
        } while (dwSize > 0);

    error:
        if (GetLastError() != 0)
            lxd::print("Error {} has occurred.\n", GetLastError());
    }

    PostFormUrlencoded::PostFormUrlencoded(const wchar_t* host, unsigned short port, const wchar_t* path, bool https,
                               std::string& result, const std::vector<std::pair<std::string, std::string>>& pairs) {
        const wchar_t* contentType = L"Content-Type: application/x-www-form-urlencoded";
        std::string content;
        
        // Specify an HTTP server.
        hSession = WinHttpOpen(L"lxd with WinHTTP Sync /1.0",
                                         WINHTTP_ACCESS_TYPE_DEFAULT_PROXY,
                                         WINHTTP_NO_PROXY_NAME,
                                         WINHTTP_NO_PROXY_BYPASS,
                                         0);

        // Connect
        if(hSession) {
            hConnect = WinHttpConnect(hSession, host,
                                      port, 0);
        } else goto error;


        // Create an HTTP request handle.
        if(hConnect) {
            hRequest = WinHttpOpenRequest(hConnect,
                                          L"POST",
                                          path,
                                          NULL, WINHTTP_NO_REFERER,
                                          WINHTTP_DEFAULT_ACCEPT_TYPES,
                                          https ? WINHTTP_FLAG_SECURE : 0);
            if(!hRequest) goto error;
        } else goto error;

        // optional key/values
        for(auto const& pair : pairs) {
            std::string urikey;
            urikey.resize(pair.first.size() * 3);
            auto usize = lxd::uri_encode(pair.first.data(), pair.first.size(), urikey.data());
            urikey.resize(usize);
            std::string urivalue;
            urivalue.resize(pair.second.size() * 3);
            usize = lxd::uri_decode(pair.second.data(), pair.second.size(), urivalue.data());
            urivalue.resize(usize);

            content += fmt::format("&{}={}", urikey, urivalue);
        }

        if(!WinHttpSendRequest(hRequest,
                               contentType,
                               static_cast<DWORD>(wcslen(contentType)),
                               content.data(),
                               static_cast<DWORD>(content.size()),
                               static_cast<DWORD>(content.size()),
                               NULL)) goto error;

        // End the request.
        if(!WinHttpReceiveResponse(hRequest, NULL)) goto error;

        // Keep checking for data until there is nothing left.
        do {
            // Check for available data.
            dwSize = 0;
            if(!WinHttpQueryDataAvailable(hRequest, &dwSize))
                lxd::print("Error {} in WinHttpQueryDataAvailable.\n",
                           GetLastError());

            // Allocate space for the buffer.
            auto pszOutBuffer = new char[dwSize + 1];
            if(!pszOutBuffer) {
                lxd::print("Out of memory\n");
                dwSize = 0;
            } else {
                // Read the data.
                ZeroMemory(pszOutBuffer, dwSize + 1);
                pszOutBuffer[dwSize] = '\0';
                if(!WinHttpReadData(hRequest, (LPVOID)pszOutBuffer,
                                    dwSize, &dwDownloaded)) {
                    lxd::print("Error {} in WinHttpReadData.\n", GetLastError());
                } else {
                    result += pszOutBuffer;
                }
                // Free the memory allocated to the buffer.
                delete[] pszOutBuffer;
            }
        } while(dwSize > 0);

    error:
	    if (GetLastError() != 0)
            lxd::print("Error {} has occurred.\n", GetLastError());
    }

    PostFormUrlencoded::~PostFormUrlencoded() {
        // Close any open handles.
        if(hRequest) WinHttpCloseHandle(hRequest);
        if(hConnect) WinHttpCloseHandle(hConnect);
        if(hSession) WinHttpCloseHandle(hSession);
    }

    PostJson::PostJson(const wchar_t* host, const wchar_t* path, bool https,
                       std::string& result, const std::vector<std::pair<std::string, std::string>>& pairs) {
        // Specify an HTTP server.
        hSession = WinHttpOpen(L"lxd with WinHTTP Sync /1.0",
                                         WINHTTP_ACCESS_TYPE_DEFAULT_PROXY,
                                         WINHTTP_NO_PROXY_NAME,
                                         WINHTTP_NO_PROXY_BYPASS,
                                         0);

        // Connect
        if(hSession)
            hConnect = WinHttpConnect(hSession, host,
                                      INTERNET_DEFAULT_HTTPS_PORT, 0);

        // Create an HTTP request handle.
        if(hConnect)
            hRequest = WinHttpOpenRequest(hConnect,
                                          L"POST",
                                          path,
                                          NULL, WINHTTP_NO_REFERER,
                                          WINHTTP_DEFAULT_ACCEPT_TYPES,
                                          https ? WINHTTP_FLAG_SECURE : 0);

        const wchar_t* contentType = L"Content-Type:application/json;";
        std::string json;
        for(auto const& pair : pairs) {
            json += fmt::format("\"{}\":\"{}\"", pair.first, pair.second);
            if(&pair != &pairs.back()) {
                json += ",";
            }
        }
        json = fmt::format("{{{}}}", json);
        if(!WinHttpSendRequest(hRequest,
                               contentType,
                               (DWORD)-1,
                               const_cast<char*>(json.c_str()),
                               static_cast<DWORD>(json.size()),
                               static_cast<DWORD>(json.size()),
                               NULL)) goto error;

        // End the request.
        if(!WinHttpReceiveResponse(hRequest, NULL)) goto error;

        // Keep checking for data until there is nothing left.
        result.clear();
        do {
            // Check for available data.
            dwSize = 0;
            if(!WinHttpQueryDataAvailable(hRequest, &dwSize))
                lxd::print("Error {} in WinHttpQueryDataAvailable.\n",
                           GetLastError());

            // Allocate space for the buffer.
            auto pszOutBuffer = new char[dwSize + 1];
            if(!pszOutBuffer) {
                lxd::print("Out of memory\n");
                dwSize = 0;
            } else {
                // Read the data.
                ZeroMemory(pszOutBuffer, dwSize + 1);
                if(!WinHttpReadData(hRequest, (LPVOID)pszOutBuffer,
                                    dwSize, &dwDownloaded)) {
                    lxd::print("Error {} in WinHttpReadData.\n", GetLastError());
                } else {
                    result += pszOutBuffer;
                }
                // Free the memory allocated to the buffer.
                delete[] pszOutBuffer;
            }
        } while(dwSize > 0);

    error:
	    if (GetLastError() != 0)
            lxd::print("Error {} has occurred.\n", GetLastError());
    }

    PostJson::~PostJson() {
        // Close any open handles.
        if(hRequest) WinHttpCloseHandle(hRequest);
        if(hConnect) WinHttpCloseHandle(hConnect);
        if(hSession) WinHttpCloseHandle(hSession);
    }

    GetJson::GetJson(const wchar_t* host, const wchar_t* path, bool https,
                       std::string& result, const std::vector<std::pair<std::string, std::string>>& pairs,
                       const char* authID, const char* authSecret) {
        // Specify an HTTP server.
        hSession = WinHttpOpen(L"lxd with WinHTTP Sync /1.0",
                                         WINHTTP_ACCESS_TYPE_DEFAULT_PROXY,
                                         WINHTTP_NO_PROXY_NAME,
                                         WINHTTP_NO_PROXY_BYPASS,
                                         0);

        // Connect
        if(hSession)
            hConnect = WinHttpConnect(hSession, host,
                                      INTERNET_DEFAULT_HTTPS_PORT, 0);

        // Create an HTTP request handle.
        if(hConnect)
            hRequest = WinHttpOpenRequest(hConnect,
                                          L"GET",
                                          path,
                                          NULL, WINHTTP_NO_REFERER,
                                          WINHTTP_DEFAULT_ACCEPT_TYPES,
                                          https ? WINHTTP_FLAG_SECURE : 0);

        std::wstring header(L"Content-Type:application/json;");
        if(authID && authSecret) {
            auto idAndSecret = fmt::format("{}:{}", authID, authSecret);
            auto ecSize = lxd::Base64_EncodeSizeInBytes(idAndSecret.size());
            std::string base64;
            base64.resize(ecSize);
            lxd::Base64_Encode(base64.data(), reinterpret_cast<const unsigned char*>(idAndSecret.data()), idAndSecret.size());
            header = fmt::format(L"Authorization: Basic {}\r\n{}", lxd::utf8_decode(base64), header);
        }
        std::string json;
        for(auto const& pair : pairs) {
            json += fmt::format("\"{}\":\"{}\"", pair.first, pair.second);
            if(&pair != &pairs.back()) {
                json += ",";
            }
        }
        json = fmt::format("{{{}}}", json);
        if(!WinHttpSendRequest(hRequest,
                               header.data(),
                               (DWORD)-1,
                               const_cast<char*>(json.c_str()),
                               static_cast<DWORD>(json.size()),
                               static_cast<DWORD>(json.size()),
                               NULL)) goto error;

        // End the request.
        if(!WinHttpReceiveResponse(hRequest, NULL)) goto error;

        // Keep checking for data until there is nothing left.
        result.clear();
        do {
            // Check for available data.
            dwSize = 0;
            if(!WinHttpQueryDataAvailable(hRequest, &dwSize))
                lxd::print("Error {} in WinHttpQueryDataAvailable.\n",
                           GetLastError());

            // Allocate space for the buffer.
            auto pszOutBuffer = new char[dwSize + 1];
            if(!pszOutBuffer) {
                lxd::print("Out of memory\n");
                dwSize = 0;
            } else {
                // Read the data.
                ZeroMemory(pszOutBuffer, dwSize + 1);
                if(!WinHttpReadData(hRequest, (LPVOID)pszOutBuffer,
                                    dwSize, &dwDownloaded)) {
                    lxd::print("Error {} in WinHttpReadData.\n", GetLastError());
                } else {
                    result += pszOutBuffer;
                }
                // Free the memory allocated to the buffer.
                delete[] pszOutBuffer;
            }
        } while(dwSize > 0);

    error:
	    if (GetLastError() != 0)
            lxd::print("Error {} has occurred.\n", GetLastError());
    }

    GetJson::~GetJson() {
        // Close any open handles.
        if(hRequest) WinHttpCloseHandle(hRequest);
        if(hConnect) WinHttpCloseHandle(hConnect);
        if(hSession) WinHttpCloseHandle(hSession);
    }

    PutFile::PutFile(const wchar_t* host, const wchar_t* path, bool /*https*/, std::string& result, std::string_view data) {
        DWORD dwBytesWritten = 0;

        // Use WinHttpOpen to obtain a session handle.
        hSession = WinHttpOpen(L"A WinHTTP Example Program/1.0",
            WINHTTP_ACCESS_TYPE_DEFAULT_PROXY,
            WINHTTP_NO_PROXY_NAME,
            WINHTTP_NO_PROXY_BYPASS, 0);
        if(!hSession)
            goto error;

        hConnect = WinHttpConnect(hSession, host, INTERNET_DEFAULT_HTTP_PORT, 0);
        if(!hConnect)
            goto error;

        hRequest = WinHttpOpenRequest(hConnect, L"PUT",
            path,
            NULL, WINHTTP_NO_REFERER,
            WINHTTP_DEFAULT_ACCEPT_TYPES,
            0);
        if(!hRequest)
            goto error;

        if(!WinHttpSendRequest(hRequest, WINHTTP_NO_ADDITIONAL_HEADERS, 0, WINHTTP_NO_REQUEST_DATA, 0, (DWORD)data.size(), 0))
            goto error;

        if(!WinHttpWriteData(hRequest, data.data(), (DWORD)data.size(), &dwBytesWritten))
            goto error;

        if(!WinHttpReceiveResponse(hRequest, NULL))
            goto error;

        do {
            // Check for available data.
            dwSize = 0;
            if(!WinHttpQueryDataAvailable(hRequest, &dwSize))
                goto error;

            // Allocate space for the buffer.
            auto pszOutBuffer = new char[dwSize + 1];
            if(!pszOutBuffer) {
                lxd::print("Out of memory\n");
                dwSize = 0;
            } else {
                // Read the data.
                ZeroMemory(pszOutBuffer, dwSize + 1);
                if(!WinHttpReadData(hRequest, (LPVOID)pszOutBuffer,
                    dwSize, &dwDownloaded)) {
                    lxd::print("Error {} in WinHttpReadData.\n", GetLastError());
                } else {
                    result += pszOutBuffer;
                }
                // Free the memory allocated to the buffer.
                delete[] pszOutBuffer;
            }
        } while(dwSize > 0);

        // Report any errors.
    error:
	    if (GetLastError() != 0)
            lxd::print("Error {} has occurred.\n", GetLastError());
    }
    PutFile::~PutFile() {
        // Close any open handles.
        if(hRequest) WinHttpCloseHandle(hRequest);
        if(hConnect) WinHttpCloseHandle(hConnect);
        if(hSession) WinHttpCloseHandle(hSession);
    }

    struct HttpHandleCloser { void operator()(HANDLE h) noexcept { assert(h != INVALID_HANDLE_VALUE); if (h) WinHttpCloseHandle(h); } };
    using ScopedHttpHandle = std::unique_ptr<std::remove_pointer<HANDLE>::type, HttpHandleCloser>;

    static std::string HttpRequest(
        const wchar_t* host,
        const wchar_t* path,
        const wchar_t* verb,
        bool https,
        const std::vector<std::pair<std::wstring, std::wstring>>& headers,
        std::string_view body
    ) {
        // Specify an HTTP server.
        ScopedHttpHandle hSession(WinHttpOpen(
            L"lxd with WinHTTP Sync /1.0",
            WINHTTP_ACCESS_TYPE_DEFAULT_PROXY,
            WINHTTP_NO_PROXY_NAME,
            WINHTTP_NO_PROXY_BYPASS,
            0
        ));
        if (!hSession) {
            return {};
        }

        // Connect
        ScopedHttpHandle hConnect(WinHttpConnect(hSession.get(), host, INTERNET_DEFAULT_HTTPS_PORT, 0));
        if (!hConnect) {
            return {};
        }

        ScopedHttpHandle hRequest(WinHttpOpenRequest(
            hConnect.get(),
            verb,
            path,
            NULL, WINHTTP_NO_REFERER,
            WINHTTP_DEFAULT_ACCEPT_TYPES,
            https ? WINHTTP_FLAG_SECURE : 0
        ));
        if (!hRequest) {
            return {};
        }

        std::wstring headersContent;
        for (const auto& header : headers) {
            headersContent.append(header.first);
            headersContent.push_back(L':');
            headersContent.append(header.second);
            headersContent.append(L"\r\n");
        }
        if (!headers.empty()) {
            headersContent.pop_back();
            headersContent.pop_back();
        }

        if (!WinHttpSendRequest(
            hRequest.get(),
            headersContent.c_str(),
            (DWORD)headersContent.size(),
            (void*)body.data(),
            (DWORD)body.size(),
            (DWORD)body.size(),
            NULL
            )) {
            return {};
        }

        if (!WinHttpReceiveResponse(hRequest.get(), NULL)) {
            return {};
        }

        std::string result;
        while (true) {
            // Check for available data.
            DWORD dwSize = 0;
            if (!WinHttpQueryDataAvailable(hRequest.get(), &dwSize)) {
                lxd::print("Error {} in WinHttpQueryDataAvailable.\n", GetLastError());
            }

            if (dwSize == 0) {
                break;
            }

            size_t originSize = result.size();
            result.resize(result.size() + dwSize);
            unsigned long dwDownloaded = 0;
            WinHttpReadData(hRequest.get(), result.data() + originSize, dwSize, &dwDownloaded);
            if (dwDownloaded != dwSize) {
                result.resize(originSize + dwDownloaded);
            }
        }

        return result;
    }

    std::string Post(
        const wchar_t* host,
        const wchar_t* path,
        bool https,
        const std::vector<std::pair<std::wstring, std::wstring>>& headers,
        std::string_view body
    ) {
        return HttpRequest(host, path, L"POST", https, headers, body);
    }

    std::string DLL_PUBLIC Get(
        const wchar_t* host,
        const wchar_t* path,
        bool https,
        const std::vector<std::pair<std::wstring, std::wstring>>& headers
    ) {
        return HttpRequest(host, path, L"GET", https, headers, {});
    }
}
