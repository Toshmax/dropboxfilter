// stdafx.h : include file for standard system include files,
// or project specific include files that are used frequently, but
// are changed infrequently
//

#pragma once

#include "targetver.h"

#define WIN32_LEAN_AND_MEAN             // Exclude rarely-used stuff from Windows headers
#define _CRT_NONSTDC_NO_WARNINGS
#define _CRT_SECURE_NO_WARNINGS
// Windows Header Files:
#include <windows.h>
#include <Tlhelp32.h>
#include <Winternl.h>
#include <WinIoCtl.h>

#pragma warning(disable:4005)
#include <ntstatus.h>
#pragma warning(default:4005)

// C RunTime Header Files
#include <stdlib.h>
#include <malloc.h>
#include <memory.h>
#include <tchar.h>
#include <map>
#include <set>
#include <string>
#include <vector>
#include <list>
using namespace std;
// TODO: reference additional headers your program requires here
