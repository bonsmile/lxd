#include "AsyncFile.h"
#include <stdio.h>
#include <debugapi.h>
#include "fileio.h"

//#define TRACE_ENABLED

#ifdef TRACE_ENABLED
#define TRACE_STATUS(msg)\
        OutputDebugString(msg);
#define TRACE_ERROR(msg,errorcode)\
        OutputFormattedErrorString(msg,errorcode);
#else
#define TRACE_STATUS(msg)
#define TRACE_ERROR(msg,errorcode)
#endif

AsyncFile* AsyncFile::GetInstance() {
    static AsyncFile instance(L"bonsmile.log", false, GENERIC_WRITE, FILE_SHARE_READ, nullptr);
    return &instance;
}

typedef struct OVERLAPPEDEX : public OVERLAPPED {
    LPVOID pCustomData; // Append custom data to the end of the system defined structure
}*LPOVERLAPPEDEX;

const int MAX_TIMEOUT = 60 * 1000; // 1 minute, thinks it is sufficient

/*//////////////////////////////////////////////////////////////////////////////////////

The constructor which opens the specified file in overlapped mode. It creates
the file bCreate = true, otherwise open existing.
An I/O callback interface can be specified by the client.
This shall be invoked when the IOCompletion routine is triggered by the system.

The AyncFile object can be created in sequential mode. In this case the offset
shall be calculated internally. But if the clients specifies the offset, this
flag is reset and client specified offset will be used.

The client can also specify whether it is in a UI thread or not. If UI thread,
the message pumbing shall be done to make the UI responsive.

lpctszFileName               - The full path to file

bCreate                      - Whether a new file needs to be created or not.
                              If the file already exists, the contents will be lost.

dwDesiredAccess, dwShareMode - Same as that for CreateFile

pINotifier                   - The callback interface specified by client.
                               Its OnAsyncOperationComplete function shall be invoked
                               from IOCompletion routine.

bSequentialMode              - If this flag is true the offset is incremented sequentially. Here
                               the client does not need to specify the offset in Read/Write calls.
                               Otherwise client needs to specify the offset during the Read/Write calls.

nStartOffset                 - The offset from which Read/Write needs to be started. This is relevant
                               only in case bSequentialMode = true

bInUIThread                  - Whether asynchronous I/O is performed in the context of UI thread or not

///////////////////////////////////////////////////////////////////////////////////////*/

AsyncFile::AsyncFile(const wchar_t* lpctszFileName, bool bCreate,
                     DWORD dwDesiredAccess, DWORD dwShareMode,
                     IAsyncOperationCompletionNotifier* pINotifier,
                     bool bSequentialMode,
                     __int64 nStartOffset,
                     bool bInUIThread)
    :m_pNotifier(pINotifier),
    m_hAsyncFile(INVALID_HANDLE_VALUE),
    m_hIOCompleteEvent(0),
    m_dwAccessMode(dwDesiredAccess),
    m_bSequentialMode(bSequentialMode),
    m_nNumOfBytesRequested(0),
    m_nOffset(nStartOffset),
    m_dwReqestedOvpOprnCount(0),
    m_dwErrorCode(0),
    m_bInUIThread(bInUIThread),
    m_bCleanupDone(false),
    m_bAborted(false) {
    DWORD dwCreateMode = CREATE_NEW;
    if(true == bCreate)     {

        dwCreateMode = CREATE_ALWAYS;
    }
    if(0 != lpctszFileName)     {
        if(lxd::FileExists(lpctszFileName)) {
            m_hAsyncFile = lxd::OpenFile(lpctszFileName, lxd::WriteOnly | lxd::Append);
        } else {
            m_hAsyncFile = CreateFile(lpctszFileName, dwDesiredAccess, dwShareMode, 0, dwCreateMode,
                                      FILE_FLAG_OVERLAPPED, 0);
        }
    }
    if(INVALID_HANDLE_VALUE == m_hAsyncFile)     {
        wchar_t tcszErrMsg[100] = {0};
        swprintf_s(tcszErrMsg, 100, L"File open failed -->%s", lpctszFileName);
        TRACE_ERROR(tcszErrMsg, GetLastError());
    }
    m_hIOCompleteEvent = CreateEvent(0, true, false, 0);
}

/*//////////////////////////////////////////////////////////////////////////////////////

Returns true if the file is successfully opened.

///////////////////////////////////////////////////////////////////////////////////////*/

