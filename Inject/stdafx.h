// stdafx.h : include file for standard system include files,
// or project specific include files that are used frequently, but
// are changed infrequently
//

#pragma once

#include "targetver.h"
#define WIN32_LEAN_AND_MEAN    1          // Exclude rarely-used stuff from Windows headers
#define _CRT_NONSTDC_NO_WARNINGS 1
#define _CRT_SECURE_NO_WARNINGS 1
#include <sys/types.h> 
#include <sys/stat.h>
// Windows Header Files:
#include <windows.h>
#include <Tlhelp32.h>
#include <Winternl.h>
#include <WinIoCtl.h>

#pragma warning(disable:4005)
#include <ntstatus.h>
#pragma warning(default:4005)
#include <string>
#include <map>
#include <list>
using namespace std;

bool Filter(const wchar_t *fileName,DWORD attrib);
bool Filter(const char *fileName,DWORD attrib);

bool FilterExceptions(const wchar_t *fileName,DWORD attrib);

// TODO: reference additional headers your program requires here
