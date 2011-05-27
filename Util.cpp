#include "StdAfx.h"
#include "Util.h"

DWORD FindProcessIdByName(const char *name)
{
	DWORD id = 0;
	HANDLE  hSysSnapshot = NULL;
	PROCESSENTRY32 proc;

	proc.dwSize = sizeof(proc);
	hSysSnapshot = CreateToolhelp32Snapshot ( TH32CS_SNAPPROCESS, 0 );
	if ( hSysSnapshot == (HANDLE)-1 )
		return 0;
	if ( Process32First ( hSysSnapshot, &proc ) )
	{
		proc.dwSize = sizeof(proc);
		do
		{
			if(stricmp(proc.szExeFile,name) == 0) {
				id = proc.th32ProcessID;
				break;
			}
		}
		while ( Process32Next ( hSysSnapshot, &proc ) );

	}
	CloseHandle ( hSysSnapshot );
	return id;
}

bool RemoteLoadDll(HANDLE hProcess,const char *dllPath)
{
	// Allocate memory and copy over the dll path.
	char *remoteMemory = (char *)VirtualAllocEx(hProcess,NULL,strlen(dllPath)+1,MEM_COMMIT,PAGE_READWRITE);
	if(remoteMemory == NULL)
		return false;

	if(WriteProcessMemory(hProcess,remoteMemory,dllPath,strlen(dllPath)+1,NULL) == 0) {
		VirtualFreeEx(hProcess,remoteMemory,strlen(dllPath)+1,MEM_RELEASE);
		return false;
	}

	// Create a remote thread. DllMain will be called.
	DWORD threadId;
#if 1
	HANDLE hThread = CreateRemoteThread(hProcess,NULL,0,(LPTHREAD_START_ROUTINE)&LoadLibraryA,remoteMemory,0,&threadId);
#else
	// This one is hardcore, works on Services and such.
	// Also only works on Vista+ so lets not use it if unneccesary.
	HANDLE hThread = NULL;
	RtlCreateUserThread(hProcess,NULL,FALSE,0,NULL,NULL,(LPTHREAD_START_ROUTINE)&LoadLibraryA,remoteMemory,&hThread,NULL);
#endif

	if(NULL == hThread) {
		VirtualFreeEx(hProcess,remoteMemory,strlen(dllPath)+1,MEM_RELEASE);
		return false;
	}

	// Wait for it to terminate. This is when we exit DllMain.
	WaitForSingleObject(hThread,INFINITE);
	DWORD exitCode;
	if(GetExitCodeThread(hThread,&exitCode) == 0) {
		exitCode = 0;
	}
	CloseHandle(hThread);

	// Free the memory.
	VirtualFreeEx(hProcess,remoteMemory,strlen(dllPath)+1,MEM_RELEASE);
	return exitCode != NULL;
}

bool RemoteLoadDll(DWORD processId,const char *dllPath)
{
	HANDLE hProcess = OpenProcess( PROCESS_CREATE_THREAD | PROCESS_QUERY_LIMITED_INFORMATION | PROCESS_VM_OPERATION | PROCESS_VM_WRITE , FALSE, (DWORD)processId );
	if(hProcess == NULL) {
		return false;
	}
	bool ret = RemoteLoadDll(hProcess,dllPath);
	CloseHandle(hProcess);
	return ret;
}

// Undocumented NT functions and structs. ============================================================================================================================
// - NtQueryObject
typedef struct _OBJECT_NAME_INFORMATION {
	UNICODE_STRING          Name;
	WCHAR                   NameBuffer[1];
} OBJECT_NAME_INFORMATION, *POBJECT_NAME_INFORMATION;
#if 0
typedef enum _OBJECT_INFORMATION_CLASS {
	ObjectBasicInformation,
	ObjectNameInformation,
	ObjectTypeInformation,
	ObjectAllInformation,
	ObjectDataInformation
} OBJECT_INFORMATION_CLASS, *POBJECT_INFORMATION_CLASS;
#endif
typedef NTSTATUS WINAPI NtQueryObject_t(
										__in_opt   HANDLE Handle,
										__in       OBJECT_INFORMATION_CLASS ObjectInformationClass,
										__out_opt  PVOID ObjectInformation,
										__in       ULONG ObjectInformationLength,
										__out_opt  PULONG ReturnLength
										);
NtQueryObject_t *NtQueryObjectFunc = (NtQueryObject_t *) GetProcAddress(GetModuleHandleW(L"ntdll"), "NtQueryObject");

map<string,char> device2drive;

wchar_t *PathFromHandle(HANDLE h,void *buf,size_t bufSize)
{
	// Special check for special named pipe that would hang NtQueryObject
	// http://forum.sysinternals.com/topic14546_page1.html
	if(h == (HANDLE)0x0012019F)
		return NULL;

	OBJECT_NAME_INFORMATION *info = (OBJECT_NAME_INFORMATION *)buf;

	if(NtQueryObjectFunc(h,/*ObjectNameInformation*/(OBJECT_INFORMATION_CLASS)1,info,bufSize, NULL) == 0) {
		info->Name.Buffer[info->Name.Length] = 0;
		char device[1024*10];
		int slash = 3;
		int offset;
		for(offset=0;offset<info->Name.Length;offset++) {
			device[offset] = char(info->Name.Buffer[offset]);
			device[offset+1] = 0;
			slash -= device[offset] == '\\';
			if(slash == 0)
				break;
		}
		char drive = device2drive[device];
		if(drive == 0)
			return NULL;
		PWSTR fileName = &info->Name.Buffer[offset-2];
		fileName[0] = drive;
		fileName[1] = ':';
		return fileName;
	}
	return NULL;
}

static bool InitUtil()
{
	char drives[1024*10];
	GetLogicalDriveStrings(sizeof(drives),drives);
	char *now = drives;
	while(now[0] != 0) {
		if(strcmp(now+1,":\\") == 0) {
			char dosDev[3] = "X:";
			dosDev[0] = now[0];
			char device[256];
			QueryDosDevice(dosDev,device,sizeof(device)-1);
			strcat(device,"\\");
			device2drive[device] = dosDev[0];
		}
		now += strlen(now)+1;
	}
	return true;
}

static bool initUtil = InitUtil();
