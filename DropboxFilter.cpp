// DropBoxExtender.cpp : Defines the entry point for the application.
//

#include "stdafx.h"
#include "DropBoxFilter.h"
#include "Util.h"
#include "IPC.h"
#include <Shobjidl.h>
#include <initguid.h>
#include <shlguid.h>
#include <Shlobj.h>
#include "SQLite/sqlite3.h"
#include <stdio.h>
#include <process.h>
#include <Shellapi.h>
#include "base64.h"
#define MAX_LOADSTRING 100

char dropboxPath[MAX_PATH];

void GetDropboxPath()
{
	dropboxPath[0] = 0;
	char dropboxDbPath[MAX_PATH];
	sprintf(dropboxDbPath,"%s\\Dropbox\\config.db",getenv("APPDATA"));

	bool pathFound = false;

	sqlite3 *db=NULL;
	sqlite3_open(dropboxDbPath,&db);
	if(db) {
		sqlite3_stmt *stmt;
		if(sqlite3_prepare(db,"select value from config where key is 'dropbox_path';",-1,&stmt,NULL) == SQLITE_OK) {
			if(sqlite3_step(stmt) ==  SQLITE_ROW) {
				const char *path = (char *)sqlite3_column_blob(stmt,0);
				strcpy(dropboxPath,path);
				pathFound = true;
			}
			sqlite3_finalize(stmt);
		}
		sqlite3_close(db);
	}
	
	if(!pathFound) {
		sprintf(dropboxDbPath,"%s\\Dropbox\\host.db",getenv("APPDATA"));
		FILE *file = fopen(dropboxDbPath,"r");
		if(file) {
			char line[1024*10]="";
			fgets(line,sizeof(line),file);
			fgets(line,sizeof(line),file);
			string path = base64_decode(line);
			strcpy(dropboxPath,path.c_str());
			fclose(file);
		}

	}
}

HRESULT CreateLink(LPCSTR lpszPathLink, LPCSTR lpszPathObj, LPCSTR lpszDesc, LPCSTR lpszArgs) 
{ 
	HRESULT hres; 
	IShellLink* psl; 
 
	// Get a pointer to the IShellLink interface. It is assumed that CoInitialize
	// has already been called.
	hres = CoCreateInstance(CLSID_ShellLink, NULL, CLSCTX_INPROC_SERVER, IID_IShellLink, (LPVOID*)&psl); 
	if (SUCCEEDED(hres)) 
	{ 
		IPersistFile* ppf; 
 
		// Set the path to the shortcut target and add the description. 
		psl->SetPath(lpszPathObj); 
		if(lpszArgs) {
			psl->SetArguments(lpszArgs);
		}
		psl->SetDescription(lpszDesc); 
 
		// Query IShellLink for the IPersistFile interface, used for saving the 
		// shortcut in persistent storage. 
		hres = psl->QueryInterface(IID_IPersistFile, (LPVOID*)&ppf); 
 
		if (SUCCEEDED(hres)) 
		{ 
			WCHAR wsz[MAX_PATH]; 
 
			// Ensure that the string is Unicode. 
			MultiByteToWideChar(CP_ACP, 0, lpszPathLink, -1, wsz, MAX_PATH); 
			
			// Add code here to check return value from MultiByteWideChar 
			// for success.
 
			// Save the link by calling IPersistFile::Save. 
			hres = ppf->Save(wsz, TRUE); 
			ppf->Release(); 
		} 
		psl->Release(); 
	} 
	return hres; 
}


bool CompareFiles(const char *a,const char *b)
{
	FILE *file_a,*file_b;
	file_a = fopen(a,"rb");
	file_b = fopen(b,"rb");
	if(file_a == NULL || file_b == NULL) {
		if(file_a)
			fclose(file_a);
		if(file_b)
			fclose(file_b);
		return false;
	}
	char buffer_a[1024*64];
	char buffer_b[1024*64];
	int read_a;
	while((read_a = fread(buffer_a,1,sizeof(buffer_a),file_a))) {
		int read_b = fread(buffer_b,1,sizeof(buffer_b),file_b);
		if(read_a != read_b || memcmp(buffer_a,buffer_b,read_a) != 0) {
			fclose(file_a);
			fclose(file_b);
			return false;
		}
	}
	if(!feof(file_b)) {
		fclose(file_a);
		fclose(file_b);
		return false;
	}
	fclose(file_a);
	fclose(file_b);
	return true;
}