bool AsyncFile::IsOpen() {
    return (INVALID_HANDLE_VALUE != m_hAsyncFile);
}

/*//////////////////////////////////////////////////////////////////////////////////////
Performs the asynchronous Write operation.

pvBuffer    - The buffer to be written
dwBufLen    - The length of the buffer specified
dwOffsetLow - The lower part of the offset where input data is to be written
dwOffsetHigh- The higher part of the offset where input data is to be written. Normally
              this is specified for big files > 4 GB
///////////////////////////////////////////////////////////////////////////////////////*/

bool AsyncFile::Write(const char* pvBuffer, DWORD dwBufLen, DWORD dwOffsetLow, DWORD dwOffsetHigh) {
    if(0 == pvBuffer)
        return false;
    LPOVERLAPPEDEX pOverlapped = 0;
    if(0 == (pOverlapped = static_cast<LPOVERLAPPEDEX>(PreAsyncIO(dwOffsetLow, dwOffsetHigh))))     {
        return false;
    }

    bool bRet = WriteFileEx(m_hAsyncFile, pvBuffer, dwBufLen, pOverlapped,
                            FileWriteIOCompletionRoutine);
    if(false == bRet)     {
        TRACE_STATUS(L"WriteFileEx completed --> with error");
    }     else     {
        TRACE_STATUS(L"WriteFileEx completed --> with success");
    }
    return PostAsyncIO(bRet, dwBufLen);
}

/*//////////////////////////////////////////////////////////////////////////////////////
Performs the asynchronous Read operation.

pvBuffer    - The buffer to be read
dwBufLen    - The length of the buffer specified
dwOffsetLow - The lower part of the offset from where data is to be read
dwOffsetHigh- The higher part of the offset from where data is to be read. Normally
              this is specified for big files > 4 GB
///////////////////////////////////////////////////////////////////////////////////////*/

bool AsyncFile::Read(LPVOID pvBuffer, DWORD dwBufLen, DWORD dwOffsetLow, DWORD dwOffsetHigh) {
    if(0 == pvBuffer)
        return false;
    LPOVERLAPPED pOverlapped = 0;
    if(0 == (pOverlapped = PreAsyncIO(dwOffsetLow, dwOffsetHigh)))     {
        return false;
    }
    bool bRet = ReadFileEx(m_hAsyncFile, pvBuffer, dwBufLen, pOverlapped,
                           FileReadIOCompletionRoutine);
    if(false == bRet)     {
        TRACE_STATUS(L"ReadFileEx completed --> with error");
    }     else     {
        TRACE_STATUS(L"ReadFileEx completed --> with success");
    }
    return PostAsyncIO(bRet, dwBufLen);
}


/*//////////////////////////////////////////////////////////////////////////////////////
The common routine called from Read/Write to prepare the OVERLAPPEDEX pointer with the help
of GetOverlappedPtr(). It keeps the OVERLAPPEDEX pointer in an array to be used during the
GetOverlappedResult call.

dwOffsetLow - The lower part of the offset from where data is to be read/written
dwOffsetHigh- The higher part of the offset from where data is to be read/written. Normally
              this is specified for big files > 4 GB
///////////////////////////////////////////////////////////////////////////////////////*/

LPOVERLAPPED AsyncFile::PreAsyncIO(DWORD dwOffsetLow, DWORD dwOffsetHigh) {
    if(!IsOpen())     {
        return 0;
    }
    LPOVERLAPPED pOverlapped = GetOverlappedPtr(dwOffsetLow, dwOffsetHigh);
    if(0 == pOverlapped)     {
        return 0;
    }
    m_pOvpBufArray.push_back(pOverlapped);
    ++m_dwReqestedOvpOprnCount;
    m_bAborted = false;
    return pOverlapped;
}

/*//////////////////////////////////////////////////////////////////////////////////////
The common routine called from Read/Write to prepare the OVERLAPPEDEX pointer. It also
fills the custom pointer to make the context available during the OnFileIOComplete

dwOffsetLow - The lower part of the offset from where data is to be read/written
dwOffsetHigh- The higher part of the offset from where data is to be read/written. Normally
              this is specified for big files > 4 GB
///////////////////////////////////////////////////////////////////////////////////////*/

