#pragma once
#if defined(__clang__)
#pragma clang diagnostic ignored "-Wdelete-abstract-non-virtual-dtor"
#endif

#include "defines.h"
#include "WinHTTPWrappers.h"
#include <string_view>
#include <functional>
#include <mutex>
#include <cassert>
#include <map>


namespace lxd {

class MyRequest : public WinHTTPWrappers::CAsyncDownloader {
public:
	MyRequest() = default;

	// 不支持复制和移动
	MyRequest(const MyRequest&) = delete;
	MyRequest(MyRequest&&) = delete;

	HRESULT Initialize(
		const WinHTTPWrappers::CConnection& connection,
		LPCWSTR pwszObjectName,
		LPCWSTR pwszVerb = nullptr,
		DWORD httpAuthScheme = 0,
		const std::pair<std::wstring_view, std::wstring_view>& cred = {}
	);

	virtual ~MyRequest() {
		MyClose();
	}

	virtual void MyClose();

	HRESULT OnCallbackComplete(
		_In_ HRESULT hr,
		_In_ HINTERNET hInternet,
		_In_ DWORD dwInternetStatus,
		_In_opt_ LPVOID lpvStatusInformation,
		_In_ DWORD dwStatusInformationLength
	) override;

	// 请求发出后必须有结果此类才能析构
	HRESULT SendRequest(_In_reads_bytes_opt_(dwOptionalLength) LPVOID lpOptional = WINHTTP_NO_REQUEST_DATA, _In_ DWORD dwOptionalLength = 0);

	HRESULT OnCallback(
		_In_ HINTERNET hInternet,
		_In_ DWORD dwInternetStatus,
		_In_opt_ LPVOID lpvStatusInformation,
		_In_ DWORD dwStatusInformationLength
	) override;

	DWORD GetLastStatusCode();

	void Wait();

	bool IsRequesting();

protected:
	void SetCanSafeExit();

	void SetCannotSafeExit();

	static constexpr int BUFFER_SIZE = 12288;	// 适当增大缓冲区以减少 OnProgress 的调用次数

private:
	std::condition_variable m_cv;
	std::mutex m_mt;
	// 为 true 时析构函数可以无阻执行
	bool m_fCanSafeExit = true;

	bool _isClosing = false;
	bool _isRequesting = false;
};


class GetRequest : public MyRequest {
public:
	~GetRequest() {
		MyClose();
	}

	HRESULT Initialize(
		const WinHTTPWrappers::CConnection& connection,
		LPCWSTR pwszObjectName,
		std::function<void(HRESULT, DWORD, std::string_view)> onComplete,
		std::function<void(float progress)> onProgress,
		const std::map<std::wstring, std::wstring> headers = {},
		DWORD httpAuthScheme = 0,
		const std::pair<std::wstring_view, std::wstring_view>& cred = {}
	);

	HRESULT OnCallbackComplete(
		_In_ HRESULT hr,
		_In_ HINTERNET hInternet,
		_In_ DWORD dwInternetStatus,
		_In_opt_ LPVOID lpvStatusInformation,
		_In_ DWORD dwStatusInformationLength
	) override;

	HRESULT OnReadData(_In_reads_bytes_(dwBytesRead) const void* lpvBuffer, _In_ DWORD dwBytesRead) override;

	std::wstring GetHeaders() override;
private:
	std::function<void(HRESULT, DWORD, std::string_view)> _onComplete;
	std::function<void(float progress)> _onProgress;
	std::map<std::wstring, std::wstring> _headers;
};

class RequestTask {
public:
	//virtual ~RequestTask() {};

	virtual void Wait() = 0;

	virtual void Cancel() = 0;

	virtual bool IsRequesting() = 0;
};

class GetRequestTask : public RequestTask {
public:
	// 只能移动不能复制
	GetRequestTask(const GetRequestTask&) = delete;
	GetRequestTask(GetRequestTask&&) = default;

	GetRequestTask& operator=(const GetRequestTask&) = delete;
	GetRequestTask& operator=(GetRequestTask&&) = default;

	GetRequestTask() = default;

	GetRequestTask(
		const WinHTTPWrappers::CSession& session,
		LPCWSTR pwszServerName,
		LPCWSTR pwszObjectName,
		std::function<void(HRESULT, DWORD, std::string_view)> onComplete,
		std::function<void(float progress)> onProgress = {},
		const std::map<std::wstring, std::wstring>& headers = {},
		std::wstring_view fileToSave = L"",
		DWORD httpAuthScheme = 0,
		const std::pair<std::wstring_view, std::wstring_view>& cred = {}
	);

	//virtual ~GetRequestTask() {}

	void Wait() override;

	void Cancel() override;

	bool IsRequesting() override;
private:
	std::unique_ptr<WinHTTPWrappers::CConnection> _conn;
	std::unique_ptr<GetRequest> _getReq;
};

class PostRequest : public MyRequest {
public:
	~PostRequest() {
		MyClose();
	}

	void MyClose() override;

	HRESULT Initialize(
		const WinHTTPWrappers::CConnection& connection,
		LPCWSTR pwszObjectName,
		std::function<void(HRESULT, DWORD, std::string_view)> onComplete,
		std::function<void(float progress)> onProgress,
		const std::map<std::wstring, std::wstring>& headers = {},
		const std::vector<std::pair<std::string_view, std::string_view>>& formData = {},
		const std::vector<std::tuple<std::string_view, std::string_view, std::string_view>>& files = {},
		DWORD httpAuthScheme = 0,
		const std::pair<std::wstring_view, std::wstring_view>& cred = {}
	);

