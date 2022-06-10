#include "AsyncHttp.h"
#include <Windows.h>
#include <atlstr.h>
#include <memory>
#include <fmt/format.h>
#include <fmt/xchar.h>

namespace lxd {

HRESULT MyRequest::Initialize(
	const WinHTTPWrappers::CConnection& connection,
	LPCWSTR pwszObjectName,
	LPCWSTR pwszVerb,
	DWORD httpAuthScheme,
	const std::pair<std::wstring_view, std::wstring_view>& cred
) {
	if (httpAuthScheme) {
		m_bHTTPPreauthentication = true;
		m_dwHTTPPreauthenticationScheme = httpAuthScheme;
		m_sHTTPUserName = cred.first;
		m_sHTTPPassword = cred.second;
	}

	return WinHTTPWrappers::CAsyncDownloader::Initialize(connection, pwszObjectName, pwszVerb, nullptr, 0, 0, WINHTTP_FLAG_SECURE, BUFFER_SIZE);
}

// 关闭时必须调用此函数而不是 CAsyncDownloader::Close
void MyRequest::MyClose() {
	ATL::CCritSecLock sl(m_cs.m_sec, true);
	_isClosing = true;
	sl.Unlock();

	CAsyncDownloader::Close();

	std::unique_lock<std::mutex> lk(m_mt);
	m_cv.wait(lk, [&] { return m_fCanSafeExit; });
}

HRESULT MyRequest::OnCallbackComplete(
	_In_ HRESULT hr,
	_In_ HINTERNET hInternet,
	_In_ DWORD dwInternetStatus,
	_In_opt_ LPVOID lpvStatusInformation,
	_In_ DWORD dwStatusInformationLength
) {
	hr = WinHTTPWrappers::CAsyncDownloader::OnCallbackComplete(hr, hInternet, dwInternetStatus, lpvStatusInformation, dwStatusInformationLength);
	_isRequesting = false;
	SetCanSafeExit();

	return hr;
}

HRESULT MyRequest::SendRequest(_In_reads_bytes_opt_(dwOptionalLength) LPVOID lpOptional, _In_ DWORD dwOptionalLength) {
	HRESULT hr = WinHTTPWrappers::CAsyncDownloader::SendRequest(lpOptional, dwOptionalLength);

	if (SUCCEEDED(hr)) {
		_isRequesting = true;
		SetCannotSafeExit();
	}

	return hr;
}

HRESULT MyRequest::OnCallback(
	_In_ HINTERNET hInternet,
	_In_ DWORD dwInternetStatus,
	_In_opt_ LPVOID lpvStatusInformation,
	_In_ DWORD dwStatusInformationLength
) {
	HRESULT hr;

	ATL::CCritSecLock sl(m_cs.m_sec, true);

	if (!_isRequesting) {
		// 请求完成后取消不是错误
		return S_OK;
	}

	if (_isClosing) {
		// 已调用 Close，此时句柄已失效。在收到 HANDLE_CLOSING 之前不处理任何消息
		if (dwInternetStatus != WINHTTP_CALLBACK_STATUS_HANDLE_CLOSING) {
			return S_OK;
		}

		hr = OnRequestErrorCallback(hInternet, dwInternetStatus, lpvStatusInformation, dwStatusInformationLength);
		SetStatusCallback(nullptr, WINHTTP_CALLBACK_FLAG_ALL_NOTIFICATIONS);
		_isClosing = false;
	} else {
		hr = WinHTTPWrappers::CAsyncDownloader::OnCallback(hInternet, dwInternetStatus, lpvStatusInformation, dwStatusInformationLength);
	}

	return hr;
}

DWORD MyRequest::GetLastStatusCode() {
	bool valid;
	DWORD c = WinHTTPWrappers::CAsyncDownloader::GetLastStatusCode(valid);
	return valid ? c : 0;
}

void MyRequest::Wait() {
	std::unique_lock<std::mutex> lk(m_mt);
	m_cv.wait(lk, [&] { return m_fCanSafeExit; });
}

bool MyRequest::IsRequesting() {
	ATL::CCritSecLock sl(m_cs.m_sec, true);
	return _isRequesting;
}

void MyRequest::SetCanSafeExit() {
	m_mt.lock();
	m_fCanSafeExit = true;
	m_mt.unlock();
	m_cv.notify_all();
}

void MyRequest::SetCannotSafeExit() {
	m_mt.lock();
	m_fCanSafeExit = false;
	m_mt.unlock();
}


HRESULT GetRequest::Initialize(
	const WinHTTPWrappers::CConnection& connection,
	LPCWSTR pwszObjectName,
	std::function<void(HRESULT, DWORD, std::string_view)> onComplete,
	std::function<void(float)> onProgress,
	DWORD httpAuthScheme,
	const std::pair<std::wstring_view, std::wstring_view>& cred
) {
	HRESULT hr = MyRequest::Initialize(connection, pwszObjectName, L"GET", httpAuthScheme, cred);
	if (FAILED(hr)) {
		return hr;
	}

	_onComplete = onComplete;
	_onProgress = onProgress;

	return hr;
}

HRESULT GetRequest::OnCallbackComplete(
	_In_ HRESULT hr,
	_In_ HINTERNET hInternet,
	_In_ DWORD dwInternetStatus,
	_In_opt_ LPVOID lpvStatusInformation,
	_In_ DWORD dwStatusInformationLength
) {
	if (_onComplete) {
		std::string response;
		if (SUCCEEDED(hr)) {
			if (!m_sFileToDownloadInto.empty()) {
				response = "file";
			} else {
				response = { m_Response.begin(), m_Response.end() };
			};
		}

		_onComplete(hr, GetLastStatusCode(), response);
	}

	return MyRequest::OnCallbackComplete(hr, hInternet, dwInternetStatus, lpvStatusInformation, dwStatusInformationLength);
}

HRESULT GetRequest::OnReadData(_In_reads_bytes_(dwBytesRead) const void* lpvBuffer, _In_ DWORD dwBytesRead) {
	HRESULT hr = MyRequest::OnReadData(lpvBuffer, dwBytesRead);

	ATL::CCritSecLock sl(m_cs.m_sec, true);
	if (_onProgress) {
		if (m_nContentLength <= 0) {
			_onProgress(0);
		} else {
			_onProgress(float(m_nTotalBytesRead) / m_nContentLength);
		}
	}

	return hr;
}

void PostRequest::MyClose() {
	MyRequest::MyClose();

	ATL::CCritSecLock sl(m_cs.m_sec, true);
	_isWriteDataCompleted = false;
	_bufferPos = 0;
}

HRESULT PostRequest::Initialize(
	const WinHTTPWrappers::CConnection& connection,
	LPCWSTR pwszObjectName,
	std::function<void(HRESULT, DWORD, std::string_view)> onComplete,
	std::function<void(float)> onProgress,
	const std::map<std::wstring, std::wstring>& headers,
	const std::vector<std::pair<std::string_view, std::string_view>>& formData,
	const std::vector<std::tuple<std::string_view, std::string_view, std::string_view>>& files,
	DWORD httpAuthScheme,
	const std::pair<std::wstring_view, std::wstring_view>& cred
) {
	_headers[L"Content-Type"] = fmt::format(L"multipart/form-data;boundary=\"{}\"", _formDataBoundaryW);
	return Initialize(connection, pwszObjectName, onComplete, onProgress, headers, MergeFormData(formData, files), httpAuthScheme, cred);
}

HRESULT PostRequest::Initialize(
	const WinHTTPWrappers::CConnection& connection,
	LPCWSTR pwszObjectName,
	std::function<void(HRESULT, DWORD, std::string_view)> onComplete,
	std::function<void(float progress)> onProgress,
	const std::map<std::wstring, std::wstring>& headers,
	std::string_view body,
	DWORD httpAuthScheme,
	const std::pair<std::wstring_view, std::wstring_view>& cred
) {
	_headers.insert(headers.begin(), headers.end());

	_body = body;
	m_lpRequest = _body.c_str();
	m_dwRequestSize = (DWORD)_body.size();

	HRESULT hr = MyRequest::Initialize(connection, pwszObjectName, L"POST", httpAuthScheme, cred);
	if (FAILED(hr)) {
		return hr;
	}

	_onComplete = onComplete;
	_onProgress = onProgress;

	return hr;
}

HRESULT PostRequest::OnCallbackComplete(
	_In_ HRESULT hr,
	_In_ HINTERNET hInternet,
	_In_ DWORD dwInternetStatus,
	_In_opt_ LPVOID lpvStatusInformation,
	_In_ DWORD dwStatusInformationLength
) {
	if (_onComplete) {
		std::string response;
		if (SUCCEEDED(hr)) {
			response = std::string(m_Response.begin(), m_Response.end());
		}

		_onComplete(hr, GetLastStatusCode(), response);
	}

	return MyRequest::OnCallbackComplete(hr, hInternet, dwInternetStatus, lpvStatusInformation, dwStatusInformationLength);
}

HRESULT PostRequest::OnWriteData() {
	ATL::CCritSecLock sl(m_cs.m_sec, true);

	if (_isWriteDataCompleted) {
		return S_OK;
	}

	HRESULT hr;
	// 从缓冲区读取body数据
	if (m_dwRequestSize) {
		ATLASSERT(m_lpRequest != nullptr); //m_pbyRequest should be provided if m_dwRequestSize is non-zero

		// 为了显示进度条分段传输
		unsigned int nBytesWrite = _bufferPos + BUFFER_SIZE <= m_dwRequestSize ? BUFFER_SIZE : m_dwRequestSize - _bufferPos;
		hr = WriteData((BYTE*)m_lpRequest + _bufferPos, nBytesWrite, nullptr);
		if (FAILED(hr))
			return hr;

		_bufferPos += nBytesWrite;

		if (_onProgress) {
			_onProgress(float(_bufferPos) / m_dwRequestSize);
		}

		hr = _bufferPos >= m_dwRequestSize ? S_FALSE : S_OK;
	} else {
		//There's nothing more to upload so return S_FALSE
		hr = S_FALSE;
	}

	if (hr == S_FALSE) {
		// 所有数据已写入
		_isWriteDataCompleted = true;
	}

	return hr;
}

std::wstring PostRequest::GetHeaders() {
	std::wstring headers = MyRequest::GetHeaders();
	
	for (const auto& [name, value] : _headers) {
		headers.append(fmt::format(L"{}: {}\r\n", name, value));
	}

	return headers;
}

std::string PostRequest::MergeFormData(
	const std::vector<std::pair<std::string_view, std::string_view>>& pairs,
	const std::vector<std::tuple<std::string_view, std::string_view, std::string_view>>& files
) const {
	std::string result;

	for (const auto& pair : pairs) {
		result += fmt::format("--{}\r\nContent-Disposition: form-data; name=\"{}\"\r\n\r\n{}\r\n", _formDataBoundary, pair.first, pair.second);
	}

	for (const auto& file : files) {
		result += fmt::format(
			"--{}\r\nContent-Disposition: form-data; name=\"{}\"; filename=\"{}\"\r\n\r\n{}\r\n",
			_formDataBoundary, std::get<0>(file), std::get<1>(file), std::get<2>(file)
		);
	}

	result += fmt::format("--{}--\r\n", _formDataBoundary);
	return result;
}

HttpClient::HttpClient() {
	HRESULT hr = _session.Initialize(L"WinHttpWrap", WINHTTP_ACCESS_TYPE_DEFAULT_PROXY, WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_BYPASS);
	assert(SUCCEEDED(hr));
}

HttpClient::~HttpClient() {
	CancelAll();
}


unsigned int HttpClient::GetAsync(
	std::wstring_view url,
	std::function<void(HRESULT, DWORD, std::string_view)> onComplete,
	std::function<void(float progress) > onProgress,
	std::wstring_view fileToSave,
	DWORD httpAuthScheme,
	const std::pair<std::wstring_view, std::wstring_view>& cred
) {
	std::scoped_lock lk(_mutex);

	URL_COMPONENTS urlComp{};
	urlComp.dwStructSize = sizeof(urlComp);
	urlComp.dwSchemeLength = (DWORD)-1;
	urlComp.dwHostNameLength = (DWORD)-1;
	urlComp.dwUrlPathLength = (DWORD)-1;
	urlComp.dwExtraInfoLength = (DWORD)-1;

	BOOL rs = WinHttpCrackUrl(url.data(), (DWORD)url.size(), 0, &urlComp);
	if (rs != TRUE) {
		if (onComplete) {
			onComplete(ATL::AtlHresultFromLastError(), 0, "");
		}
		return {};
	}

	unsigned int id = _NewTaskId();
	
	auto completeCb = [this, onComplete, id](HRESULT hr, DWORD lastStatusCode, std::string_view response) {
		if (onComplete) {
			onComplete(hr, lastStatusCode, response);
		}

		std::scoped_lock lk(_mutex);
		_tasks.erase(id);
	};

	_tasks.emplace(id, new GetRequestTask(
		_session,
		CStringW(urlComp.lpszHostName, urlComp.dwHostNameLength),
		CStringW(urlComp.lpszUrlPath, urlComp.dwUrlPathLength),
		completeCb,
		onProgress,
		fileToSave,
		httpAuthScheme,
		cred
	));

	return id;
}

unsigned int HttpClient::PostFormDataAsync(
	std::wstring_view url,
	std::function<void(HRESULT, DWORD, std::string_view)> onComplete,
	std::function<void(float progress)> onProgress,
	const std::vector<std::pair<std::string_view, std::string_view>>& pairs,
	const std::vector<std::tuple<std::string_view, std::string_view, std::string_view>>& files,
	DWORD httpAuthScheme,
	const std::pair<std::wstring_view, std::wstring_view>& cred,
	const std::map<std::wstring, std::wstring>& headers
) {
	std::scoped_lock lk(_mutex);

	URL_COMPONENTS urlComp{};
	urlComp.dwStructSize = sizeof(urlComp);
	urlComp.dwSchemeLength = (DWORD)-1;
	urlComp.dwHostNameLength = (DWORD)-1;
	urlComp.dwUrlPathLength = (DWORD)-1;
	urlComp.dwExtraInfoLength = (DWORD)-1;

	BOOL rs = WinHttpCrackUrl(url.data(), (DWORD)url.size(), 0, &urlComp);
	if (rs != TRUE) {
		if (onComplete) {
			onComplete(ATL::AtlHresultFromLastError(), 0, "");
		}
		return {};
	}

	unsigned int id = _NewTaskId();
	auto completeCb = [onComplete, this, id](HRESULT hr, DWORD lastStatusCode, std::string_view response) {
		if (onComplete) {
			onComplete(hr, lastStatusCode, response);
		}

		std::scoped_lock lk(_mutex);
		if (_tasks.find(id) != _tasks.end()) {
			_tasks.erase(id);
		}
	};
	
	_tasks.emplace(id, new PostRequestTask(
		_session,
		CStringW(urlComp.lpszHostName, urlComp.dwHostNameLength),
		CStringW(urlComp.lpszUrlPath, urlComp.dwUrlPathLength + urlComp.dwExtraInfoLength),
		completeCb,
		onProgress,
		headers,
		pairs,
		files,
		httpAuthScheme,
		cred
	));

	return id;
}

unsigned int HttpClient::PostAsync(
	std::wstring_view url,
	std::function<void(HRESULT hr, DWORD lastStatusCode, std::string_view response)> onComplete,
	std::function<void(float progress)> onProgress,
	std::string_view body,
	DWORD httpAuthScheme,
	const std::pair<std::wstring_view, std::wstring_view>& cred,
	const std::map<std::wstring, std::wstring>& headers
) {
	std::scoped_lock lk(_mutex);

	URL_COMPONENTS urlComp{};
	urlComp.dwStructSize = sizeof(urlComp);
	urlComp.dwSchemeLength = (DWORD)-1;
	urlComp.dwHostNameLength = (DWORD)-1;
	urlComp.dwUrlPathLength = (DWORD)-1;
	urlComp.dwExtraInfoLength = (DWORD)-1;

	BOOL rs = WinHttpCrackUrl(url.data(), (DWORD)url.size(), 0, &urlComp);
	if (rs != TRUE) {
		if (onComplete) {
			onComplete(ATL::AtlHresultFromLastError(), 0, "");
		}
		return {};
	}

	unsigned int id = _NewTaskId();
	auto completeCb = [onComplete, this, id](HRESULT hr, DWORD lastStatusCode, std::string_view response) {
		if (onComplete) {
			onComplete(hr, lastStatusCode, response);
		}

		std::scoped_lock lk(_mutex);
		if (_tasks.find(id) != _tasks.end()) {
			_tasks.erase(id);
		}
	};

	_tasks.emplace(id, new PostRequestTask(
		_session,
		CStringW(urlComp.lpszHostName, urlComp.dwHostNameLength),
		CStringW(urlComp.lpszUrlPath, urlComp.dwUrlPathLength + urlComp.dwExtraInfoLength),
		completeCb,
		onProgress,
		headers,
		body,
		httpAuthScheme,
		cred
	));

	return id;
}

bool HttpClient::Wait(unsigned int requestId) {
	std::unique_lock lk(_mutex);

	auto it = _tasks.find(requestId);
	if (it == _tasks.end()) {
		return false;
	}

	lk.unlock();

	it->second->Wait();
	return true;
}

void HttpClient::WaitAll() {
	std::scoped_lock lk(_mutex);
	for (const auto& pair : _tasks) {
		pair.second->Wait();
	}
}

void HttpClient::CancelAll() {
	std::scoped_lock lk(_mutex);
	for (const auto& pair : _tasks) {
		pair.second->Cancel();
	}
}

bool HttpClient::Cancel(unsigned int requestId) {
	std::scoped_lock lk(_mutex);

	auto it = _tasks.find(requestId);
	if (it == _tasks.end()) {
		return false;
	}

	it->second->Cancel();
	return true;
}

bool HttpClient::IsRequesting(unsigned int requestId) {
	std::scoped_lock lk(_mutex);

	auto it = _tasks.find(requestId);
	if (it == _tasks.end()) {
		return false;
	}

	return it->second->IsRequesting();
}


GetRequestTask::GetRequestTask(
	const WinHTTPWrappers::CSession& session,
	LPCWSTR pwszServerName,
	LPCWSTR pwszObjectName,
	std::function<void(HRESULT, DWORD, std::string_view)> onComplete,
	std::function<void(float progress)> onProgress,
	std::wstring_view fileToSave,
	DWORD httpAuthScheme,
	const std::pair<std::wstring_view, std::wstring_view>& cred
) {
	_conn.reset(new WinHTTPWrappers::CConnection());
	HRESULT hr = _conn->Initialize(session, pwszServerName);
	if (FAILED(hr)) {
		onComplete(hr, 0, "");
		return;
	}

	_getReq.reset(new GetRequest());
	if (!fileToSave.empty()) {
		_getReq->m_sFileToDownloadInto = fileToSave;
	}
	hr = _getReq->Initialize(*_conn, pwszObjectName, onComplete, onProgress, httpAuthScheme, cred);
	if (FAILED(hr)) {
		onComplete(hr, 0, "");
		return;
	}

	hr = _getReq->SendRequest();
	if (FAILED(hr)) {
		onComplete(hr, 0, "");
		return;
	}

	return;
}

void GetRequestTask::Wait() {
	if (_getReq) {
		_getReq->Wait();
	}

}

void GetRequestTask::Cancel() {
	if (_getReq) {
		_getReq->MyClose();
	}
}

bool GetRequestTask::IsRequesting() {
	if (!_getReq) {
		return false;
	}

	return _getReq->IsRequesting();
}

PostRequestTask::PostRequestTask(
	const WinHTTPWrappers::CSession& session,
	LPCWSTR pwszServerName,
	LPCWSTR pwszObjectName,
	std::function<void(HRESULT, DWORD, std::string_view)> onComplete,
	std::function<void(float progress)> onProgress,
	const std::map<std::wstring, std::wstring>& headers,
	const std::vector<std::pair<std::string_view, std::string_view>>& pairs,
	const std::vector<std::tuple<std::string_view, std::string_view, std::string_view>>& files,
	DWORD httpAuthScheme,
	const std::pair<std::wstring_view,
	std::wstring_view>& cred
) {
	_conn.reset(new WinHTTPWrappers::CConnection());
	HRESULT hr = _conn->Initialize(session, pwszServerName);
	if (FAILED(hr)) {
		onComplete(hr, 0, "");
		return;
	}

	_postReq.reset(new PostRequest());

	hr = _postReq->Initialize(
		*_conn,
		pwszObjectName,
		onComplete,
		onProgress,
		headers,
		pairs,
		files,
		httpAuthScheme,
		cred
	);
	if (FAILED(hr)) {
		onComplete(hr, 0, "");
		return;
	}

	hr = _postReq->SendRequest();
	if (FAILED(hr)) {
		onComplete(hr, 0, "");
		return;
	}
}

PostRequestTask::PostRequestTask(
	const WinHTTPWrappers::CSession& session,
	LPCWSTR pwszServerName,
	LPCWSTR pwszObjectName,
	std::function<void(HRESULT, DWORD, std::string_view)> onComplete,
	std::function<void(float progress)> onProgress,
	const std::map<std::wstring, std::wstring>& headers,
	std::string_view body,
	DWORD httpAuthScheme,
	const std::pair<std::wstring_view, std::wstring_view>& cred
) {
	_conn.reset(new WinHTTPWrappers::CConnection());
	HRESULT hr = _conn->Initialize(session, pwszServerName);
	if (FAILED(hr)) {
		onComplete(hr, 0, "");
		return;
	}

	_postReq.reset(new PostRequest());

	hr = _postReq->Initialize(
		*_conn,
		pwszObjectName,
		onComplete,
		onProgress,
		headers,
		body,
		httpAuthScheme,
		cred
	);
	if (FAILED(hr)) {
		onComplete(hr, 0, "");
		return;
	}

	hr = _postReq->SendRequest();
	if (FAILED(hr)) {
		onComplete(hr, 0, "");
		return;
	}
}

void PostRequestTask::Wait() {
	if (_postReq) {
		_postReq->Wait();
	}
}

void PostRequestTask::Cancel() {
	if (_postReq) {
		_postReq->MyClose();
	}
}

bool PostRequestTask::IsRequesting() {
	if (!_postReq) {
		return false;
	}

	return _postReq->IsRequesting();
}

}