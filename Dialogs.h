// Dialogs.h - modal dialogs: Add Student, Add Course, About.
#pragma once

#include <string>

#include "Win32Common.h"
#include "EnrollmentSystem.h"

/// Shows the modal "Add Student" dialog. The dialog validates through
/// @p system.AddStudent and stays open (inline error + focus on the bad field)
/// until the input is accepted or the user cancels.
/// @return true if a student was added; @p message then holds the success text.
bool ShowAddStudentDialog(HWND parent, HINSTANCE instance, EnrollmentSystem& system, std::wstring& message);

/// Shows the modal "Add Course" dialog (ID, name, capacity with spin control).
/// @return true if a course was added; @p message then holds the success text.
bool ShowAddCourseDialog(HWND parent, HINSTANCE instance, EnrollmentSystem& system, std::wstring& message);

/// Shows the modal About box.
void ShowAboutDialog(HWND parent, HINSTANCE instance);