LPOVERLAPPED AsyncFile::GetOverlappedPtr(DWORD dwOffsetLow, DWORD dwOffsetHigh) {
    LPOVERLAPPEDEX pOverlapped = new OVERLAPPEDEX;
    ZeroMemory(pOverlapped, sizeof(OVERLAPPEDEX));
    if(0 == pOverlapped)     {
        return 0;
    }

    //if the client specified offset, override the m_bSequentialMode
    if((0 != dwOffsetLow) || (0 != dwOffsetHigh))     {
        m_bSequentialMode = false;
    }
    if(true == m_bSequentialMode)     {
        LARGE_INTEGER LargeOffset = {0};
        LargeOffset.QuadPart = m_nOffset;
        pOverlapped->Offset = LargeOffset.LowPart;
        pOverlapped->OffsetHigh = LargeOffset.HighPart;
    }     else     {
        pOverlapped->Offset = dwOffsetLow;
        pOverlapped->OffsetHigh = dwOffsetHigh;
    }
    pOverlapped->pCustomData = this;
    return pOverlapped;
}

/*//////////////////////////////////////////////////////////////////////////////////////
The common routine called from Read/Write after the asynchronous I/O call. It calculates
the next offset if the m_bSequentialMode flag is true. Also it keeps track of bytes requested
to Read/Write. This is to compare it after the whole I/O is completed. If all the bytes are
not Read/Write there is data loss. In the case of UI thread it dispatches the window messages
to make the UI responsive.

bRet     - The return of ReadFileEx/WriteFileEx
dwBufLen - The length of data buffer specified
///////////////////////////////////////////////////////////////////////////////////////*/

bool AsyncFile::PostAsyncIO(bool bRet, DWORD dwBufLen) {
    if(m_bInUIThread)     {
        PumpMessage();
    }
    DWORD dwLastError = GetLastError();
    if(false == bRet)     {
        TRACE_ERROR(L"IO operation failed", dwLastError);
        return false;
    }

    if(ERROR_SUCCESS == dwLastError)     {
        if(true == m_bSequentialMode)         {
            m_nOffset += dwBufLen;
        }
        m_nNumOfBytesRequested += dwBufLen;
        return true;
    }
    return false;
}

/*//////////////////////////////////////////////////////////////////////////////////////
The interface to cancel the asynchronous I/O operation. It signals the IOCompletion event
to stop waiting for the I/O to complete.
///////////////////////////////////////////////////////////////////////////////////////*/

bool AsyncFile::AbortIO() {
    bool bRet = CancelIo(m_hAsyncFile);
    if(false == bRet)     {
        TRACE_ERROR(L"CancelIo failed", GetLastError());
    }     else     {
        TRACE_STATUS(L"CancelIo completed --> with success");
    }
    ::SetEvent(m_hIOCompleteEvent);
    m_bAborted = true;
    return bRet;
}

/*//////////////////////////////////////////////////////////////////////////////////////
The call back function registered with WriteFileEx. This will be invoked when a WriteFilEx
oeration is completed.

dwErrorCode                 - The status of the operation
dwNumberOfBytesTransfered   - The number of bytes written
lpOverlapped                - The OVERLAPPEDEX pointer provided during WriteFileEx call
///////////////////////////////////////////////////////////////////////////////////////*/

void WINAPI AsyncFile::FileWriteIOCompletionRoutine(DWORD dwErrorCode,
                                                    DWORD dwNumberOfBytesTransfered,
                                                    LPOVERLAPPED lpOverlapped) {
    OnFileIOComplete(dwErrorCode, dwNumberOfBytesTransfered, lpOverlapped, false);
}

/*//////////////////////////////////////////////////////////////////////////////////////
The call back function registered with WriteFileEx. This will be invoked when a ReadFileEx
oeration is completed.

dwErrorCode                 - The status of the operation
dwNumberOfBytesTransfered   - The number of bytes read
lpOverlapped                - The OVERLAPPEDEX pointer provided during ReadFileEx call
///////////////////////////////////////////////////////////////////////////////////////*/

void WINAPI AsyncFile::FileReadIOCompletionRoutine(DWORD dwErrorCode,
                                                   DWORD dwNumberOfBytesTransfered,
                                                   LPOVERLAPPED lpOverlapped) {
    OnFileIOComplete(dwErrorCode, dwNumberOfBytesTransfered, lpOverlapped, true);
}

