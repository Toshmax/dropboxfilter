#include "stdafx.h"
#include "Hook.h"
#include "../Util.h"
#include "mhook-lib/mhook.h"
#define ARRAY_COUNT(XXX) (sizeof(XXX)/sizeof((XXX)[0]))
// Overlapp emulator thread. =========================================================================================================

struct OverlappedData
{
	OverlappedData()
	{
		overlapped.hEvent = CreateEvent(NULL,FALSE,FALSE,NULL);
	}
	~OverlappedData()
	{
		CloseHandle(overlapped.hEvent);
	}
	OVERLAPPED overlapped;
	OVERLAPPED *realOverlapped;
	void		*data1;
	void		*data2;
	LONG		inUse;
	void		(*func)(void *,void *);
};

OverlappedData overlappedEmuData[10];

DWORD WINAPI OverlappEmuThread(void *)
{
	HANDLE waitHandles[ARRAY_COUNT(overlappedEmuData)];
	for(int i=0;i<ARRAY_COUNT(overlappedEmuData);i++) {
		waitHandles[i] = overlappedEmuData[i].overlapped.hEvent;
	}
	for(;;) {
		int wait = WaitForMultipleObjects(ARRAY_COUNT(waitHandles),waitHandles,FALSE,INFINITE);
		OverlappedData * data = &overlappedEmuData[wait];
		if(data->overlapped.InternalHigh) {
			data->func(data->data1,data->data2);
		}
		data->realOverlapped->Internal = data->overlapped.Internal;
		data->realOverlapped->InternalHigh = data->overlapped.InternalHigh;
		SetEvent(data->realOverlapped->hEvent);
		data->inUse = 0;
	}
}

OVERLAPPED *EmulateOverlapped(OVERLAPPED *overlapped, void *funcData1, void *funcData2, void (*func)(void *,void *))
{
	for(;;) {
		for(int i=0;i<ARRAY_COUNT(overlappedEmuData);i++) {
			OverlappedData * data = &overlappedEmuData[i];
			if(InterlockedCompareExchange(&data->inUse,1,0) != 0) {
				continue;
			}
			data->data1 = funcData1;
			data->data2 = funcData2;
			data->overlapped.Internal = overlapped->Internal;
			data->overlapped.InternalHigh = overlapped->InternalHigh;
			data->realOverlapped = overlapped;
			data->func = func;
			return &data->overlapped;
		}
		Sleep(100);
	}
}

void EmulateOverlappedRelease(OVERLAPPED *emuOverlapped)
{
	for(int i=0;i<ARRAY_COUNT(overlappedEmuData);i++) {
		if(emuOverlapped != &overlappedEmuData[i].overlapped)
			continue;
		overlappedEmuData[i].inUse = 0;
		return ;
	}
}

// Hooks. ============================================================================================================================

HANDLE handle2pathMutex = CreateMutex(NULL,FALSE,NULL);
map<HANDLE,wstring> handle2path;

////////////////////////////////////////////////////////////////////////////
typedef BOOL
WINAPI
FindNextFileW_t(
				__in  HANDLE hFindFile,
				__out LPWIN32_FIND_DATAW lpFindFileData
				);

FindNextFileW_t *FindNextFileW_sys = &FindNextFileW;

BOOL
WINAPI
FindNextFileW_hook(
				   __in  HANDLE hFindFile,
				   __out LPWIN32_FIND_DATAW lpFindFileData
				   )
{
	BOOL ret;
	for(;;) {
		ret = FindNextFileW_sys(hFindFile,lpFindFileData);
		if(!ret)
			break;
		if(wcscmp(lpFindFileData->cFileName,L"..") == 0 || wcscmp(lpFindFileData->cFileName,L".") == 0)
			break;

		wchar_t path[1024*10]=L"";
		WaitForSingleObject(handle2pathMutex,INFINITE);
		map<HANDLE,wstring>::iterator iter = handle2path.find(hFindFile);
		if(iter != handle2path.end()) {
			wcscpy(path,iter->second.c_str());
		}
		ReleaseMutex(handle2pathMutex);
		wcscat(path,lpFindFileData->cFileName);

		if(!Filter(path,lpFindFileData->dwFileAttributes))
			break;
	}
	return ret;
}

////////////////////////////////////////////////////////////////////////////
typedef BOOL
WINAPI
FindClose_t(
				__in  HANDLE hFindFile
				);

FindClose_t *FindClose_sys = &FindClose;