bool UnInstallAppInit()
{
	char installPath[MAX_PATH];
	sprintf(installPath,"%s\\DropboxFilter",getenv("ProgramFiles"));
	char dllPath[MAX_PATH];
	sprintf(dllPath,"%s\\DropboxFilter.dll",installPath);
	char shortDllPath[MAX_PATH];
	GetShortPathNameA(dllPath,shortDllPath,sizeof(shortDllPath));

	char buffer[1024*10];
	DWORD len=sizeof(buffer);
	DWORD type=REG_SZ;

	HKEY key;
	bool ok=true;
	ok &= ERROR_SUCCESS == RegOpenKeyEx(HKEY_LOCAL_MACHINE, "Software\\Microsoft\\Windows NT\\CurrentVersion\\Windows", 0, KEY_READ|KEY_WRITE, &key);
	ok &= ERROR_SUCCESS == RegQueryValueExA(key,"AppInit_DLLs",NULL,&type,(BYTE*)buffer,&len);
	char *offset;
	if((offset = strstr(buffer,shortDllPath)) != NULL) {
		if(offset != buffer && offset[-1] == ',') {
			memmove(buffer-1,buffer+strlen(shortDllPath),strlen(buffer+strlen(shortDllPath))+1);
		} else if(offset[strlen(shortDllPath)] == ',') {
			memmove(buffer,buffer+strlen(shortDllPath)+1,strlen(buffer+strlen(shortDllPath)+1)+1);
		} else {
			memmove(buffer,buffer+strlen(shortDllPath),strlen(buffer+strlen(shortDllPath))+1);
		}
		ok &= ERROR_SUCCESS == RegSetValueExA(key,"AppInit_DLLs",NULL,REG_SZ,(BYTE*)buffer,strlen(buffer)+1);
		
		DWORD enable = buffer[0] != 0;
		ok &= ERROR_SUCCESS == RegSetValueExA(key,"LoadAppInit_DLLs",NULL,REG_DWORD,(BYTE*)&enable,sizeof(enable));
	}
	RegCloseKey(key);
	return true;
}

bool UnInstallPiggyback()
{
	char startupPath[MAX_PATH];
	SHGetSpecialFolderPath(NULL,startupPath,CSIDL_STARTUP,TRUE);
	char startLnkPath[MAX_PATH];
	sprintf(startLnkPath,"%s\\Dropbox.lnk",startupPath);
	char exePath[MAX_PATH];
	sprintf(exePath,"%s\\Dropbox\\bin\\Dropbox.exe",getenv("APPDATA"));

	DeleteFile(startLnkPath);
	CreateLink(startLnkPath,exePath,"Dropbox",NULL);
	return true;
}

bool InstallAppInit()
{
	UnInstallPiggyback();
	char installPath[MAX_PATH];
	sprintf(installPath,"%s\\DropboxFilter",getenv("ProgramFiles"));
	char dllPath[MAX_PATH];
	sprintf(dllPath,"%s\\DropboxFilter.dll",installPath);
	char shortDllPath[MAX_PATH];
	GetShortPathNameA(dllPath,shortDllPath,sizeof(shortDllPath));

	char buffer[1024*10];
	DWORD len=sizeof(buffer);
	DWORD type=REG_SZ;

	HKEY key;
	bool ok=true;
	ok &= ERROR_SUCCESS == RegOpenKeyEx(HKEY_LOCAL_MACHINE, "Software\\Microsoft\\Windows NT\\CurrentVersion\\Windows", 0, KEY_READ|KEY_WRITE, &key);
	ok &= ERROR_SUCCESS == RegQueryValueExA(key,"AppInit_DLLs",NULL,&type,(BYTE*)buffer,&len);
	if(strstr(buffer,shortDllPath) == NULL) {
		if(buffer[0] != 0) {
			strcat(buffer,",");
		}
		strcat(buffer,shortDllPath);
		ok &= ERROR_SUCCESS == RegSetValueExA(key,"AppInit_DLLs",NULL,REG_SZ,(BYTE*)buffer,strlen(buffer)+1);
		DWORD enable = 1;
		ok &= ERROR_SUCCESS == RegSetValueExA(key,"LoadAppInit_DLLs",NULL,REG_DWORD,(BYTE*)&enable,sizeof(enable));
	}
	RegCloseKey(key);
	if(!ok) {
		MessageBoxA(NULL,"Failed to set registry values.\nYou migh have to run this installer with Administrator privilegies\n\nExit installer right click on installer executable and choose run as administrator.\n","Error",MB_OK);
		return false;
	}
	return true;
}