/*//////////////////////////////////////////////////////////////////////////////////////
The common routine which handles call back function registered with ReadFileEx/WriteFileEx.
This will be invoked when a ReadFileEx/WriteFileEx oeration is completed. It invokes the
callback interface registered by the client.

dwErrorCode                 - The status of the operation
dwNumberOfBytesTransfered   - The number of bytes read
lpOverlapped                - The OVERLAPPEDEX pointer provided during ReadFileEx/WriteFileEx
                              call
bRead                       - Identified Read/Write
///////////////////////////////////////////////////////////////////////////////////////*/

void AsyncFile::OnFileIOComplete(DWORD dwErrorCode,
                                 DWORD dwNumberOfBytesTransfered,
                                 LPOVERLAPPED lpOverlapped, bool bRead) {
    if(0 == lpOverlapped)     {
        TRACE_STATUS(L"FileIOCompletionRoutine completed --> with POVERLAPPED NULL");
        return;
    }
    LPOVERLAPPEDEX pOverlappedEx = static_cast<LPOVERLAPPEDEX>(lpOverlapped);
    AsyncFile* pAsyncFile = reinterpret_cast<AsyncFile*>(pOverlappedEx->pCustomData);
    pAsyncFile->m_dwErrorCode = dwErrorCode;

    if(ERROR_SUCCESS == dwErrorCode)     {
        TRACE_STATUS(L"FileIOCompletionRoutine completed --> with success");
    }     else     {
        TRACE_ERROR(L"FileIOCompletionRoutine completed", dwErrorCode);
    }
    pAsyncFile->CheckAndRaiseIOCompleteEvent();
    if(pAsyncFile->m_pNotifier)     {
        pAsyncFile->m_pNotifier->OnAsyncOperationComplete(bRead, dwErrorCode);
    }
}

/*//////////////////////////////////////////////////////////////////////////////////////
Checks whether all the requested overlapped calls completed by decrementing a counter.
If the counter is 0, all the requested asynchronous operations got callbacks. Then it
will set the m_hIOCompleteEvent to end the alertable wait state.
///////////////////////////////////////////////////////////////////////////////////////*/

void AsyncFile::CheckAndRaiseIOCompleteEvent() {
    (m_dwReqestedOvpOprnCount > 0) ? --m_dwReqestedOvpOprnCount : m_dwReqestedOvpOprnCount;
    if(0 == m_dwReqestedOvpOprnCount)     {
        ::SetEvent(m_hIOCompleteEvent);
    }
}

/*//////////////////////////////////////////////////////////////////////////////////////
The multiple overlapped Read/Write requests can be done. Now, the thread is free to do
other operations. Finally, when the thread needs to know the status of the asynchronous
operation, it can place the thread in alretable wait state with the help of
WaitForSingleObjectEx or MsgWaitForMultipleObjectsEx(for UI thread). Then the IOCompletion
routine registered will be called per asynchrnous I/O call requested. Once all the callbacks
are invoked the m_hIOCompleteEvent is signalled and the alertable wait state is completed.
Now, the GetOverlappedResult() API is invoked to find whether all the requested I/O operations
compeleted successfully.

bFlushBuffers - Whether to flush the file buffers or not.
                This is required for preventing data loss as system performs a delayed write.
                So when an abnormal shutdown/restart of system happens, data is lost.
///////////////////////////////////////////////////////////////////////////////////////*/

bool AsyncFile::IsAsyncIOComplete(bool bFlushBuffers) {
    if(!IsOpen())     {
        return false;
    }

    bool bWaitReturn = false;
    if(m_bInUIThread)     {
        bWaitReturn = WaitOnUIThread();
    }     else     {
        bWaitReturn = NormalWait();
    }

    int nTotalNumOfBytesTrans = 0;
    int nOvpBufferCount = m_pOvpBufArray.size();

    for(int nIdx = 0; nIdx < nOvpBufferCount; ++nIdx)     {
        LPOVERLAPPED lpOverlapped = reinterpret_cast<LPOVERLAPPED>(m_pOvpBufArray[nIdx]);
        DWORD dwNumberOfBytesTransferred = 0;
        bool bRet = GetOverlappedResult(m_hAsyncFile, lpOverlapped, &dwNumberOfBytesTransferred, true);
        if(false == bRet)         {
            m_dwErrorCode = GetLastError();
            if(ERROR_IO_INCOMPLETE == m_dwErrorCode)             {
                TRACE_STATUS(L"IsAsyncIOComplete --> IO pending...");
            }
        }
        nTotalNumOfBytesTrans += dwNumberOfBytesTransferred;
        if(true == m_bInUIThread)         {
            PumpMessage();
        }
    }

    Cleanup(bFlushBuffers);

    if(m_nNumOfBytesRequested == nTotalNumOfBytesTrans)     {
        m_nNumOfBytesRequested = 0;
        return true;
    }

    if(false == bWaitReturn)     {
        return false;
    }
    if(ERROR_SUCCESS != m_dwErrorCode)     {
        TRACE_ERROR(L"AsyncIO operation completed with error", m_dwErrorCode);
        return false;
    }
    return false;
}

