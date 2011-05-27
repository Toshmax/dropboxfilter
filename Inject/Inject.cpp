// Inject.cpp : Defines the exported functions for the DLL application.
//

#include "stdafx.h"
#include "Hook.h"
#include "glob.h"
#include "../Util.h"
#include <string>
#include <deque>
#include "../SQLite/sqlite3.h"
using namespace std;

deque<wstring> filterExpr;
AutoFreeHandle filterExprMutex = CreateMutex(NULL,FALSE,NULL);

wchar_t dropboxPath[MAX_PATH];

void GetDropboxPath()
{
	dropboxPath[0] = 0;
	char dropboxDbPath[MAX_PATH];
	sprintf(dropboxDbPath,"%s\\Dropbox\\config.db",getenv("APPDATA"));

	sqlite3 *db=NULL;
	sqlite3_open(dropboxDbPath,&db);
	if(db) {
		sqlite3_stmt *stmt;
		if(sqlite3_prepare(db,"select value from config where key is 'dropbox_path';",-1,&stmt,NULL) == SQLITE_OK) {
			if(sqlite3_step(stmt) ==  SQLITE_ROW) {
				const char *path = (char *)sqlite3_column_blob(stmt,0);
				mbstowcs(dropboxPath,path,strlen(path)+1);
			}
			sqlite3_finalize(stmt);
		}
		sqlite3_close(db);
	}
}

__int64 settingsMtime;

void ReadSettings()
{
	static int inside=false;
	if(inside)
		return;
	inside = true;
	if(dropboxPath[0] == 0) {
		GetDropboxPath();
	}

	wchar_t configPath[MAX_PATH];
	wsprintfW(configPath,L"%s\\DropboxFilter.cfg",dropboxPath);

	struct _stat64 stat;
	if(_wstat64(configPath,&stat) == -1) {
		inside = false;
		return ;
	}

	if(settingsMtime == stat.st_mtime) {
		inside = false;
		return;
	}
	settingsMtime = stat.st_mtime;

	FILE *file = _wfopen(configPath,L"r");
	if(file) {
		WaitForSingleObject(filterExprMutex,INFINITE);
		filterExpr.clear();
		ReleaseMutex(filterExprMutex);

		wchar_t line[1024*10];
		while(fgetws(line,sizeof(line),file)) {
			if(line[0] == L'#')
				continue;
			int in=0,out=0;
			while(line[in] != 0) {
				wchar_t c = line[in++];
				if(c == '\n')
					continue;
				line[out++] = c;
			}
			line[out++] = 0;
			if(line[0] == 0)
				continue;
			WaitForSingleObject(filterExprMutex,INFINITE);
			filterExpr.push_back(line);
			ReleaseMutex(filterExprMutex);
		}
		fclose(file);
	} else {
	}
	inside = false;
}

bool endsWith(const char *str,const char *comp)
{
	str = str+strlen(str)-strlen(comp);
	return stricmp(str,comp) == 0;
}

bool endsWith(const wchar_t *str,const wchar_t *comp)
{
	str = str+wcslen(str)-wcslen(comp);
	return wcsicmp(str,comp) == 0;
}


bool beginsWith(const wchar_t *str,const wchar_t *comp)
{
	return wcsnicmp(str,comp,wcslen(comp)) == 0;
}

bool Filter(const wchar_t *fileName)
{
	if(endsWith(fileName,L"\\DropboxFilter.cfg")) {
		ReadSettings();
		return false;
	}
	if(!beginsWith(fileName,dropboxPath)) {
		return false;
	}
	fileName += wcslen(dropboxPath);

	WaitForSingleObject(filterExprMutex,INFINITE);
	for(unsigned i=0;i<filterExpr.size();i++) {
		if(WildcardMatch(fileName,filterExpr[i].c_str())) {
			ReleaseMutex(filterExprMutex);
			return true;
		}
	}
	ReleaseMutex(filterExprMutex);
	return false;
}

bool Init()
{
	char name[MAX_PATH]="";
	GetModuleFileNameA(NULL,name,sizeof(name));
	if(!endsWith(name,"\\dropbox.exe"))
		return false;

	ReadSettings();
	Hook();
	return true;
}

void  Detatch()
{
	UnHook();
}

BOOL APIENTRY DllMain( HMODULE hModule,
					  DWORD  ul_reason_for_call,
					  LPVOID lpReserved
					  )
{
	switch (ul_reason_for_call)
	{
	case DLL_PROCESS_ATTACH:
		if(!Init()) {
			return FALSE;
		}
		break;
	case DLL_PROCESS_DETACH:
		Detatch();
		break;
	case DLL_THREAD_ATTACH:
	case DLL_THREAD_DETACH:
		break;
	}
	return TRUE;
}

