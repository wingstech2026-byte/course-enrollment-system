// Win32Common.h - one place for the Windows headers, build macros and libraries.
#pragma once

#ifndef NOMINMAX
#define NOMINMAX  // keep std::min / std::max usable
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef UNICODE
#define UNICODE
#endif
#ifndef _UNICODE
#define _UNICODE
#endif

#include <windows.h>
#include <commctrl.h>  // list view, tab, status bar, progress bar, up-down ...
#include <uxtheme.h>   // SetWindowTheme

#pragma comment(lib, "comctl32.lib")
#pragma comment(lib, "uxtheme.lib")