BOOL
WINAPI
FindClose_hook(
				   __in  HANDLE hFindFile
				   )
{
	WaitForSingleObject(handle2pathMutex,INFINITE);
	map<HANDLE,wstring>::iterator iter = handle2path.find(hFindFile);
	if(iter != handle2path.end()) {
		handle2path.erase(iter);
	}
	ReleaseMutex(handle2pathMutex);
	return FindClose_sys(hFindFile);
}

////////////////////////////////////////////////////////////////////////////
typedef __out
HANDLE
WINAPI
FindFirstFileExW_t(
    __in       LPCWSTR lpFileName,
    __in       FINDEX_INFO_LEVELS fInfoLevelId,
    __out      LPVOID lpFindFileData,
    __in       FINDEX_SEARCH_OPS fSearchOp,
    __reserved LPVOID lpSearchFilter,
    __in       DWORD dwAdditionalFlags
    );

FindFirstFileExW_t *FindFirstFileExW_sys = &FindFirstFileExW;

__out
HANDLE
WINAPI
FindFirstFileExW_hook(
    __in       LPCWSTR lpFileName,
    __in       FINDEX_INFO_LEVELS fInfoLevelId,
    __out      LPVOID _lpFindFileData,
    __in       FINDEX_SEARCH_OPS fSearchOp,
    __reserved LPVOID lpSearchFilter,
    __in       DWORD dwAdditionalFlags
    )
{
	WIN32_FIND_DATAW *lpFindFileData = (WIN32_FIND_DATAW *)_lpFindFileData;
	HANDLE hRet = FindFirstFileExW_sys(lpFileName,fInfoLevelId,lpFindFileData,fSearchOp,lpSearchFilter,dwAdditionalFlags);
	if(hRet == INVALID_HANDLE_VALUE)
		return hRet;

	wchar_t path[1024*10];
	wcscpy(path,lpFileName);
	wchar_t *lastSep=NULL;
	for(int i=0;path[i];i++) {
		if(path[i] == '\\' || path[i] == '/') {
			lastSep = path+i;
		}
	}
	if(lastSep)
		lastSep[1] = 0;
	WaitForSingleObject(handle2pathMutex,INFINITE);
	handle2path[hRet] = path;
	ReleaseMutex(handle2pathMutex);

	for(;;) {
		if(wcscmp(lpFindFileData->cFileName,L"..") == 0 || wcscmp(lpFindFileData->cFileName,L".") == 0)
			break;
		int len = wcslen(path);
		wcscat(path,lpFindFileData->cFileName);
		if(!Filter(path,lpFindFileData->dwFileAttributes))
			break;
		path[len] = 0;
		BOOL ret = FindNextFileW_sys(hRet,lpFindFileData);
		if(!ret) {
			FindClose(hRet);
			return INVALID_HANDLE_VALUE;
		}
	}
	return hRet;
}

///////////////////////////////////////////////////////////////////////////
typedef __out
HANDLE
WINAPI
FindFirstFileW_t(
			   __in  LPCWSTR lpFileName,
			   __out LPWIN32_FIND_DATAW lpFindFileData
			   );

FindFirstFileW_t *FindFirstFileW_sys = &FindFirstFileW;

__out
HANDLE
WINAPI
FindFirstFileW_hook(
				 __in  LPCWSTR lpFileName,
				 __out LPWIN32_FIND_DATAW lpFindFileData
				 )
{
	HANDLE hRet = FindFirstFileW_sys(lpFileName,lpFindFileData);
	if(hRet == INVALID_HANDLE_VALUE)
		return hRet;

	wchar_t path[1024*10];
	wcscpy(path,lpFileName);
	wchar_t *lastSep=NULL;
	for(int i=0;path[i];i++) {
		if(path[i] == '\\' || path[i] == '/') {
			lastSep = path+i;
		}
	}
	if(lastSep)
		lastSep[1] = 0;
	WaitForSingleObject(handle2pathMutex,INFINITE);
	handle2path[hRet] = path;
	ReleaseMutex(handle2pathMutex);

	for(;;) {
		if(wcscmp(lpFindFileData->cFileName,L"..") == 0 || wcscmp(lpFindFileData->cFileName,L".") == 0)
			break;
		int len = wcslen(path);
		wcscat(path,lpFindFileData->cFileName);
		if(!Filter(path,lpFindFileData->dwFileAttributes))
			break;
		path[len] = 0;
		BOOL ret = FindNextFileW_sys(hRet,lpFindFileData);
		if(!ret) {
			FindClose(hRet);
			return INVALID_HANDLE_VALUE;
		}
	}
	return hRet;
}