bool InstallPiggyback()
{
	UnInstallAppInit();
	char startupPath[MAX_PATH];
	SHGetSpecialFolderPath(NULL,startupPath,CSIDL_STARTUP,TRUE);
	char startLnkPath[MAX_PATH];
	sprintf(startLnkPath,"%s\\Dropbox.lnk",startupPath);
	char installPath[MAX_PATH];
	sprintf(installPath,"%s\\DropboxFilter",getenv("ProgramFiles"));
	char exePath[MAX_PATH];
	sprintf(exePath,"%s\\DropboxFilter.exe",installPath);

	DeleteFile(startLnkPath);
	CreateLink(startLnkPath,exePath,"DropboxFilter","--launchDropbox");
	return true;
}

bool Install()
{
	char installPath[MAX_PATH];
	sprintf(installPath,"%s\\DropboxFilter",getenv("ProgramFiles"));
	char exePath[MAX_PATH];
	sprintf(exePath,"%s\\DropboxFilter.exe",installPath);
	char dllPath[MAX_PATH];
	sprintf(dllPath,"%s\\DropboxFilter.dll",installPath);


	char currExePath[MAX_PATH];
	GetModuleFileNameA(NULL,currExePath,sizeof(currExePath));

	if(CompareFiles(exePath,currExePath)) {
		return true;
	}
	char msg[1024*10];
	sprintf(msg,"First we need to install the DropboxFilter executable and DLL into a program files directory.\n\nWhen you press OK the following will be done.\n\n1. The dropbox process will be terminated\n2. A directory %s will be created.\n3. Executable will be copied to\n\t%s\n4. Dll will be copied to\n\t%s",installPath,exePath,dllPath);
	if(MessageBox(NULL,msg,"Install",MB_OKCANCEL) != IDOK) {
		return false;
	}
	DWORD id = FindProcessIdByName("dropbox.exe");
	if(id) {
		HANDLE hProcess = OpenProcess(PROCESS_TERMINATE,FALSE,id);
		if(hProcess != NULL) {
			TerminateProcess(hProcess,0);
			CloseHandle(hProcess);
		}
	}
	CreateDirectory(installPath,NULL);
	if(!CopyFileA(currExePath,exePath,FALSE)) {
		MessageBox(NULL,"Failed to install executable to Program Files directory.","Failed",MB_OK);
		return false;
	}

	FILE *file = fopen(dllPath,"wb");
	int written;
	HRSRC hResource;
	if(file) {
		hResource = FindResource(NULL,"ID_DLL","DLL");
		void *resource = LockResource(LoadResource(NULL,hResource));
		written = fwrite(resource,1,SizeofResource(NULL,hResource),file);
		fclose(file);
	}
	if(file == NULL || written != SizeofResource(NULL,hResource)) {
		MessageBox(NULL,"Failed to install dll to Program Files directory.\nYou migh have to run this installer with Administrator privilegies\n\nExit installer right click on installer executable and choose run as administrator.","Failed",MB_OK);
	}
	return true;
}

bool UnInstall()
{
	char installPath[MAX_PATH];
	sprintf(installPath,"%s\\DropboxFilter",getenv("ProgramFiles"));
	char exePath[MAX_PATH];
	sprintf(exePath,"%s\\DropboxFilter.exe",installPath);
	char dllPath[MAX_PATH];
	sprintf(dllPath,"%s\\DropboxFilter.dll",installPath);

	DWORD id = FindProcessIdByName("dropbox.exe");
	if(id) {
		HANDLE hProcess = OpenProcess(PROCESS_TERMINATE,FALSE,id);
		if(hProcess != NULL) {
			TerminateProcess(hProcess,0);
			CloseHandle(hProcess);
		}
	}

	DeleteFileA(exePath);
	DeleteFileA(dllPath);
	RemoveDirectoryA(installPath);
	return true;
}

