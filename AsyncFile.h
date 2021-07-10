#pragma once

#include "defines.h"
#include <Windows.h>
#include <vector>

class DLL_PUBLIC IAsyncOperationCompletionNotifier {
public:
    virtual void OnAsyncOperationComplete(bool bRead, DWORD dwErrorCode) = 0;

};


class DLL_PUBLIC AsyncFile {
public:
    static AsyncFile* GetInstance();

    AsyncFile(const wchar_t* lpctszFileName, bool bCreate, DWORD dwDesiredAccess, DWORD dwShareMode,
              IAsyncOperationCompletionNotifier* pINotifier,
              bool bSequentialMode = false,
              __int64 nStartOffset = 0,
              bool bInUIThread = false);
    bool IsOpen();
    bool Write(const char* pvBuffer, DWORD dwBufLen, DWORD dwOffsetLow = 0, DWORD dwOffsetHigh = 0);
    bool Read(LPVOID pvBuffer, DWORD dwBufLen, DWORD dwOffsetLow = 0, DWORD dwOffsetHigh = 0);
    bool IsAsyncIOComplete(bool bFlushBuffers = true);
    bool AbortIO();
    DWORD GetFileLength(DWORD* pdwOffsetHigh);
    __int64 GetLargeFileLength();
    void Reset(bool bSequentialMode = false, __int64 nStartOffset = 0);
    operator HANDLE()     {
        return m_hAsyncFile;
    }
    bool SeekFile(__int64 nBytesToSeek, __int64& nNewOffset, DWORD dwSeekOption);
    ~AsyncFile(void);
private:
    LPOVERLAPPED PreAsyncIO(DWORD dwOffsetLow, DWORD dwOffsetHigh);
    LPOVERLAPPED GetOverlappedPtr(DWORD dwOffsetLow, DWORD dwOffsetHigh);
    bool PostAsyncIO(bool bRet, DWORD dwBufLen);
    bool WaitOnUIThread();
    bool NormalWait();
    void PumpMessage();
    static void WINAPI FileWriteIOCompletionRoutine(DWORD dwErrorCode,
                                                    DWORD dwNumberOfBytesTransfered,
                                                    LPOVERLAPPED lpOverlapped);
    static void WINAPI FileReadIOCompletionRoutine(DWORD dwErrorCode,
                                                   DWORD dwNumberOfBytesTransfered,
                                                   LPOVERLAPPED lpOverlapped);

    static void OnFileIOComplete(DWORD dwErrorCode,
                                 DWORD dwNumberOfBytesTransfered,
                                 LPOVERLAPPED lpOverlapped, bool bRead);
    void CheckAndRaiseIOCompleteEvent();
    void Cleanup(bool bFlushBuffers);
    static void OutputFormattedErrorString(const wchar_t* ptcMsg, DWORD dwErrorCode);
    AsyncFile();
    AsyncFile(const AsyncFile&);
    AsyncFile& operator=(const AsyncFile&);
private:
    IAsyncOperationCompletionNotifier* m_pNotifier;
    HANDLE m_hAsyncFile;
    HANDLE m_hIOCompleteEvent;
    DWORD m_dwAccessMode;
    bool m_bSequentialMode;
    __int64 m_nNumOfBytesRequested;
    __int64 m_nOffset;
    DWORD m_dwReqestedOvpOprnCount;
    DWORD m_dwErrorCode;
    bool m_bInUIThread;
    std::vector<void*> m_pOvpBufArray;
    bool m_bCleanupDone;
    bool m_bAborted;
};
