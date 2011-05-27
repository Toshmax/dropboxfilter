#pragma once

DWORD FindProcessIdByName(const char *name);
bool RemoteLoadDll(HANDLE hProcess,const char *dllPath);
bool RemoteLoadDll(DWORD processId,const char *dllPath);
wchar_t *PathFromHandle(HANDLE h,void *buf,size_t bufSize);


struct AutoFreeHandle
{
	HANDLE h;
	AutoFreeHandle(HANDLE _h): h(_h){}
	~AutoFreeHandle()
	{
		CloseHandle(h);
	}
	operator HANDLE() { return h; }
};