void EditConfig()
{
	GetDropboxPath();
	if(dropboxPath[0] == 0) {
		MessageBox(NULL,"Cant find where your dropbox path is.\n\nIs dropbox installed?","Error",MB_OK);
		return;
	}
	char configPath[MAX_PATH];
	sprintf(configPath,"%s\\DropboxFilter.cfg",dropboxPath);
	FILE *file = fopen(configPath,"r");
	if(file == NULL) {
		if(MessageBoxA(NULL,"I cant find a config file DropboxFilter.cfg in your dropbox folder, do you wish to create a default one?","Default config file",MB_YESNO) == IDYES) {
			FILE *file = fopen(configPath,"wb");
			int written;
			HRSRC hResource;
			if(file) {
				hResource = FindResource(NULL,"ID_CONFIG","TXT");
				void *resource = LockResource(LoadResource(NULL,hResource));
				written = fwrite(resource,1,SizeofResource(NULL,hResource),file);
				fclose(file);
			}
			if(file == NULL || written != SizeofResource(NULL,hResource)) {
				MessageBox(NULL,"Failed to copy default config to dropbox folder.","Failed",MB_OK);
			}
		} else {
			return;
		}
	} else {
		fclose(file);
	}
	char notepadExe[MAX_PATH];
	sprintf(notepadExe,"%s\\notepad.exe",getenv("SystemRoot"));
	_spawnl(_P_DETACH,notepadExe,notepadExe,configPath,NULL);
}

void LaunchDropbox()
{
	char exePath[MAX_PATH];
	sprintf(exePath,"%s\\Dropbox\\bin\\Dropbox.exe",getenv("APPDATA"));
	char installPath[MAX_PATH];
	sprintf(installPath,"%s\\DropboxFilter",getenv("ProgramFiles"));
	char dllPath[MAX_PATH];
	sprintf(dllPath,"%s\\DropboxFilter.dll",installPath);

	STARTUPINFO si;
	::ZeroMemory(&si, sizeof(STARTUPINFO));

	PROCESS_INFORMATION pi;

	if(!CreateProcessA(exePath,"",NULL,NULL,FALSE,CREATE_SUSPENDED,NULL,NULL,&si,&pi)) {
		MessageBox(NULL,"Failed to launch dropbox","DropboxFilter",MB_OK);
		return;
	}
	if(!RemoteLoadDll(pi.hProcess,dllPath)) {
		MessageBox(NULL,"Failed to load dll into dropbox remotely","DropboxFilter",MB_OK);
		TerminateProcess(pi.hProcess,0);
		return;
	}
	ResumeThread(pi.hThread);
}

bool IsElevated()
{
	HKEY key=NULL;
	bool ok = ERROR_SUCCESS == RegOpenKeyEx(HKEY_LOCAL_MACHINE, "Software\\Microsoft\\Windows NT\\CurrentVersion\\Windows", 0, KEY_READ|KEY_WRITE, &key);
	RegCloseKey(key);
	return ok;
}

void Elevate(HWND hWnd,int cmd)
{
	if(IsElevated())
		return;
	char path[MAX_PATH];
	GetModuleFileNameA(NULL,path,sizeof(path));
	char param[16];
	sprintf(param,"%d",cmd);
	ShellExecute(hWnd, "runas", path, param, 0, SW_SHOWNORMAL);
	ExitProcess(0);
}