	HRESULT Initialize(
		const WinHTTPWrappers::CConnection& connection,
		LPCWSTR pwszObjectName,
		std::function<void(HRESULT, DWORD, std::string_view)> onComplete,
		std::function<void(float progress)> onProgress,
		const std::map<std::wstring, std::wstring>& headers = {},
		std::string_view body = "",
		DWORD httpAuthScheme = 0,
		const std::pair<std::wstring_view, std::wstring_view>& cred = {}
	);

	HRESULT OnCallbackComplete(
		_In_ HRESULT hr,
		_In_ HINTERNET hInternet,
		_In_ DWORD dwInternetStatus,
		_In_opt_ LPVOID lpvStatusInformation,
		_In_ DWORD dwStatusInformationLength
	) override;

	HRESULT OnWriteData() override;

	std::wstring GetHeaders() override;

	std::string MergeFormData(
		const std::vector<std::pair<std::string_view, std::string_view>>& pairs,
		const std::vector<std::tuple<std::string_view, std::string_view, std::string_view>>& files
	) const;

private:
	std::function<void(HRESULT, DWORD, std::string_view)> _onComplete;
	std::function<void(float)> _onProgress;

	const wchar_t* _formDataBoundaryW = L"1SUB64X86GK5";
	const char* _formDataBoundary = "1SUB64X86GK5";

	std::string _body;
	std::map<std::wstring, std::wstring> _headers;
	bool _isWriteDataCompleted = false;
	unsigned int _bufferPos = 0;
};

class PostRequestTask : public RequestTask {
public:
	// 只能移动不能复制
	PostRequestTask(const PostRequestTask&) = delete;
	PostRequestTask(PostRequestTask&&) = default;

	PostRequestTask& operator=(const PostRequestTask&) = delete;
	PostRequestTask& operator=(PostRequestTask&&) = default;

	PostRequestTask() = default;

	PostRequestTask(
		const WinHTTPWrappers::CSession& session,
		LPCWSTR pwszServerName,
		LPCWSTR pwszObjectName,
		std::function<void(HRESULT, DWORD, std::string_view)> onComplete,
		std::function<void(float progress)> onProgress = {},
		const std::map<std::wstring, std::wstring>& headers = {},
		const std::vector<std::pair<std::string_view, std::string_view>>& formData = {},
		const std::vector<std::tuple<std::string_view, std::string_view, std::string_view>>& files = {},
		DWORD httpAuthScheme = 0,
		const std::pair<std::wstring_view, std::wstring_view>& cred = {}
	);

	PostRequestTask(
		const WinHTTPWrappers::CSession& session,
		LPCWSTR pwszServerName,
		LPCWSTR pwszObjectName,
		std::function<void(HRESULT, DWORD, std::string_view)> onComplete,
		std::function<void(float progress)> onProgress = {},
		const std::map<std::wstring, std::wstring>& headers = {},
		std::string_view body = "",
		DWORD httpAuthScheme = 0,
		const std::pair<std::wstring_view, std::wstring_view>& cred = {}
	);

	//~PostRequestTask() {}

	void Wait() override;

	void Cancel() override;

	bool IsRequesting() override;
private:
	std::unique_ptr<WinHTTPWrappers::CConnection> _conn;
	std::unique_ptr<PostRequest> _postReq;
};

class DLL_PUBLIC HttpClient {
public:
	HttpClient();

	~HttpClient();

	unsigned int GetAsync(
		std::wstring_view url,
		std::function<void(HRESULT hr, DWORD lastStatusCode, std::string_view response)> onComplete,
		std::function<void(float progress)> onProgress = {},
		std::wstring_view fileToSave = L"",
		DWORD httpAuthScheme = 0,
		const std::pair<std::wstring_view, std::wstring_view>& cred = {},
		const std::map<std::wstring, std::wstring>& headers = {}
	);

	unsigned int PostFormDataAsync(
		std::wstring_view url,
		std::function<void(HRESULT hr, DWORD lastStatusCode, std::string_view response)> onComplete,
		std::function<void(float progress)> onProgress = {},
		const std::vector<std::pair<std::string_view, std::string_view>>& pairs = {},
		const std::vector<std::tuple<std::string_view, std::string_view, std::string_view>>& files = {},
		DWORD httpAuthScheme = 0,
		const std::pair<std::wstring_view, std::wstring_view>& cred = {},
		const std::map<std::wstring, std::wstring>& headers = {}
	);

	unsigned int PostAsync(
		std::wstring_view url,
		std::function<void(HRESULT hr, DWORD lastStatusCode, std::string_view response)> onComplete,
		std::function<void(float progress)> onProgress = {},
		std::string_view body = "",
		DWORD httpAuthScheme = 0,
		const std::pair<std::wstring_view, std::wstring_view>& cred = {},
		const std::map<std::wstring, std::wstring>& headers = {}
	);


	bool Wait(unsigned int requestId);

	void WaitAll();

	void CancelAll();

	bool Cancel(unsigned int requestId);

	bool IsRequesting(unsigned int requestId);

private:
	unsigned int _NewTaskId() {
		return _nextId++;
	}

	WinHTTPWrappers::CSession _session;

	std::unordered_map<unsigned int, std::unique_ptr<RequestTask>> _tasks;
	std::recursive_mutex _mutex;

	std::atomic<unsigned int> _nextId = 0;
};

}