/*//////////////////////////////////////////////////////////////////////////////////////
Clears all the data structures. If bFlushBuffers = true calls, FlushFileBuffers() API
to write all the pending data to disk.

bFlushBuffers - Whether to flush the file buffers or not.
                This is required for preventing data loss as system performs a delayed write.
                So when an abnormal shutdown/restart of system happens, data is lost.
///////////////////////////////////////////////////////////////////////////////////////*/

void AsyncFile::Cleanup(bool bFlushBuffers) {
    if(true == m_bCleanupDone)
        return;
    int nOvpBufCount = m_pOvpBufArray.size();
    for(int nIdx = 0; nIdx < nOvpBufCount; ++nIdx)     {
        LPOVERLAPPEDEX pOverlapped = reinterpret_cast<LPOVERLAPPEDEX>(m_pOvpBufArray[nIdx]);
        delete pOverlapped;
    }

    m_pOvpBufArray.clear();

    if((0 != m_hAsyncFile) && (GENERIC_READ != m_dwAccessMode) && (true == bFlushBuffers) && (false == m_bAborted))     {
        TRACE_STATUS(L"Flushing file buffers...");
        if(false == FlushFileBuffers(m_hAsyncFile))         {
            TRACE_ERROR(L"FlushFileBuffers failed", GetLastError());
        }
    }
    ::ResetEvent(m_hIOCompleteEvent);
    m_bCleanupDone = true;
}

/*//////////////////////////////////////////////////////////////////////////////////////
The IOCompletion callbacks shall be invoked by the system, when the thread is palaced in
the alertable wait state. Actaully, from my observation, these callbacks are called in the
context of the wait API. In the case of UI thread, to make the UI responsive the window
messages should be dispatched. So the MsgWaitForMultipleEx() API is used to wait. Therefore
when a message is available in the queue, it will be dispatched. The function returns failure
in the case of timeout or any other failure. Otherwise it waits till the m_hIOCompleteEvent
is signalled.
///////////////////////////////////////////////////////////////////////////////////////*/

bool AsyncFile::WaitOnUIThread() {
    for(;; )     {
        DWORD dwWaitOvpOprn = MsgWaitForMultipleObjectsEx(1, &m_hIOCompleteEvent,
                                                          MAX_TIMEOUT, QS_ALLEVENTS, MWMO_ALERTABLE | MWMO_INPUTAVAILABLE);
        switch(dwWaitOvpOprn)         {
            case WAIT_FAILED:
                return false;
            case WAIT_OBJECT_0:
                return true;
            case WAIT_TIMEOUT:
                return false;
        }

        if(m_bAborted)         {
            return false;
        }

        // Make the UI responsive, dispatch any message in the queue
        PumpMessage();
    }
    return false;
}

/*//////////////////////////////////////////////////////////////////////////////////////
This is for non UI threads. The IOCompletion callbacks shall be invoked by the system,
when the thread is palaced in the alertable wait state. Actaully, from my observation,
these callbacks are called in the context of the wait API. The function returns failure
in the case of timeout or any other failure. Otherwise it waits till the m_hIOCompleteEvent
is signalled.
///////////////////////////////////////////////////////////////////////////////////////*/

bool AsyncFile::NormalWait() {
    for(; ; )     {
        DWORD dwWaitOvpOprn = WaitForSingleObjectEx(m_hIOCompleteEvent, MAX_TIMEOUT, true);
        switch(dwWaitOvpOprn)         {
            case WAIT_FAILED:
                return false;
            case WAIT_OBJECT_0:
                return true;
            case WAIT_TIMEOUT:
                return false;
        }

        if(m_bAborted)         {
            return false;
        }
    }
    return false;
}

/*//////////////////////////////////////////////////////////////////////////////////////
To prepare the AsyncFile instance for next set of I/O operations. Resets all required
members.
///////////////////////////////////////////////////////////////////////////////////////*/

