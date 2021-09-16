#include "http.h"
#include <Windows.h>
#include <winhttp.h>
#include "debug.h"
#include "encoding.h"
#include "base64.h"
#include <fmt/xchar.h>
#include <cassert>

namespace lxd {
	HttpRequestSync::HttpRequestSync(const wchar_t* host, unsigned short port, const wchar_t* path, std::string& result, const std::string& post) {
        // Specify an HTTP server.
        HINTERNET hSession = WinHttpOpen(L"lxd with WinHTTP Sync /1.0",
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
        if(hRequest)
            bResults = WinHttpSendRequest(hRequest,
                                          WINHTTP_NO_ADDITIONAL_HEADERS,
                                          0,
                                          post.empty() ? WINHTTP_NO_REQUEST_DATA : const_cast<char*>(post.c_str()),
                                          post.empty() ? 0 : static_cast<DWORD>(post.size()),
                                          post.empty() ? 0 : static_cast<DWORD>(post.size()),
                                          NULL);

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
                               std::string& result, const std::vector<std::pair<std::string, std::string>>& pairs,
                               const std::vector<std::pair<std::string, std::string>>& files,
                               const char* authID, const char* authSecret) {
        // Fix `skipped by goto` error
        std::vector<std::pair<std::string, std::string>> additionals;
        std::wstring header(L"Content-Type:multipart/form-data; boundary=1SUB64X86GK5");
        std::vector<std::string> optionals;
        // Specify an HTTP server.
        HINTERNET hSession = WinHttpOpen(L"lxd with WinHTTP Sync /1.0",
                                         WINHTTP_ACCESS_TYPE_DEFAULT_PROXY,
                                         WINHTTP_NO_PROXY_NAME,
                                         WINHTTP_NO_PROXY_BYPASS,
                                         0);

        // Connect
        if(hSession) {
            hConnect = WinHttpConnect(hSession, host,
                                      INTERNET_DEFAULT_HTTPS_PORT, 0);
        } else goto error;
            

        // Create an HTTP request handle.
        if(hConnect) {
            hRequest = WinHttpOpenRequest(hConnect,
                                          L"POST",
                                          path,
                                          NULL, WINHTTP_NO_REFERER,
                                          WINHTTP_DEFAULT_ACCEPT_TYPES,
                                          https ? WINHTTP_FLAG_SECURE : 0);
        } else goto error;

        if(authID && authSecret) {
            auto idAndSecret = fmt::format("{}:{}", authID, authSecret);
            auto ecSize = lxd::Base64_EncodeSizeInBytes(idAndSecret.size());
            std::string base64;
            base64.resize(ecSize);
            lxd::Base64_Encode(base64.data(), reinterpret_cast<const unsigned char*>(idAndSecret.data()), idAndSecret.size());
            header = fmt::format(L"Authorization: Basic {}\r\n{}", lxd::utf8_decode(base64), header);
        }
        const char* boundary = "--1SUB64X86GK5\r\n";
        const char* disposition = "Content-Disposition: form-data; ";
        const char* newLine = "\r\n";
        const char* finalBody = "--1SUB64X86GK5--\r\n";
        size_t totalSize{};
        // optional key/values
        for(auto const& pair : pairs) {
            auto str = fmt::format("{}{}name=\"{}\"\r\n\r\n{}", boundary, disposition, pair.first, pair.second);
            lxd::print("{}", str);
            totalSize += str.size();
            optionals.emplace_back(std::move(str));
        }
        //optional = fmt::format("{}{}", optional, boundary);
        // additional
        for(auto const& file : files) {
            auto str = fmt::format("{}{}name=\"StlBinary\"; filename=\"{}\"\r\nContent-Type: application/zip\r\n\r\n", boundary, disposition, file.first);
            lxd::print("{}", str);
            totalSize += str.size();
            totalSize += file.second.size();
            additionals.emplace_back(std::make_pair(std::move(str), file.second));
        }
        totalSize += strlen(newLine) * optionals.size();
        totalSize += strlen(newLine) * additionals.size();
        totalSize += strlen(finalBody);

        // Send a request.
        if(!WinHttpAddRequestHeaders(
            hRequest,
            header.c_str(),
            static_cast<DWORD>(header.size()),
            WINHTTP_ADDREQ_FLAG_ADD
        )) goto error;

        if(!WinHttpSendRequest(hRequest,
                               WINHTTP_NO_ADDITIONAL_HEADERS,
                               0,
                               WINHTTP_NO_REQUEST_DATA,
                               0,
                               static_cast<DWORD>(totalSize),
                               NULL)) goto error;
        DWORD dwBytesWritten = 0;
        size_t checkSize{};
        for(auto const& optional : optionals) {
            if(!WinHttpWriteData(hRequest, optional.c_str(),
                                 static_cast<DWORD>(optional.size()),
                                 &dwBytesWritten)) goto error;
            checkSize += optional.size();
            if(!WinHttpWriteData(hRequest, newLine,
                                 static_cast<DWORD>(strlen(newLine)),
                                 &dwBytesWritten)) goto error;
            checkSize += strlen(newLine);
        }
        for(auto const& additional : additionals) {
            if(!WinHttpWriteData(hRequest, additional.first.c_str(),
                                 static_cast<DWORD>(additional.first.size()),
                                 &dwBytesWritten)) goto error;
            checkSize += additional.first.size();
            if(!WinHttpWriteData(hRequest, additional.second.c_str(),
                                 static_cast<DWORD>(additional.second.size()),
                                 &dwBytesWritten)) goto error;
            checkSize += additional.second.size();
            if(!WinHttpWriteData(hRequest, newLine,
                                 static_cast<DWORD>(strlen(newLine)),
                                 &dwBytesWritten)) goto error;
            checkSize += strlen(newLine);
        }
        if(!WinHttpWriteData(hRequest, finalBody,
                             static_cast<DWORD>(strlen(finalBody)),
                             &dwBytesWritten)) goto error;
        checkSize += strlen(finalBody);
        assert(totalSize == checkSize);

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
        lxd::print("Error {} has occurred.\n", GetLastError());
    }

    PostFormdata::~PostFormdata() {
        // Close any open handles.
        if(hRequest) WinHttpCloseHandle(hRequest);
        if(hConnect) WinHttpCloseHandle(hConnect);
        if(hSession) WinHttpCloseHandle(hSession);
    }

    PostFormUrlencoded::PostFormUrlencoded(const wchar_t* host, unsigned short port, const wchar_t* path, bool https,
                               std::string& result, const std::vector<std::pair<std::string, std::string>>& pairs) {
        std::string content;
        // Specify an HTTP server.
        HINTERNET hSession = WinHttpOpen(L"lxd with WinHTTP Sync /1.0",
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

        const wchar_t* contentType = L"Content-Type: application/x-www-form-urlencoded";
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
        HINTERNET hSession = WinHttpOpen(L"lxd with WinHTTP Sync /1.0",
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
                               -1,
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
        HINTERNET hSession = WinHttpOpen(L"lxd with WinHTTP Sync /1.0",
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
                               -1,
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
        lxd::print("Error {} has occurred.\n", GetLastError());
    }

    GetJson::~GetJson() {
        // Close any open handles.
        if(hRequest) WinHttpCloseHandle(hRequest);
        if(hConnect) WinHttpCloseHandle(hConnect);
        if(hSession) WinHttpCloseHandle(hSession);
    }
}