////////////////////////////////////////////////////////////////////////////////////////////////////
typedef BOOL
WINAPI
ReadDirectoryChangesW_t(
					  __in        HANDLE hDirectory,
					  __out_bcount_part(nBufferLength, *lpBytesReturned) LPVOID lpBuffer,
					  __in        DWORD nBufferLength,
					  __in        BOOL bWatchSubtree,
					  __in        DWORD dwNotifyFilter,
					  __out_opt   LPDWORD lpBytesReturned,
					  __inout_opt LPOVERLAPPED lpOverlapped,
					  __in_opt    LPOVERLAPPED_COMPLETION_ROUTINE lpCompletionRoutine
					  );
ReadDirectoryChangesW_t *ReadDirectoryChangesW_sys = &ReadDirectoryChangesW;

void FilterNotifyInformation(void *_info,HANDLE hDirectory)
{
	FILE_NOTIFY_INFORMATION *in = (PFILE_NOTIFY_INFORMATION)_info;
	for(;;) {
		if(in->Action != FILE_ACTION_REMOVED) {
			wchar_t nameBuf[1024*10];
			wchar_t *name = PathFromHandle(hDirectory,nameBuf,sizeof(nameBuf));
			if(name != NULL) {
				size_t inIdx=0;
				size_t outIdx = wcslen(name);
				name[outIdx++] = '\\';
				for(;inIdx < in->FileNameLength/2;outIdx++,inIdx++) {
					name[outIdx] = in->FileName[inIdx];
				}
				name[outIdx] = 0;
				if(Filter(name,GetFileAttributesW(name))) {
					in->FileName[0] = '@';
				}
			}
		}

		if(in->NextEntryOffset == 0) {
			break;
		}
		in = (FILE_NOTIFY_INFORMATION*)(size_t(in)+in->NextEntryOffset);
	}
}

BOOL
WINAPI
ReadDirectoryChangesW_hook(
						__in        HANDLE hDirectory,
						__out_bcount_part(nBufferLength, *lpBytesReturned) LPVOID lpBuffer,
						__in        DWORD nBufferLength,
						__in        BOOL bWatchSubtree,
						__in        DWORD dwNotifyFilter,
						__out_opt   LPDWORD lpBytesReturned,
						__inout_opt LPOVERLAPPED lpOverlapped,
						__in_opt    LPOVERLAPPED_COMPLETION_ROUTINE lpCompletionRoutine
						)
{
	if(lpOverlapped) {
		OVERLAPPED *emuOverlapped = EmulateOverlapped(lpOverlapped,lpBuffer,hDirectory,FilterNotifyInformation);
		BOOL ret = ReadDirectoryChangesW_sys(hDirectory,lpBuffer,nBufferLength,bWatchSubtree,dwNotifyFilter,lpBytesReturned,emuOverlapped,NULL);
		if(!ret) {
			EmulateOverlappedRelease(emuOverlapped);
		}
		return ret;
	}

	BOOL ret = ReadDirectoryChangesW_sys(hDirectory,lpBuffer,nBufferLength,bWatchSubtree,dwNotifyFilter,lpBytesReturned,lpOverlapped,lpCompletionRoutine);
	if(ret) {
		FilterNotifyInformation(lpBuffer,hDirectory);
	}
	return ret;
}

HANDLE hEmuThread;

void Hook()
{
	hEmuThread = CreateThread(NULL,0,OverlappEmuThread,NULL,0,NULL);
	Mhook_SetHook((void **)&FindFirstFileW_sys,FindFirstFileW_hook);
	Mhook_SetHook((void **)&FindFirstFileExW_sys,FindFirstFileExW_hook);
	Mhook_SetHook((void **)&FindNextFileW_sys,FindNextFileW_hook);
	Mhook_SetHook((void **)&ReadDirectoryChangesW_sys,ReadDirectoryChangesW_hook);
}

void UnHook()
{
	Mhook_Unhook((void **)&FindFirstFileW_sys);
	Mhook_Unhook((void **)&FindFirstFileExW_sys);
	Mhook_Unhook((void **)&FindNextFileW_sys);
	Mhook_Unhook((void **)&ReadDirectoryChangesW_sys);
	TerminateThread(hEmuThread,0);
	CloseHandle(hEmuThread);
}
