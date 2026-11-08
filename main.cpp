// main.cpp - program entry point.
#include <exception>

#include "Win32Common.h"
#include "MainWindow.h"

// Enable Visual Styles: ask Windows to load Common Controls v6 for this program.
#pragma comment(linker, "\"/manifestdependency:type='win32' name='Microsoft.Windows.Common-Controls' version='6.0.0.0' processorArchitecture='*' publicKeyToken='6595b64144ccf1df' language='*'\"")

int WINAPI wWinMain(_In_ HINSTANCE instance, _In_opt_ HINSTANCE, _In_ PWSTR, _In_ int showCommand)
{
    // Per-Monitor V2 DPI awareness: must be set before any window is created.
    static_cast<void>(SetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2));

    INITCOMMONCONTROLSEX controls{};
    controls.dwSize = sizeof(controls);
    controls.dwICC = ICC_WIN95_CLASSES | ICC_STANDARD_CLASSES;
    InitCommonControlsEx(&controls);

    try {
        MainWindow window(instance);
        if (!window.Create(showCommand)) {
            MessageBoxW(nullptr, L"The main window could not be created.", L"Course Enrollment Management System",
                        MB_OK | MB_ICONERROR);
            return 1;
        }
        return window.Run();
    } catch (const std::exception&) {
        MessageBoxW(nullptr, L"An unexpected error occurred (out of memory?). The program will close.",
                    L"Course Enrollment Management System", MB_OK | MB_ICONERROR);
        return 2;
    }
}
