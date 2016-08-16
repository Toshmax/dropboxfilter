// Inject.cpp : Defines the exported functions for the DLL application.
//

#include "stdafx.h"
#include "Hook.h"
#include "glob.h"
#include "../Util.h"
#include <string>
#include <deque>
#include "../SQLite/sqlite3.h"
#include "../base64.h"
#include <iostream>
#include <fstream>
using namespace std;

struct FilterExpr
{
	wstring	expr;
	DWORD attrib_eor;
	DWORD attrib_mask;
};

deque<FilterExpr> filterExpr;
AutoFreeHandle filterExprMutex = CreateMutex(NULL,FALSE,NULL);

wchar_t dropboxPath[MAX_PATH];

wofstream log_file;

#ifdef _DEBUG

void log(const string& message)
{
	wclog << "DropboxFilter: " << message.c_str() << endl;
}

void log(const wstring& message)
{
	wclog << "DropboxFilter: " << message << endl;
}

#else

// do nothing for a release build
void log(...) {}

#endif

void GetDropboxPath()
{
	dropboxPath[0] = 0;
	log("Looking for Dropbox path...");

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
				mbstowcs(dropboxPath,path,strlen(path)+1);
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
			mbstowcs(dropboxPath,path.c_str(),path.size()+1);
			fclose(file);
		}

	}

	log("Assuming Dropbox path is:");
	log(dropboxPath);
}

__int64 settingsMtime;

void ReadSettings()
{
	log("Reading settings...");

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
		wchar_t statement[1024*10];
		while(fgetws(line,sizeof(line),file)) {
			int in=0;
next:
			while(line[in] == ' ' || line[in] == '\t') {
				in++;
			}
			int out=0;
			if(line[in] == L'#')
				continue;
			wchar_t c = 0;
			while(line[in] != 0) {
				c = line[in++];
				if(c == '\n' || c == '\r')
					continue;
				if(c == '#') {
					in--;
					break;
				}
				if(c == '<' || c == ',' || c == ';' ) {
					break;
				}
				statement[out++] = c;
			}
			while(out && (statement[out-1] == ' ' || statement[out-1] == '\t')) {
				out++;
			}
			statement[out++] = 0;
			if(statement[0] == 0)
				continue;

			FilterExpr expr;
			expr.expr = statement;
			expr.attrib_eor = 0;
			expr.attrib_mask = 0;

			if(c == '<') {
				while(line[in] != 0 && line[in] != '>') {
					c = line[in];
					bool set= true;
					if(c == '+') {
						set = true;
						in++;
					} else if(c == '-') {
						set = false;
						in++;
					}
					c = line[in++];
					switch(c) {
					case 'h':
					case 'H':
						expr.attrib_mask |= FILE_ATTRIBUTE_HIDDEN;
						expr.attrib_eor |= FILE_ATTRIBUTE_HIDDEN * set;
						break;
					case 's':
					case 'S':
						expr.attrib_mask |= FILE_ATTRIBUTE_SYSTEM;
						expr.attrib_eor |= FILE_ATTRIBUTE_SYSTEM * set;
						break;
					case 'a':
					case 'A':
						expr.attrib_mask |= FILE_ATTRIBUTE_ARCHIVE;
						expr.attrib_eor |= FILE_ATTRIBUTE_ARCHIVE * set;
						break;
					case 'c':
					case 'C':
						expr.attrib_mask |= FILE_ATTRIBUTE_COMPRESSED;
						expr.attrib_eor |= FILE_ATTRIBUTE_COMPRESSED * set;
						break;
					case 'd':
					case 'D':
						expr.attrib_mask |= FILE_ATTRIBUTE_DIRECTORY;
						expr.attrib_eor |= FILE_ATTRIBUTE_DIRECTORY * set;
						break;
					case 'r':
					case 'R':
						expr.attrib_mask |= FILE_ATTRIBUTE_READONLY;
						expr.attrib_eor |= FILE_ATTRIBUTE_READONLY * set;
						break;
					case 'j':
					case 'J':
						expr.attrib_mask |= FILE_ATTRIBUTE_REPARSE_POINT;
						expr.attrib_eor |= FILE_ATTRIBUTE_REPARSE_POINT * set;
						break;
					}
				}
				if(line[in] == '>') {
					in++;
				}
			}
			WaitForSingleObject(filterExprMutex,INFINITE);
			filterExpr.push_back(expr);
			ReleaseMutex(filterExprMutex);
			goto next;
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

bool Filter(const wchar_t *fileName,DWORD attrib)
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
		if((attrib ^ filterExpr[i].attrib_eor) & filterExpr[i].attrib_mask)
			continue;
		if(WildcardMatch(fileName,filterExpr[i].expr.c_str())) {
			ReleaseMutex(filterExprMutex);
			return true;
		}
	}

	log(wstring(fileName) + L": not filtered");

	ReleaseMutex(filterExprMutex);
	return false;
}

bool hooked = false;

bool Init()
{
	// check if this really is the Dropbox process
	char name[MAX_PATH]="";
	GetModuleFileNameA(NULL,name,sizeof(name));
	if(!endsWith(name,"\\dropbox.exe"))
		return false;

#ifdef _DEBUG
	// open the log file for wclog
	log_file.open("C:\\dbfilter.log", ios::app);
	wclog.rdbuf(log_file.rdbuf());
#endif

	log("Init()");

	ReadSettings();

	log("Calling Hook()");
	Hook();
	hooked = true;

	return true;
}

void  Detatch()
{
	log("Detach()");
	// unhook only if it was really done
	if (hooked) {
		UnHook();
	}
}

BOOL APIENTRY DllMain( HMODULE hModule,
					  DWORD  ul_reason_for_call,
					  LPVOID lpReserved
					  )
{
	switch (ul_reason_for_call)
	{
	case DLL_PROCESS_ATTACH:
		if (!Init()) {
			log("Error in Init)");
			return FALSE;
		}
		break;
	case DLL_PROCESS_DETACH:
		Detatch();
		log("Finished");
		break;
	case DLL_THREAD_ATTACH:
	case DLL_THREAD_DETACH:
		break;
	}
	return TRUE;
}

