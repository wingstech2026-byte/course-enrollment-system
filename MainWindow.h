// MainWindow.h - the application's main window (tab control, lists, status bar).
#pragma once

#include <array>
#include <memory>
#include <string>
#include <type_traits>
#include <vector>

#include "Win32Common.h"
#include "EnrollmentSystem.h"
#include "FileManager.h"

/// Describes one list-view column. Widths are given at 96 DPI and scaled at run time.
struct ColumnDef {
    std::wstring title;
    int width96;
    bool flexible;  // true: this column absorbs the remaining width of the list
};

/// One row of a list view (cells[0] is the row's key, used to keep the selection).
struct ListRow {
    std::vector<std::wstring> cells;
    int tag = 0;  // free-form: for the courses list 0 = open, 1 = almost full, 2 = full
};

/// Data behind a virtual (LVS_OWNERDATA) list view, including its sort state.
struct ListModel {
    std::vector<ColumnDef> columns;
    std::vector<ListRow> rows;
    int sortColumn = 0;
    bool ascending = true;
};

/// RAII owner of a GDI font.
struct FontDeleter {
    void operator()(HFONT font) const noexcept { DeleteObject(font); }
};
using FontHandle = std::unique_ptr<std::remove_pointer_t<HFONT>, FontDeleter>;

class MainWindow {
public:
    explicit MainWindow(HINSTANCE instance);

    MainWindow(const MainWindow&) = delete;
    MainWindow& operator=(const MainWindow&) = delete;

    /// Registers the window classes, creates and shows the window, loads the data files.
    bool Create(int showCommand);

    /// Runs the message loop until the window is closed. Returns the exit code.
    int Run();

private:
    // ---- Window procedures ----------------------------------------------
    static LRESULT CALLBACK WindowProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam);
    static LRESULT CALLBACK PageProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam);
    static BOOL CALLBACK SetFontProc(HWND child, LPARAM font);
    LRESULT HandleMessage(UINT message, WPARAM wParam, LPARAM lParam);

    // ---- Creation ---------------------------------------------------------
    bool RegisterClasses();
    bool CreateControls();
    HWND CreatePage(int id, bool visible, bool clipChildren);
    HWND CreateChild(HWND parent, const wchar_t* className, const wchar_t* text, DWORD style, int id,
                     DWORD exStyle = 0);
    HWND CreateList(HWND parent, int id, const ListModel& model);
    bool CreateStudentsPage();
    bool CreateCoursesPage();
    bool CreateEnrollmentPage();
    void UpdateFont();

    // ---- Layout -----------------------------------------------------------
    int Scale(int value96) const;
    void Layout();
    void LayoutStudentsPage(int width, int height);
    void LayoutCoursesPage(int width, int height);
    void LayoutEnrollmentPage(int width, int height);
    void UpdateStatusParts();
    void FitColumns(HWND list, const ListModel& model) const;
    void ShowPage(int index);

    // ---- Messages ---------------------------------------------------------
    void OnCommand(int id, int code);
    LRESULT OnNotify(NMHDR* header);
    LRESULT OnCoursesCustomDraw(NMLVCUSTOMDRAW* draw);
    bool OnClose();

    // ---- User actions -----------------------------------------------------
    void OnAddStudent();
    void OnAddCourse();
    void OnEnroll();
    void OnWithdraw();
    bool SaveData();
    void LoadData();

    // ---- Refreshing the display ---------------------------------------------
    void RefreshAll();
    void RefreshStudentsList();
    void RefreshCoursesList();
    void RefreshCombos();
    void FillCourseCombo(HWND combo);
    void RefreshStudentCourses();
    void RefreshCourseStudents();
    void UpdateEnrollButtons();
    void UpdateStatusBar();
    void UpdateTitle();
    void SetStatus(const std::wstring& text);

    // ---- List-view helpers ------------------------------------------------
    ListModel* ModelFor(int controlId);
    void SetRows(HWND list, ListModel& model, std::vector<ListRow> rows);
    void Resort(HWND list, ListModel& model);
    void Reapply(HWND list, const ListModel& model, const std::wstring& selectedKey);
    std::wstring SelectedKey(HWND list, const ListModel& model) const;
    void UpdateSortArrow(HWND list, const ListModel& model) const;

    // ---- Data -------------------------------------------------------------
    HINSTANCE instance_;
    EnrollmentSystem system_;
    FileManager files_;
    std::wstring lastAction_;

    // ---- Window state -----------------------------------------------------
    HWND hwnd_ = nullptr;
    HACCEL accel_ = nullptr;
    int dpi_ = 96;
    FontHandle font_;

    // ---- Controls (owned by Windows through the parent/child relationship) ---
    HWND status_ = nullptr;
    HWND tab_ = nullptr;
    std::array<HWND, 3> pages_{};

    HWND stuSearchLabel_ = nullptr;
    HWND stuSearch_ = nullptr;
    HWND stuAdd_ = nullptr;
    HWND stuList_ = nullptr;

    HWND crsSearchLabel_ = nullptr;
    HWND crsSearch_ = nullptr;
    HWND crsAdd_ = nullptr;
    HWND crsList_ = nullptr;

    HWND enrLeftGroup_ = nullptr;
    HWND enrStudentLabel_ = nullptr;
    HWND enrStudentCombo_ = nullptr;
    HWND enrStudentList_ = nullptr;
    HWND enrCourseLabel_ = nullptr;
    HWND enrCourseCombo_ = nullptr;
    HWND enrEnroll_ = nullptr;
    HWND enrWithdraw_ = nullptr;
    HWND enrRightGroup_ = nullptr;
    HWND enrViewLabel_ = nullptr;
    HWND enrViewCombo_ = nullptr;
    HWND enrSummary_ = nullptr;
    HWND enrProgress_ = nullptr;
    HWND enrViewList_ = nullptr;

    // ---- Models of the four list views ------------------------------------
    ListModel studentsModel_;
    ListModel coursesModel_;
    ListModel studentCoursesModel_;  // "By Student" list
    ListModel courseStudentsModel_;  // "By Course" list

    // Combo box item index -> ID (combo items display "ID - Name").
    std::vector<std::wstring> studentComboIds_;
    std::vector<std::wstring> courseComboIds_;
};