// Message handler for about box.
INT_PTR CALLBACK About(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam)
{
	UNREFERENCED_PARAMETER(lParam);
	switch (message)
	{
	case WM_INITDIALOG:
		{
			HWND hwndOwner = GetDesktopWindow(); 
			RECT rc, rcDlg, rcOwner; 

			GetWindowRect(hwndOwner, &rcOwner); 
			GetWindowRect(hDlg, &rcDlg); 
			CopyRect(&rc, &rcOwner); 

			SetWindowTextA(GetDlgItem(hDlg,IDC_INSTRUCTIONS),"DropboxFilter is a program that hooks into dropbox and adds filter capabilities.\n\nThere is two ways to install it.\n\n1. AppInit Dll's\n\tThis method is a bit intrusive, anal virus scanners might detect this as a threat. But, it will not disappear when Dropbox is automatically updated.\n\n2. Piggyback start up launching.\n\tThis method will replace the dropbox system start with its own. When dropbox is automatically updated it might remove this and thereby disabling DropboxFilter without you noticing.\n\nAppInit method is recommended.");
			if(!IsElevated()) {
				Button_SetElevationRequiredState(GetDlgItem(hDlg,IDC_INSTALL_APPINIT),TRUE);
				Button_SetElevationRequiredState(GetDlgItem(hDlg,IDC_INSTALL_PIGGYBACK),TRUE);
				Button_SetElevationRequiredState(GetDlgItem(hDlg,IDC_UNINSTALL),TRUE);
			}
			// Offset the owner and dialog box rectangles so that right and bottom 
			// values represent the width and height, and then offset the owner again 
			// to discard space taken up by the dialog box. 

			OffsetRect(&rcDlg, -rcDlg.left, -rcDlg.top); 
			OffsetRect(&rc, -rc.left, -rc.top); 
			OffsetRect(&rc, -rcDlg.right, -rcDlg.bottom); 

			// The new position is the sum of half the remaining space and the owner's 
			// original position. 

			SetWindowPos(hDlg, 
				HWND_TOP, 
				rcOwner.left + (rc.right / 2), 
				rcOwner.top + (rc.bottom / 2), 
				0, 0,          // Ignores size arguments. 
				SWP_NOSIZE);
			if(lParam) {
				if(IsElevated()) {
					PostMessage(hDlg,WM_COMMAND,lParam,0);
				} else {
					MessageBox(NULL,"Could not elevate process rights. Your user needs to have admin rights to install this program","Cant elevate",MB_OK);
				}
			}
		}
		return (INT_PTR)TRUE;

	case WM_COMMAND:
		switch(LOWORD(wParam)) {
		case ID_CLOSE:
		case IDCANCEL:
			EndDialog(hDlg, LOWORD(wParam));
			return (INT_PTR)TRUE;
		case IDC_INSTALL_APPINIT:
			Elevate(hDlg,IDC_INSTALL_APPINIT);
			if(!Install()) {
				return TRUE;
			}
			if(!InstallAppInit()) {
				return TRUE;
			}
			MessageBoxA(NULL,"Install done.\n\nStart dropbox manualy from the start menu.","Installed",MB_OK);
			return TRUE;
		case IDC_INSTALL_PIGGYBACK:
			Elevate(hDlg,IDC_INSTALL_PIGGYBACK);
			if(!Install()) {
				return TRUE;
			}
			if(!InstallPiggyback()) {
				return TRUE;
			}
			MessageBoxA(NULL,"Install done.\n\nStart dropbox manualy from the startup folder on the start menu.","Installed",MB_OK);
			return TRUE;
		case IDC_UNINSTALL:
			Elevate(hDlg,IDC_INSTALL_PIGGYBACK);
			UnInstall();
			UnInstallPiggyback();
			UnInstallAppInit();
			MessageBoxA(NULL,"UnInstall done.\n\nStart dropbox manualy from the start menu.","UnInstalled",MB_OK);
			return TRUE;
		case IDC_EDIT_CONFIG:
			EditConfig();
			return TRUE;
		}
		break;
	}
	return (INT_PTR)FALSE;
}

int APIENTRY _tWinMain(HINSTANCE hInstance,
					 HINSTANCE hPrevInstance,
					 LPTSTR    lpCmdLine,
					 int       nCmdShow)
{
	UNREFERENCED_PARAMETER(hPrevInstance);

	if(strcmp(lpCmdLine,"--launchDropbox")==0) {
		LaunchDropbox();
		return 0;
	}

	DialogBoxParamA(hInstance, MAKEINTRESOURCE(IDD_DROPBOX_FILTER), NULL, About,atoi(lpCmdLine));

	return 0;
}