void AsyncFile::Reset(bool bSequentialMode, __int64 nStartOffset) {
    m_bSequentialMode = bSequentialMode;
    m_nOffset = nStartOffset;
    m_nNumOfBytesRequested = 0;
    m_dwReqestedOvpOprnCount = 0;
    m_dwErrorCode = 0;
    m_bCleanupDone = false;
    m_bAborted = false;
}

/*//////////////////////////////////////////////////////////////////////////////////////
Returns the size of the file. This function can be used if the size is required as low
and high parts.
///////////////////////////////////////////////////////////////////////////////////////*/

DWORD AsyncFile::GetFileLength(DWORD* pdwOffsetHigh) {
    return GetFileSize(m_hAsyncFile, pdwOffsetHigh);
}

/*//////////////////////////////////////////////////////////////////////////////////////
Returns the size of the file. This function can be used if the size as an int64 value.
///////////////////////////////////////////////////////////////////////////////////////*/

__int64 AsyncFile::GetLargeFileLength() {
    LARGE_INTEGER liFileSize = {0};
    bool bRet = GetFileSizeEx(m_hAsyncFile, &liFileSize);
    if(false != bRet)     {
        return liFileSize.QuadPart;
    }
    return -1;
}

/*//////////////////////////////////////////////////////////////////////////////////////
Wrapper for the SetFilePointer() API. Moves the file pointer to the specified offset.
///////////////////////////////////////////////////////////////////////////////////////*/

bool AsyncFile::SeekFile(__int64 nBytesToSeek, __int64& nNewOffset, DWORD dwSeekOption) {
    LARGE_INTEGER liBytesToSeek = {0};
    liBytesToSeek.QuadPart = nBytesToSeek;
    LARGE_INTEGER liNewOffset = {0};
    bool bRet = SetFilePointerEx(m_hAsyncFile, liBytesToSeek, &liNewOffset, dwSeekOption);
    if(bRet)     {
        nNewOffset = liNewOffset.QuadPart;
        return true;
    }
    return false;
}

/*//////////////////////////////////////////////////////////////////////////////////////
The destructor. Obiviously, clears all the members.
///////////////////////////////////////////////////////////////////////////////////////*/

AsyncFile::~AsyncFile(void) {
    Cleanup(true);
    ::SetEvent(m_hIOCompleteEvent);
    CloseHandle(m_hIOCompleteEvent);
    m_hIOCompleteEvent = 0;
    if(IsOpen())     {
        CloseHandle(m_hAsyncFile);
    }
    m_hAsyncFile = 0;
}

/*//////////////////////////////////////////////////////////////////////////////////////
Dispatches the window messages to make the UI responsive.
///////////////////////////////////////////////////////////////////////////////////////*/

void AsyncFile::PumpMessage() {
    MSG stMsg = {0};
    while(::PeekMessage(&stMsg, 0, 0, 0, PM_REMOVE))     {
        TranslateMessage(&stMsg);
        DispatchMessage(&stMsg);
    }
}

/*//////////////////////////////////////////////////////////////////////////////////////
Formats error message from system error code.
///////////////////////////////////////////////////////////////////////////////////////*/

void AsyncFile::OutputFormattedErrorString(const wchar_t* ptcMsg, DWORD dwErrorCode) {
    LPTSTR lptszMsgBuf = 0;
    DWORD dwBufLen = 0;
    if(0 != (dwBufLen = FormatMessage(FORMAT_MESSAGE_ALLOCATE_BUFFER |
                                      FORMAT_MESSAGE_FROM_SYSTEM |
                                      FORMAT_MESSAGE_IGNORE_INSERTS,
                                      0, dwErrorCode,
                                      MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
                                      reinterpret_cast<LPTSTR>(&lptszMsgBuf), 0, 0)))     {
        const wchar_t* INFIX = L"-->";
        dwBufLen += wcslen(ptcMsg) + wcslen(INFIX) + 1;
        wchar_t* ptcErrMsg = new wchar_t[dwBufLen];
        ZeroMemory(ptcErrMsg, sizeof(wchar_t) * dwBufLen);
        swprintf_s(ptcErrMsg, dwBufLen, L"%s%s%s", ptcMsg, INFIX, lptszMsgBuf);
        TRACE_STATUS(ptcErrMsg);
        LocalFree(lptszMsgBuf);
        delete[] ptcErrMsg;
    }
}
