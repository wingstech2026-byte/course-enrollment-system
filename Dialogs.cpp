// Dialogs.cpp - Add Student / Add Course / About dialogs.
#include "Dialogs.h"

#include "resource.h"

namespace {

/// State shared between the caller and a dialog procedure (passed via lParam).
struct DialogContext {
    EnrollmentSystem* system = nullptr;
    std::wstring message;  // success text, filled when the dialog closes with OK
};

DialogContext* GetContext(HWND dialog)
{
    return reinterpret_cast<DialogContext*>(GetWindowLongPtrW(dialog, DWLP_USER));
}

/// Reads the text of a dialog control.
std::wstring ReadText(HWND dialog, int controlId)
{
    HWND control = GetDlgItem(dialog, controlId);
    const int length = GetWindowTextLengthW(control);
    std::wstring text(static_cast<size_t>(length) + 1, L'\0');
    const int copied = GetWindowTextW(control, text.data(), length + 1);
    text.resize(static_cast<size_t>(copied));
    return text;
}

/// Centers the dialog over its owner window.
void CenterOnParent(HWND dialog)
{
    HWND parent = GetParent(dialog);
    RECT dialogRect{};
    RECT parentRect{};
    if (parent == nullptr || !GetWindowRect(dialog, &dialogRect) || !GetWindowRect(parent, &parentRect)) return;

    const int x = parentRect.left + ((parentRect.right - parentRect.left) - (dialogRect.right - dialogRect.left)) / 2;
    const int y = parentRect.top + ((parentRect.bottom - parentRect.top) - (dialogRect.bottom - dialogRect.top)) / 2;
    SetWindowPos(dialog, nullptr, x, y, 0, 0, SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE);
}

/// Maps the failing field to the dialog control that should receive focus.
int ControlForField(InputField field, int idControl, int nameControl, int capacityControl)
{
    switch (field) {
    case InputField::Name: return nameControl;
    case InputField::Capacity: return capacityControl;
    case InputField::Id:
    case InputField::None:
    default: return idControl;
    }
}

/// Shows a validation failure: an error message box, then the same text inline
/// in the dialog, with focus (and selection) on the offending field.
void ReportError(HWND dialog, const wchar_t* boxTitle, const std::wstring& text, int focusControl)
{
    MessageBoxW(dialog, text.c_str(), boxTitle, MB_OK | MB_ICONERROR);
    SetDlgItemTextW(dialog, IDC_DLG_ERROR, text.c_str());
    HWND field = GetDlgItem(dialog, focusControl);
    SetFocus(field);
    SendMessageW(field, EM_SETSEL, 0, -1);
}

/// WM_CTLCOLORSTATIC: draws the inline error label in red.
INT_PTR ColorErrorLabel(HWND dialog, HDC dc, HWND control)
{
    if (control != GetDlgItem(dialog, IDC_DLG_ERROR)) return FALSE;
    SetTextColor(dc, RGB(192, 0, 0));
    SetBkColor(dc, GetSysColor(COLOR_3DFACE));
    return reinterpret_cast<INT_PTR>(GetSysColorBrush(COLOR_3DFACE));
}

// ---------------------------------------------------------------------------
// Add Student
// ---------------------------------------------------------------------------

INT_PTR CALLBACK AddStudentProc(HWND dialog, UINT message, WPARAM wParam, LPARAM lParam)
{
    switch (message) {
    case WM_INITDIALOG:
        SetWindowLongPtrW(dialog, DWLP_USER, lParam);
        CenterOnParent(dialog);
        return TRUE;  // let the dialog manager focus the first control

    case WM_CTLCOLORSTATIC:
        return ColorErrorLabel(dialog, reinterpret_cast<HDC>(wParam), reinterpret_cast<HWND>(lParam));

    case WM_COMMAND:
        switch (LOWORD(wParam)) {
        case IDOK: {
            DialogContext* context = GetContext(dialog);
            if (context == nullptr) return TRUE;

            const OperationResult result =
                context->system->AddStudent(ReadText(dialog, IDC_STUDENT_ID), ReadText(dialog, IDC_STUDENT_NAME));
            if (!result) {
                ReportError(dialog, L"Cannot add student", result.message,
                            ControlForField(result.field, IDC_STUDENT_ID, IDC_STUDENT_NAME, IDC_STUDENT_NAME));
                return TRUE;  // keep the dialog open
            }
            context->message = result.message;
            EndDialog(dialog, IDOK);
            return TRUE;
        }
        case IDCANCEL:
            EndDialog(dialog, IDCANCEL);
            return TRUE;
        default:
            break;
        }
        break;

    default:
        break;
    }
    return FALSE;
}

// ---------------------------------------------------------------------------
// Add Course
// ---------------------------------------------------------------------------

INT_PTR CALLBACK AddCourseProc(HWND dialog, UINT message, WPARAM wParam, LPARAM lParam)
{
    switch (message) {
    case WM_INITDIALOG: {
        SetWindowLongPtrW(dialog, DWLP_USER, lParam);
        CenterOnParent(dialog);

        // Spin control: attach it to the capacity edit box and give it a sensible range.
        HWND spin = GetDlgItem(dialog, IDC_COURSE_CAPACITY_SPIN);
        SendMessageW(spin, UDM_SETBUDDY, reinterpret_cast<WPARAM>(GetDlgItem(dialog, IDC_COURSE_CAPACITY)), 0);
        SendMessageW(spin, UDM_SETRANGE32, 1, EnrollmentSystem::kMaxCapacity);
        SendMessageW(spin, UDM_SETPOS32, 0, 30);
        return TRUE;
    }

    case WM_CTLCOLORSTATIC:
        return ColorErrorLabel(dialog, reinterpret_cast<HDC>(wParam), reinterpret_cast<HWND>(lParam));

    case WM_COMMAND:
        switch (LOWORD(wParam)) {
        case IDOK: {
            DialogContext* context = GetContext(dialog);
            if (context == nullptr) return TRUE;

            const OperationResult result = context->system->AddCourse(
                ReadText(dialog, IDC_COURSE_ID), ReadText(dialog, IDC_COURSE_NAME),
                ReadText(dialog, IDC_COURSE_CAPACITY));
            if (!result) {
                ReportError(dialog, L"Cannot add course", result.message,
                            ControlForField(result.field, IDC_COURSE_ID, IDC_COURSE_NAME, IDC_COURSE_CAPACITY));
                return TRUE;  // keep the dialog open
            }
            context->message = result.message;
            EndDialog(dialog, IDOK);
            return TRUE;
        }
        case IDCANCEL:
            EndDialog(dialog, IDCANCEL);
            return TRUE;
        default:
            break;
        }
        break;

    default:
        break;
    }
    return FALSE;
}

// ---------------------------------------------------------------------------
// About
// ---------------------------------------------------------------------------

INT_PTR CALLBACK AboutProc(HWND dialog, UINT message, WPARAM wParam, LPARAM)
{
    switch (message) {
    case WM_INITDIALOG:
        CenterOnParent(dialog);
        return TRUE;

    case WM_COMMAND:
        if (LOWORD(wParam) == IDOK || LOWORD(wParam) == IDCANCEL) {
            EndDialog(dialog, IDOK);
            return TRUE;
        }
        break;

    default:
        break;
    }
    return FALSE;
}

/// Runs a modal dialog and reports whether it was closed with OK.
bool RunDataDialog(HWND parent, HINSTANCE instance, int templateId, DLGPROC procedure,
                   EnrollmentSystem& system, std::wstring& message)
{
    DialogContext context;
    context.system = &system;

    const INT_PTR result = DialogBoxParamW(instance, MAKEINTRESOURCEW(templateId), parent, procedure,
                                           reinterpret_cast<LPARAM>(&context));
    if (result != IDOK) return false;
    message = context.message;
    return true;
}

}  // namespace

bool ShowAddStudentDialog(HWND parent, HINSTANCE instance, EnrollmentSystem& system, std::wstring& message)
{
    return RunDataDialog(parent, instance, IDD_ADD_STUDENT, AddStudentProc, system, message);
}

bool ShowAddCourseDialog(HWND parent, HINSTANCE instance, EnrollmentSystem& system, std::wstring& message)
{
    return RunDataDialog(parent, instance, IDD_ADD_COURSE, AddCourseProc, system, message);
}

void ShowAboutDialog(HWND parent, HINSTANCE instance)
{
    DialogBoxParamW(instance, MAKEINTRESOURCEW(IDD_ABOUT), parent, AboutProc, 0);
}
