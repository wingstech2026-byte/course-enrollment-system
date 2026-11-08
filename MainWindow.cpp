// MainWindow.cpp - window creation, controls, layout and message handling.
#include "MainWindow.h"

#include <algorithm>
#include <cwctype>

#include "Dialogs.h"
#include "resource.h"

namespace {

constexpr wchar_t kMainClass[] = L"CourseEnrollmentMainWindow";
constexpr wchar_t kPageClass[] = L"CourseEnrollmentPage";
constexpr wchar_t kAppTitle[] = L"Course Enrollment Management System";

// Window sizes at 96 DPI.
constexpr int kMinWidth96 = 1000;
constexpr int kMinHeight96 = 650;
constexpr int kInitialWidth96 = 1100;
constexpr int kInitialHeight96 = 720;

// Row colors of the courses list (light green / yellow / red).
constexpr COLORREF kColorOpen = RGB(226, 244, 229);
constexpr COLORREF kColorAlmostFull = RGB(255, 246, 204);
constexpr COLORREF kColorFull = RGB(250, 222, 222);
constexpr COLORREF kColorRowText = RGB(30, 30, 30);

// Course status levels (also stored in ListRow::tag).
constexpr int kStatusOpen = 0;
constexpr int kStatusAlmostFull = 1;
constexpr int kStatusFull = 2;

/// Converts a control ID to the HMENU parameter of CreateWindowEx.
HMENU ControlId(int id)
{
    return reinterpret_cast<HMENU>(static_cast<INT_PTR>(id));
}

/// Creates the "Segoe UI" 9 pt font scaled for the given DPI.
HFONT CreateUiFont(int dpi)
{
    return CreateFontW(-MulDiv(9, dpi, 72), 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE, DEFAULT_CHARSET,
                       OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY,
                       DEFAULT_PITCH | FF_DONTCARE, L"Segoe UI");
}

/// Moves/resizes a child window (ignores null handles and negative sizes).
void Place(HWND window, int x, int y, int width, int height)
{
    if (window == nullptr) return;
    MoveWindow(window, x, y, (std::max)(width, 0), (std::max)(height, 0), TRUE);
}

std::wstring GetText(HWND window)
{
    if (window == nullptr) return std::wstring();
    const int length = GetWindowTextLengthW(window);
    std::wstring text(static_cast<size_t>(length) + 1, L'\0');
    const int copied = GetWindowTextW(window, text.data(), length + 1);
    text.resize(static_cast<size_t>(copied));
    return text;
}

std::wstring ToLower(std::wstring text)
{
    for (wchar_t& ch : text) ch = static_cast<wchar_t>(std::towlower(ch));
    return text;
}

/// Case-insensitive substring test; @p needleLower must already be lower case.
bool ContainsNoCase(const std::wstring& haystack, const std::wstring& needleLower)
{
    return ToLower(haystack).find(needleLower) != std::wstring::npos;
}

/// Natural, case-insensitive comparison ("S2" sorts before "S10"). <0, 0, >0.
int CompareNatural(const std::wstring& a, const std::wstring& b)
{
    const int result = CompareStringEx(LOCALE_NAME_USER_DEFAULT, NORM_IGNORECASE | SORT_DIGITSASNUMBERS,
                                       a.c_str(), static_cast<int>(a.size()), b.c_str(),
                                       static_cast<int>(b.size()), nullptr, nullptr, 0);
    return result - CSTR_EQUAL;
}

ListRow MakeRow(std::vector<std::wstring> cells, int tag = 0)
{
    ListRow row;
    row.cells = std::move(cells);
    row.tag = tag;
    return row;
}

/// Open / Almost full (>= 80 %) / Full.
int StatusLevel(int enrolled, int capacity)
{
    if (enrolled >= capacity) return kStatusFull;
    if (enrolled * 100 >= capacity * 80) return kStatusAlmostFull;
    return kStatusOpen;
}

const wchar_t* StatusText(int level)
{
    switch (level) {
    case kStatusFull: return L"Full";
    case kStatusAlmostFull: return L"Almost full";
    default: return L"Open";
    }
}

/// Sorts the rows of a model by its current sort column / direction.
void SortRows(ListModel& model)
{
    if (model.sortColumn < 0) return;
    const size_t column = static_cast<size_t>(model.sortColumn);
    const bool ascending = model.ascending;
    std::stable_sort(model.rows.begin(), model.rows.end(), [column, ascending](const ListRow& a, const ListRow& b) {
        const int c = CompareNatural(a.cells[column], b.cells[column]);
        return ascending ? c < 0 : c > 0;
    });
}

/// ID of the item selected in a combo box ("" if none).
std::wstring ComboSelectedId(HWND combo, const std::vector<std::wstring>& ids)
{
    if (combo == nullptr) return std::wstring();
    const LRESULT index = SendMessageW(combo, CB_GETCURSEL, 0, 0);
    if (index < 0 || static_cast<size_t>(index) >= ids.size()) return std::wstring();
    return ids[static_cast<size_t>(index)];
}

/// Selects the combo item whose ID is @p id (or clears the selection).
void SelectComboById(HWND combo, const std::vector<std::wstring>& ids, const std::wstring& id)
{
    WPARAM index = static_cast<WPARAM>(-1);
    if (!id.empty()) {
        for (size_t i = 0; i < ids.size(); ++i) {
            if (ids[i] == id) {
                index = static_cast<WPARAM>(i);
                break;
            }
        }
    }
    SendMessageW(combo, CB_SETCURSEL, index, 0);
}

std::wstring PlacesText(int remaining)
{
    return std::to_wstring(remaining) + (remaining == 1 ? L" place remaining" : L" places remaining");
}

}  // namespace

// ===========================================================================
// Construction and creation
// ===========================================================================

MainWindow::MainWindow(HINSTANCE instance)
    : instance_(instance), files_(FileManager::ExecutableDirectory())
{
    studentsModel_.columns = {{L"Student ID", 110, false}, {L"Name", 260, true}, {L"Courses Taken", 120, false}};
    coursesModel_.columns = {{L"Course ID", 110, false}, {L"Name", 260, true},      {L"Capacity", 90, false},
                             {L"Enrolled", 90, false},   {L"Remaining", 100, false}, {L"Status", 110, false}};
    studentCoursesModel_.columns = {{L"Course ID", 90, false}, {L"Course Name", 200, true}, {L"Seats", 90, false}};
    courseStudentsModel_.columns = {{L"Student ID", 100, false}, {L"Student Name", 200, true}};
}

bool MainWindow::RegisterClasses()
{
    WNDCLASSEXW page{};
    page.cbSize = sizeof(page);
    page.lpfnWndProc = &MainWindow::PageProc;
    page.hInstance = instance_;
    page.hCursor = LoadCursorW(nullptr, IDC_ARROW);
    page.hbrBackground = reinterpret_cast<HBRUSH>(static_cast<INT_PTR>(COLOR_WINDOW + 1));
    page.lpszClassName = kPageClass;
    if (RegisterClassExW(&page) == 0) return false;

    WNDCLASSEXW main{};
    main.cbSize = sizeof(main);
    main.lpfnWndProc = &MainWindow::WindowProc;
    main.hInstance = instance_;
    main.hIcon = LoadIconW(instance_, MAKEINTRESOURCEW(IDI_APPICON));
    main.hIconSm = static_cast<HICON>(LoadImageW(instance_, MAKEINTRESOURCEW(IDI_APPICON), IMAGE_ICON,
                                                 GetSystemMetrics(SM_CXSMICON), GetSystemMetrics(SM_CYSMICON),
                                                 LR_SHARED));
    main.hCursor = LoadCursorW(nullptr, IDC_ARROW);
    main.hbrBackground = reinterpret_cast<HBRUSH>(static_cast<INT_PTR>(COLOR_BTNFACE + 1));
    main.lpszMenuName = MAKEINTRESOURCEW(IDR_MAINMENU);
    main.lpszClassName = kMainClass;
    return RegisterClassExW(&main) != 0;
}

bool MainWindow::Create(int showCommand)
{
    if (!RegisterClasses()) return false;

    dpi_ = static_cast<int>(GetDpiForSystem());
    const HWND hwnd = CreateWindowExW(0, kMainClass, kAppTitle, WS_OVERLAPPEDWINDOW | WS_CLIPCHILDREN,
                                      CW_USEDEFAULT, CW_USEDEFAULT, Scale(kInitialWidth96),
                                      Scale(kInitialHeight96), nullptr, nullptr, instance_, this);
    if (hwnd == nullptr) return false;

    accel_ = LoadAcceleratorsW(instance_, MAKEINTRESOURCEW(IDR_ACCEL));

    ShowWindow(hwnd_, showCommand);
    UpdateWindow(hwnd_);

    LoadData();  // automatic load at startup (missing files = empty data)
    return true;
}

int MainWindow::Run()
{
    MSG msg{};
    while (GetMessageW(&msg, nullptr, 0, 0) > 0) {
        if (TranslateAcceleratorW(hwnd_, accel_, &msg)) continue;  // Ctrl+S
        if (IsDialogMessageW(hwnd_, &msg)) continue;               // Tab / Shift+Tab / mnemonics
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }
    return static_cast<int>(msg.wParam);
}

HWND MainWindow::CreateChild(HWND parent, const wchar_t* className, const wchar_t* text, DWORD style, int id,
                             DWORD exStyle)
{
    return CreateWindowExW(exStyle, className, text, WS_CHILD | WS_VISIBLE | style, 0, 0, 0, 0, parent,
                           ControlId(id), instance_, nullptr);
}

HWND MainWindow::CreatePage(int id, bool visible, bool clipChildren)
{
    // The Enrollment page must NOT clip its children: its group boxes are transparent and cover
    // large areas, so the page has to erase its background underneath them.
    const DWORD style = WS_CHILD | WS_CLIPSIBLINGS | (clipChildren ? WS_CLIPCHILDREN : 0) | (visible ? WS_VISIBLE : 0);
    return CreateWindowExW(WS_EX_CONTROLPARENT, kPageClass, nullptr, style, 0, 0, 0, 0, hwnd_, ControlId(id),
                           instance_, nullptr);
}

HWND MainWindow::CreateList(HWND parent, int id, const ListModel& model)
{
    HWND list = CreateWindowExW(WS_EX_CLIENTEDGE, WC_LISTVIEWW, L"",
                                WS_CHILD | WS_VISIBLE | WS_TABSTOP | LVS_REPORT | LVS_SINGLESEL |
                                    LVS_SHOWSELALWAYS | LVS_OWNERDATA,
                                0, 0, 0, 0, parent, ControlId(id), instance_, nullptr);
    if (list == nullptr) return nullptr;

    ListView_SetExtendedListViewStyle(list, LVS_EX_FULLROWSELECT | LVS_EX_GRIDLINES | LVS_EX_DOUBLEBUFFER |
                                                LVS_EX_LABELTIP);
    SetWindowTheme(list, L"Explorer", nullptr);  // modern selection / header look

    for (size_t i = 0; i < model.columns.size(); ++i) {
        LVCOLUMNW column{};
        column.mask = LVCF_FMT | LVCF_TEXT | LVCF_WIDTH | LVCF_SUBITEM;
        column.fmt = LVCFMT_LEFT;
        column.cx = Scale(model.columns[i].width96);
        column.pszText = const_cast<LPWSTR>(model.columns[i].title.c_str());
        column.iSubItem = static_cast<int>(i);
        ListView_InsertColumn(list, static_cast<int>(i), &column);
    }
    return list;
}

bool MainWindow::CreateStudentsPage()
{
    HWND page = pages_[0];
    stuSearchLabel_ = CreateChild(page, L"STATIC", L"&Search:", SS_LEFT, IDC_STATIC);
    stuSearch_ = CreateChild(page, L"EDIT", L"", ES_AUTOHSCROLL | WS_TABSTOP, IDC_STU_SEARCH, WS_EX_CLIENTEDGE);
    stuAdd_ = CreateChild(page, L"BUTTON", L"Add Student...", BS_PUSHBUTTON | WS_TABSTOP, IDC_STU_ADD);
    stuList_ = CreateList(page, IDC_STU_LIST, studentsModel_);
    if (stuSearch_ != nullptr) Edit_SetCueBannerText(stuSearch_, L"Type to filter by ID or name...");
    return stuSearchLabel_ && stuSearch_ && stuAdd_ && stuList_;
}

bool MainWindow::CreateCoursesPage()
{
    HWND page = pages_[1];
    crsSearchLabel_ = CreateChild(page, L"STATIC", L"&Search:", SS_LEFT, IDC_STATIC);
    crsSearch_ = CreateChild(page, L"EDIT", L"", ES_AUTOHSCROLL | WS_TABSTOP, IDC_CRS_SEARCH, WS_EX_CLIENTEDGE);
    crsAdd_ = CreateChild(page, L"BUTTON", L"Add Course...", BS_PUSHBUTTON | WS_TABSTOP, IDC_CRS_ADD);
    crsList_ = CreateList(page, IDC_CRS_LIST, coursesModel_);
    if (crsSearch_ != nullptr) Edit_SetCueBannerText(crsSearch_, L"Type to filter by ID or name...");
    return crsSearchLabel_ && crsSearch_ && crsAdd_ && crsList_;
}

bool MainWindow::CreateEnrollmentPage()
{
    HWND page = pages_[2];

    // Creation order == tab order. Group boxes come first so they sit at the bottom of the z-order.
    enrLeftGroup_ = CreateChild(page, L"BUTTON", L"By Student", BS_GROUPBOX | WS_CLIPSIBLINGS, IDC_STATIC);
    enrStudentLabel_ = CreateChild(page, L"STATIC", L"S&tudent:", SS_LEFT, IDC_STATIC);
    enrStudentCombo_ = CreateChild(page, L"COMBOBOX", L"", CBS_DROPDOWNLIST | WS_VSCROLL | WS_TABSTOP, IDC_ENR_STU_COMBO);
    enrStudentList_ = CreateList(page, IDC_ENR_STU_LIST, studentCoursesModel_);
    enrCourseLabel_ = CreateChild(page, L"STATIC", L"&Course:", SS_LEFT, IDC_STATIC);
    enrCourseCombo_ = CreateChild(page, L"COMBOBOX", L"", CBS_DROPDOWNLIST | WS_VSCROLL | WS_TABSTOP, IDC_ENR_CRS_COMBO);
    enrEnroll_ = CreateChild(page, L"BUTTON", L"&Enroll", BS_PUSHBUTTON | WS_TABSTOP, IDC_ENR_ENROLL);
    enrWithdraw_ = CreateChild(page, L"BUTTON", L"&Withdraw Selected", BS_PUSHBUTTON | WS_TABSTOP, IDC_ENR_WITHDRAW);

    enrRightGroup_ = CreateChild(page, L"BUTTON", L"By Course", BS_GROUPBOX | WS_CLIPSIBLINGS, IDC_STATIC);
    enrViewLabel_ = CreateChild(page, L"STATIC", L"C&ourse:", SS_LEFT, IDC_STATIC);
    enrViewCombo_ = CreateChild(page, L"COMBOBOX", L"", CBS_DROPDOWNLIST | WS_VSCROLL | WS_TABSTOP, IDC_ENR_VIEW_COMBO);
    enrSummary_ = CreateChild(page, L"STATIC", L"", SS_LEFT | SS_NOPREFIX, IDC_ENR_VIEW_SUMMARY);
    enrProgress_ = CreateChild(page, PROGRESS_CLASSW, L"", PBS_SMOOTH, IDC_ENR_VIEW_PROGRESS);
    enrViewList_ = CreateList(page, IDC_ENR_VIEW_LIST, courseStudentsModel_);

    if (!(enrLeftGroup_ && enrStudentLabel_ && enrStudentCombo_ && enrStudentList_ && enrCourseLabel_ &&
          enrCourseCombo_ && enrEnroll_ && enrWithdraw_ && enrRightGroup_ && enrViewLabel_ && enrViewCombo_ &&
          enrSummary_ && enrProgress_ && enrViewList_))
        return false;

    // The group boxes were created first, which puts them on TOP of the z-order (over the
    // controls they frame). Send them to the bottom so they only act as background frames.
    SetWindowPos(enrLeftGroup_, HWND_BOTTOM, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE);
    SetWindowPos(enrRightGroup_, HWND_BOTTOM, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE);
    return true;
}

bool MainWindow::CreateControls()
{
    dpi_ = static_cast<int>(GetDpiForWindow(hwnd_));

    status_ = CreateWindowExW(0, STATUSCLASSNAMEW, nullptr, WS_CHILD | WS_VISIBLE | SBARS_SIZEGRIP, 0, 0, 0, 0,
                              hwnd_, ControlId(IDC_STATUSBAR), instance_, nullptr);
    tab_ = CreateWindowExW(0, WC_TABCONTROLW, nullptr, WS_CHILD | WS_VISIBLE | WS_CLIPSIBLINGS | WS_TABSTOP, 0, 0,
                           0, 0, hwnd_, ControlId(IDC_TAB), instance_, nullptr);
    if (status_ == nullptr || tab_ == nullptr) return false;

    const wchar_t* const titles[] = {L"Students", L"Courses", L"Enrollment"};
    for (int i = 0; i < 3; ++i) {
        TCITEMW item{};
        item.mask = TCIF_TEXT;
        item.pszText = const_cast<LPWSTR>(titles[i]);
        TabCtrl_InsertItem(tab_, i, &item);
    }

    pages_[0] = CreatePage(IDC_PAGE_STUDENTS, true, true);
    pages_[1] = CreatePage(IDC_PAGE_COURSES, false, true);
    pages_[2] = CreatePage(IDC_PAGE_ENROLLMENT, false, false);
    for (HWND page : pages_) {
        if (page == nullptr) return false;
    }

    if (!CreateStudentsPage() || !CreateCoursesPage() || !CreateEnrollmentPage()) return false;

    // New child windows are inserted at the BOTTOM of the sibling z-order, so the tab control
    // (created first) would be drawn on top of the pages. Send it to the very bottom so the
    // pages sit above it. (Side effect: the tab strip is last in the Tab-key order.)
    SetWindowPos(tab_, HWND_BOTTOM, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE);

    SendMessageW(enrProgress_, PBM_SETRANGE32, 0, 100);

    UpdateFont();
    Layout();
    RefreshAll();
    SetStatus(L"Ready.");
    return true;
}

void MainWindow::UpdateFont()
{
    FontHandle font(CreateUiFont(dpi_));
    if (!font) return;
    EnumChildWindows(hwnd_, &MainWindow::SetFontProc, reinterpret_cast<LPARAM>(font.get()));
    font_ = std::move(font);  // the old font is released only after every control switched over
}

BOOL CALLBACK MainWindow::SetFontProc(HWND child, LPARAM font)
{
    SendMessageW(child, WM_SETFONT, static_cast<WPARAM>(font), TRUE);
    return TRUE;
}

// ===========================================================================
// Window procedures
// ===========================================================================

LRESULT CALLBACK MainWindow::WindowProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam)
{
    MainWindow* self = nullptr;
    if (message == WM_NCCREATE) {
        const auto* create = reinterpret_cast<const CREATESTRUCTW*>(lParam);
        self = static_cast<MainWindow*>(create->lpCreateParams);
        self->hwnd_ = hwnd;
        SetWindowLongPtrW(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(self));
    } else {
        self = reinterpret_cast<MainWindow*>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));
    }
    if (self == nullptr) return DefWindowProcW(hwnd, message, wParam, lParam);

    const LRESULT result = self->HandleMessage(message, wParam, lParam);
    if (message == WM_NCDESTROY) {
        SetWindowLongPtrW(hwnd, GWLP_USERDATA, 0);
        self->hwnd_ = nullptr;
    }
    return result;
}

// The tab "pages" are plain container windows. They forward notifications to
// the main window and paint labels on the same white background as the tab body.
LRESULT CALLBACK MainWindow::PageProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam)
{
    switch (message) {
    case WM_COMMAND:
    case WM_NOTIFY:
        return SendMessageW(GetParent(hwnd), message, wParam, lParam);
    case WM_CTLCOLORSTATIC:
        SetBkMode(reinterpret_cast<HDC>(wParam), TRANSPARENT);
        return reinterpret_cast<LRESULT>(GetSysColorBrush(COLOR_WINDOW));
    default:
        return DefWindowProcW(hwnd, message, wParam, lParam);
    }
}

LRESULT MainWindow::HandleMessage(UINT message, WPARAM wParam, LPARAM lParam)
{
    switch (message) {
    case WM_CREATE:
        return CreateControls() ? 0 : -1;

    case WM_SIZE:
        if (wParam != SIZE_MINIMIZED) Layout();
        return 0;

    case WM_GETMINMAXINFO: {
        auto* info = reinterpret_cast<MINMAXINFO*>(lParam);
        info->ptMinTrackSize.x = Scale(kMinWidth96);
        info->ptMinTrackSize.y = Scale(kMinHeight96);
        return 0;
    }

    case WM_DPICHANGED: {
        dpi_ = static_cast<int>(HIWORD(wParam));
        UpdateFont();
        const auto* suggested = reinterpret_cast<const RECT*>(lParam);
        SetWindowPos(hwnd_, nullptr, suggested->left, suggested->top, suggested->right - suggested->left,
                     suggested->bottom - suggested->top, SWP_NOZORDER | SWP_NOACTIVATE);
        Layout();
        return 0;
    }

    case WM_SETFOCUS:
        if (tab_ != nullptr) SetFocus(tab_);
        return 0;

    case WM_COMMAND:
        OnCommand(static_cast<int>(LOWORD(wParam)), static_cast<int>(HIWORD(wParam)));
        return 0;

    case WM_NOTIFY:
        return OnNotify(reinterpret_cast<NMHDR*>(lParam));

    case WM_CLOSE:
        if (OnClose()) DestroyWindow(hwnd_);
        return 0;

    case WM_DESTROY:
        PostQuitMessage(0);
        return 0;

    default:
        break;
    }
    return DefWindowProcW(hwnd_, message, wParam, lParam);
}

// ===========================================================================
// Layout
// ===========================================================================

int MainWindow::Scale(int value96) const
{
    return MulDiv(value96, dpi_, 96);
}

void MainWindow::UpdateStatusParts()
{
    const int first = Scale(140);
    const int second = first + Scale(140);
    const int third = second + Scale(170);
    int edges[4] = {first, second, third, -1};
    SendMessageW(status_, SB_SETPARTS, 4, reinterpret_cast<LPARAM>(edges));
}

void MainWindow::Layout()
{
    if (tab_ == nullptr || status_ == nullptr) return;

    RECT client{};
    GetClientRect(hwnd_, &client);

    SendMessageW(status_, WM_SIZE, 0, 0);  // the status bar positions itself at the bottom
    UpdateStatusParts();
    RECT statusRect{};
    GetWindowRect(status_, &statusRect);
    const int statusHeight = statusRect.bottom - statusRect.top;

    const int margin = Scale(8);
    const int tabWidth = client.right - 2 * margin;
    const int tabHeight = client.bottom - statusHeight - 2 * margin;
    if (tabWidth <= 0 || tabHeight <= 0) return;
    MoveWindow(tab_, margin, margin, tabWidth, tabHeight, TRUE);

    // The tab's display area, converted to main-window client coordinates.
    RECT display{};
    GetClientRect(tab_, &display);
    TabCtrl_AdjustRect(tab_, FALSE, &display);
    MapWindowPoints(tab_, hwnd_, reinterpret_cast<POINT*>(&display), 2);
    const int width = display.right - display.left;
    const int height = display.bottom - display.top;

    for (HWND page : pages_) Place(page, display.left, display.top, width, height);

    LayoutStudentsPage(width, height);
    LayoutCoursesPage(width, height);
    LayoutEnrollmentPage(width, height);
    UpdateStatusBar();
}

void MainWindow::FitColumns(HWND list, const ListModel& model) const
{
    if (list == nullptr) return;

    RECT rc{};
    GetClientRect(list, &rc);

    int fixedTotal = 0;
    int flexible = -1;
    for (size_t i = 0; i < model.columns.size(); ++i) {
        if (model.columns[i].flexible) {
            flexible = static_cast<int>(i);
        } else {
            fixedTotal += Scale(model.columns[i].width96);
            ListView_SetColumnWidth(list, static_cast<int>(i), Scale(model.columns[i].width96));
        }
    }
    if (flexible >= 0) {
        const int available = rc.right - fixedTotal - GetSystemMetricsForDpi(SM_CXVSCROLL, static_cast<UINT>(dpi_));
        const int minimum = Scale(model.columns[static_cast<size_t>(flexible)].width96);
        ListView_SetColumnWidth(list, flexible, (std::max)(minimum, available));
    }
}

void MainWindow::LayoutStudentsPage(int width, int height)
{
    const int margin = Scale(12);
    const int editHeight = Scale(24);
    const int buttonHeight = Scale(28);
    const int rowHeight = (std::max)(editHeight, buttonHeight);

    Place(stuSearchLabel_, margin, margin + Scale(5), Scale(56), Scale(18));
    Place(stuSearch_, margin + Scale(60), margin + (rowHeight - editHeight) / 2, Scale(300), editHeight);
    Place(stuAdd_, width - margin - Scale(130), margin, Scale(130), buttonHeight);

    const int listTop = margin + rowHeight + margin;
    Place(stuList_, margin, listTop, width - 2 * margin, height - listTop - margin);
    FitColumns(stuList_, studentsModel_);
}

void MainWindow::LayoutCoursesPage(int width, int height)
{
    const int margin = Scale(12);
    const int editHeight = Scale(24);
    const int buttonHeight = Scale(28);
    const int rowHeight = (std::max)(editHeight, buttonHeight);

    Place(crsSearchLabel_, margin, margin + Scale(5), Scale(56), Scale(18));
    Place(crsSearch_, margin + Scale(60), margin + (rowHeight - editHeight) / 2, Scale(300), editHeight);
    Place(crsAdd_, width - margin - Scale(130), margin, Scale(130), buttonHeight);

    const int listTop = margin + rowHeight + margin;
    Place(crsList_, margin, listTop, width - 2 * margin, height - listTop - margin);
    FitColumns(crsList_, coursesModel_);
}

void MainWindow::LayoutEnrollmentPage(int width, int height)
{
    const int margin = Scale(10);
    const int gap = Scale(12);
    const int padding = Scale(12);
    const int captionHeight = Scale(26);  // room for the group box caption
    const int editHeight = Scale(24);
    const int buttonHeight = Scale(28);
    const int labelWidth = Scale(64);
    const int dropDownHeight = Scale(260);  // for a combo box this height includes the drop-down list

    const int groupWidth = (width - 2 * margin - gap) / 2;
    const int groupHeight = height - 2 * margin;
    const int top = margin;
    const int innerWidth = groupWidth - 2 * padding;
    const int bottom = top + groupHeight - padding;

    // ---- Left: By Student ------------------------------------------------
    int x = margin + padding;
    int y = top + captionHeight;
    Place(enrLeftGroup_, margin, top, groupWidth, groupHeight);
    Place(enrStudentLabel_, x, y + Scale(4), labelWidth, Scale(18));
    Place(enrStudentCombo_, x + labelWidth, y, innerWidth - labelWidth, dropDownHeight);
    y += editHeight + Scale(10);

    const int buttonsY = bottom - buttonHeight;
    const int courseRowY = buttonsY - Scale(10) - editHeight;
    Place(enrStudentList_, x, y, innerWidth, courseRowY - Scale(12) - y);
    Place(enrCourseLabel_, x, courseRowY + Scale(4), labelWidth, Scale(18));
    Place(enrCourseCombo_, x + labelWidth, courseRowY, innerWidth - labelWidth, dropDownHeight);
    Place(enrEnroll_, x + labelWidth, buttonsY, Scale(110), buttonHeight);
    Place(enrWithdraw_, x + labelWidth + Scale(118), buttonsY, Scale(160), buttonHeight);
    FitColumns(enrStudentList_, studentCoursesModel_);

    // ---- Right: By Course ------------------------------------------------
    const int rightX = margin + groupWidth + gap;
    x = rightX + padding;
    y = top + captionHeight;
    Place(enrRightGroup_, rightX, top, groupWidth, groupHeight);
    Place(enrViewLabel_, x, y + Scale(4), labelWidth, Scale(18));
    Place(enrViewCombo_, x + labelWidth, y, innerWidth - labelWidth, dropDownHeight);
    y += editHeight + Scale(12);
    Place(enrSummary_, x, y, innerWidth, Scale(20));
    y += Scale(24);
    Place(enrProgress_, x, y, innerWidth, Scale(16));
    y += Scale(16) + Scale(12);
    Place(enrViewList_, x, y, innerWidth, bottom - y);
    FitColumns(enrViewList_, courseStudentsModel_);
}

void MainWindow::ShowPage(int index)
{
    // Hide the old pages first, then show the new one.
    for (int i = 0; i < static_cast<int>(pages_.size()); ++i) {
        if (i != index) ShowWindow(pages_[static_cast<size_t>(i)], SW_HIDE);
    }
    if (index >= 0 && index < static_cast<int>(pages_.size())) {
        ShowWindow(pages_[static_cast<size_t>(index)], SW_SHOW);
    }

    // Force a complete erase + repaint of the tab area; otherwise pixels of the previous page
    // can stay visible behind the new page's controls.
    RedrawWindow(hwnd_, nullptr, nullptr, RDW_INVALIDATE | RDW_ERASE | RDW_ALLCHILDREN | RDW_UPDATENOW);
}

// ===========================================================================
// Command and notification handling
// ===========================================================================

void MainWindow::OnCommand(int id, int code)
{
    switch (id) {
    case IDM_FILE_SAVE:
        SaveData();
        break;
    case IDM_FILE_EXIT:
        SendMessageW(hwnd_, WM_CLOSE, 0, 0);
        break;
    case IDM_HELP_ABOUT:
        ShowAboutDialog(hwnd_, instance_);
        break;

    case IDC_STU_ADD:
        if (code == BN_CLICKED) OnAddStudent();
        break;
    case IDC_CRS_ADD:
        if (code == BN_CLICKED) OnAddCourse();
        break;
    case IDC_STU_SEARCH:
        if (code == EN_CHANGE) RefreshStudentsList();  // filter as you type
        break;
    case IDC_CRS_SEARCH:
        if (code == EN_CHANGE) RefreshCoursesList();
        break;

    case IDC_ENR_STU_COMBO:
        if (code == CBN_SELCHANGE) {
            RefreshStudentCourses();
            UpdateEnrollButtons();
        }
        break;
    case IDC_ENR_CRS_COMBO:
        if (code == CBN_SELCHANGE) UpdateEnrollButtons();
        break;
    case IDC_ENR_VIEW_COMBO:
        if (code == CBN_SELCHANGE) RefreshCourseStudents();
        break;
    case IDC_ENR_ENROLL:
        if (code == BN_CLICKED) OnEnroll();
        break;
    case IDC_ENR_WITHDRAW:
        if (code == BN_CLICKED) OnWithdraw();
        break;

    default:
        break;  // IDOK / IDCANCEL sent by IsDialogMessage are ignored
    }
}

LRESULT MainWindow::OnNotify(NMHDR* header)
{
    const int id = static_cast<int>(header->idFrom);

    switch (header->code) {
    case TCN_SELCHANGE:
        if (id == IDC_TAB) ShowPage(TabCtrl_GetCurSel(tab_));
        return 0;

    case LVN_GETDISPINFOW: {
        // Virtual list: the control asks us for the text of each visible cell.
        auto* info = reinterpret_cast<NMLVDISPINFOW*>(header);
        const ListModel* model = ModelFor(id);
        if (model != nullptr && (info->item.mask & LVIF_TEXT) != 0) {
            const size_t row = static_cast<size_t>(info->item.iItem);
            const size_t column = static_cast<size_t>(info->item.iSubItem);
            if (row < model->rows.size() && column < model->rows[row].cells.size())
                info->item.pszText = const_cast<LPWSTR>(model->rows[row].cells[column].c_str());
        }
        return 0;
    }

    case LVN_COLUMNCLICK: {
        // Click on a column header: sort by it (click again to reverse).
        const auto* view = reinterpret_cast<const NMLISTVIEW*>(header);
        ListModel* model = ModelFor(id);
        if (model != nullptr) {
            if (model->sortColumn == view->iSubItem) {
                model->ascending = !model->ascending;
            } else {
                model->sortColumn = view->iSubItem;
                model->ascending = true;
            }
            Resort(header->hwndFrom, *model);
        }
        return 0;
    }

    case LVN_ITEMCHANGED:
    case LVN_ODSTATECHANGED:
        if (id == IDC_ENR_STU_LIST) UpdateEnrollButtons();  // Withdraw depends on the selection
        return 0;

    case NM_CUSTOMDRAW:
        if (id == IDC_CRS_LIST) return OnCoursesCustomDraw(reinterpret_cast<NMLVCUSTOMDRAW*>(header));
        return CDRF_DODEFAULT;

    default:
        return 0;
    }
}

LRESULT MainWindow::OnCoursesCustomDraw(NMLVCUSTOMDRAW* draw)
{
    switch (draw->nmcd.dwDrawStage) {
    case CDDS_PREPAINT:
        return CDRF_NOTIFYITEMDRAW;

    case CDDS_ITEMPREPAINT: {
        const size_t row = static_cast<size_t>(draw->nmcd.dwItemSpec);
        const bool selected =
            (ListView_GetItemState(crsList_, static_cast<int>(row), LVIS_SELECTED) & LVIS_SELECTED) != 0;
        if (row < coursesModel_.rows.size() && !selected) {  // keep the normal highlight for the selected row
            switch (coursesModel_.rows[row].tag) {
            case kStatusFull: draw->clrTextBk = kColorFull; break;
            case kStatusAlmostFull: draw->clrTextBk = kColorAlmostFull; break;
            default: draw->clrTextBk = kColorOpen; break;
            }
            draw->clrText = kColorRowText;
        }
        return CDRF_NEWFONT;
    }

    default:
        return CDRF_DODEFAULT;
    }
}

bool MainWindow::OnClose()
{
    if (!system_.IsModified()) return true;

    const int answer = MessageBoxW(hwnd_, L"You have unsaved changes.\n\nSave changes?", kAppTitle,
                                   MB_YESNOCANCEL | MB_ICONQUESTION);
    if (answer == IDCANCEL) return false;
    if (answer == IDYES) return SaveData();  // a failed save keeps the window open
    return true;                              // IDNO: discard changes
}

// ===========================================================================
// User actions
// ===========================================================================

void MainWindow::OnAddStudent()
{
    std::wstring message;
    if (ShowAddStudentDialog(hwnd_, instance_, system_, message)) {
        RefreshAll();
        SetStatus(message);
    }
}

void MainWindow::OnAddCourse()
{
    std::wstring message;
    if (ShowAddCourseDialog(hwnd_, instance_, system_, message)) {
        RefreshAll();
        SetStatus(message);
    }
}

void MainWindow::OnEnroll()
{
    const std::wstring studentId = ComboSelectedId(enrStudentCombo_, studentComboIds_);
    const std::wstring courseId = ComboSelectedId(enrCourseCombo_, courseComboIds_);

    const OperationResult result = system_.Enroll(studentId, courseId);
    if (!result) {
        MessageBoxW(hwnd_, result.message.c_str(), L"Cannot enroll", MB_OK | MB_ICONERROR);
        SetStatus(L"Enrollment rejected.");
        return;
    }
    RefreshAll();
    SetStatus(result.message);
}

void MainWindow::OnWithdraw()
{
    const std::wstring studentId = ComboSelectedId(enrStudentCombo_, studentComboIds_);
    const int index = ListView_GetNextItem(enrStudentList_, -1, LVNI_SELECTED);
    if (studentId.empty() || index < 0 || static_cast<size_t>(index) >= studentCoursesModel_.rows.size()) return;
    const std::wstring courseId = studentCoursesModel_.rows[static_cast<size_t>(index)].cells[0];

    const Student* student = system_.FindStudent(studentId);
    const Course* course = system_.FindCourse(courseId);
    std::wstring question = L"Withdraw this student from the course?";
    if (student != nullptr && course != nullptr) {
        question = L"Withdraw " + student->Name() + L" (" + student->Id() + L") from " + course->Name() + L" (" +
                   course->Id() + L")?";
    }
    if (MessageBoxW(hwnd_, question.c_str(), L"Confirm withdrawal", MB_YESNO | MB_ICONQUESTION | MB_DEFBUTTON2) !=
        IDYES)
        return;

    const OperationResult result = system_.Withdraw(studentId, courseId);
    if (!result) {
        MessageBoxW(hwnd_, result.message.c_str(), L"Cannot withdraw", MB_OK | MB_ICONERROR);
        SetStatus(L"Withdrawal rejected.");
        return;
    }
    RefreshAll();
    SetStatus(result.message);
    SetFocus(enrStudentList_);  // the Withdraw button just became disabled
}

bool MainWindow::SaveData()
{
    const OperationResult result = files_.Save(system_);
    if (!result) {
        MessageBoxW(hwnd_, (L"The data could not be saved.\n\n" + result.message).c_str(), kAppTitle,
                    MB_OK | MB_ICONERROR);
        SetStatus(L"Save failed.");
        return false;
    }
    system_.MarkSaved();
    UpdateTitle();
    SetStatus(result.message);
    return true;
}

void MainWindow::LoadData()
{
    const LoadReport report = files_.Load(system_);
    RefreshAll();

    std::wstring status = L"Loaded " + std::to_wstring(report.studentsLoaded) + L" students, " +
                          std::to_wstring(report.coursesLoaded) + L" courses, " +
                          std::to_wstring(report.enrollmentsLoaded) + L" enrollments.";
    if (!report.warnings.empty()) status += L" Problems found: " + std::to_wstring(report.warnings.size()) + L".";
    SetStatus(status);

    if (!report.warnings.empty())
        MessageBoxW(hwnd_, report.Summary(15).c_str(), kAppTitle, MB_OK | MB_ICONWARNING);
}

// ===========================================================================
// Refreshing the display
// ===========================================================================

void MainWindow::RefreshAll()
{
    RefreshStudentsList();
    RefreshCoursesList();
    RefreshCombos();
    RefreshStudentCourses();
    RefreshCourseStudents();
    UpdateEnrollButtons();
    UpdateStatusBar();
    UpdateTitle();
}

void MainWindow::RefreshStudentsList()
{
    const std::wstring filter = ToLower(GetText(stuSearch_));

    std::vector<ListRow> rows;
    for (const Student& student : system_.Students()) {
        if (!filter.empty() && !ContainsNoCase(student.Id(), filter) && !ContainsNoCase(student.Name(), filter))
            continue;
        rows.push_back(MakeRow({student.Id(), student.Name(),
                                std::to_wstring(system_.CourseCountOfStudent(student.Id()))}));
    }
    SetRows(stuList_, studentsModel_, std::move(rows));
}

void MainWindow::RefreshCoursesList()
{
    const std::wstring filter = ToLower(GetText(crsSearch_));

    std::vector<ListRow> rows;
    for (const Course& course : system_.Courses()) {
        if (!filter.empty() && !ContainsNoCase(course.Id(), filter) && !ContainsNoCase(course.Name(), filter))
            continue;
        const int enrolled = system_.EnrolledCount(course.Id());
        const int remaining = (std::max)(0, course.Capacity() - enrolled);
        const int level = StatusLevel(enrolled, course.Capacity());
        rows.push_back(MakeRow({course.Id(), course.Name(), std::to_wstring(course.Capacity()),
                                std::to_wstring(enrolled), std::to_wstring(remaining), StatusText(level)},
                               level));
    }
    SetRows(crsList_, coursesModel_, std::move(rows));
}

void MainWindow::FillCourseCombo(HWND combo)
{
    SendMessageW(combo, CB_RESETCONTENT, 0, 0);
    for (const std::wstring& id : courseComboIds_) {
        const Course* course = system_.FindCourse(id);
        if (course == nullptr) continue;
        const int remaining = system_.RemainingPlaces(id);
        const std::wstring label = course->Id() + L" - " + course->Name() + L"  (" +
                                   (remaining > 0 ? std::to_wstring(remaining) + L" left" : std::wstring(L"full")) +
                                   L")";
        SendMessageW(combo, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(label.c_str()));
    }
}

void MainWindow::RefreshCombos()
{
    // Remember the current selections so they survive the rebuild.
    const std::wstring selectedStudent = ComboSelectedId(enrStudentCombo_, studentComboIds_);
    const std::wstring selectedEnrollCourse = ComboSelectedId(enrCourseCombo_, courseComboIds_);
    const std::wstring selectedViewCourse = ComboSelectedId(enrViewCombo_, courseComboIds_);

    // Students, sorted by ID, shown as "ID - Name".
    std::vector<const Student*> students;
    for (const Student& student : system_.Students()) students.push_back(&student);
    std::stable_sort(students.begin(), students.end(),
                     [](const Student* a, const Student* b) { return CompareNatural(a->Id(), b->Id()) < 0; });

    studentComboIds_.clear();
    SendMessageW(enrStudentCombo_, CB_RESETCONTENT, 0, 0);
    for (const Student* student : students) {
        studentComboIds_.push_back(student->Id());
        const std::wstring label = student->Id() + L" - " + student->Name();
        SendMessageW(enrStudentCombo_, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(label.c_str()));
    }
    SelectComboById(enrStudentCombo_, studentComboIds_, selectedStudent);

    // Courses (both course combos share the same ID list), sorted by ID.
    std::vector<const Course*> courses;
    for (const Course& course : system_.Courses()) courses.push_back(&course);
    std::stable_sort(courses.begin(), courses.end(),
                     [](const Course* a, const Course* b) { return CompareNatural(a->Id(), b->Id()) < 0; });

    courseComboIds_.clear();
    for (const Course* course : courses) courseComboIds_.push_back(course->Id());
    FillCourseCombo(enrCourseCombo_);
    FillCourseCombo(enrViewCombo_);
    SelectComboById(enrCourseCombo_, courseComboIds_, selectedEnrollCourse);
    SelectComboById(enrViewCombo_, courseComboIds_, selectedViewCourse);
}

void MainWindow::RefreshStudentCourses()
{
    const std::wstring studentId = ComboSelectedId(enrStudentCombo_, studentComboIds_);

    std::vector<ListRow> rows;
    if (!studentId.empty()) {
        for (const Course* course : system_.CoursesOfStudent(studentId)) {
            const std::wstring seats =
                std::to_wstring(system_.EnrolledCount(course->Id())) + L" / " + std::to_wstring(course->Capacity());
            rows.push_back(MakeRow({course->Id(), course->Name(), seats}));
        }
    }
    SetRows(enrStudentList_, studentCoursesModel_, std::move(rows));
}

void MainWindow::RefreshCourseStudents()
{
    const std::wstring courseId = ComboSelectedId(enrViewCombo_, courseComboIds_);
    const Course* course = courseId.empty() ? nullptr : system_.FindCourse(courseId);

    std::vector<ListRow> rows;
    if (course == nullptr) {
        SetWindowTextW(enrSummary_, L"Select a course to see its enrolled students.");
        SendMessageW(enrProgress_, PBM_SETRANGE32, 0, 100);
        SendMessageW(enrProgress_, PBM_SETPOS, 0, 0);
        SendMessageW(enrProgress_, PBM_SETSTATE, PBST_NORMAL, 0);
    } else {
        for (const Student* student : system_.StudentsInCourse(courseId))
            rows.push_back(MakeRow({student->Id(), student->Name()}));

        const int enrolled = system_.EnrolledCount(courseId);
        const int remaining = system_.RemainingPlaces(courseId);
        const std::wstring summary = L"Enrolled " + std::to_wstring(enrolled) + L" / " +
                                     std::to_wstring(course->Capacity()) + L" — " + PlacesText(remaining);
        SetWindowTextW(enrSummary_, summary.c_str());

        // The bar turns yellow when almost full and red when full (same thresholds as the Status column).
        const int level = StatusLevel(enrolled, course->Capacity());
        SendMessageW(enrProgress_, PBM_SETRANGE32, 0, course->Capacity());
        SendMessageW(enrProgress_, PBM_SETPOS, static_cast<WPARAM>(enrolled), 0);
        SendMessageW(enrProgress_, PBM_SETSTATE,
                     level == kStatusFull ? PBST_ERROR : (level == kStatusAlmostFull ? PBST_PAUSED : PBST_NORMAL), 0);
    }
    SetRows(enrViewList_, courseStudentsModel_, std::move(rows));
}

void MainWindow::UpdateEnrollButtons()
{
    if (enrEnroll_ == nullptr || enrWithdraw_ == nullptr || enrStudentList_ == nullptr) return;

    const bool studentSelected = !ComboSelectedId(enrStudentCombo_, studentComboIds_).empty();
    const bool courseSelected = !ComboSelectedId(enrCourseCombo_, courseComboIds_).empty();
    const bool rowSelected = ListView_GetNextItem(enrStudentList_, -1, LVNI_SELECTED) >= 0;

    EnableWindow(enrEnroll_, (studentSelected && courseSelected) ? TRUE : FALSE);
    EnableWindow(enrWithdraw_, (studentSelected && rowSelected) ? TRUE : FALSE);
}

void MainWindow::UpdateStatusBar()
{
    if (status_ == nullptr) return;

    const std::wstring parts[4] = {
        L"Students: " + std::to_wstring(system_.Students().size()),
        L"Courses: " + std::to_wstring(system_.Courses().size()),
        L"Enrollments: " + std::to_wstring(system_.Enrollments().size()),
        lastAction_,
    };
    for (int i = 0; i < 4; ++i)
        SendMessageW(status_, SB_SETTEXTW, static_cast<WPARAM>(i), reinterpret_cast<LPARAM>(parts[i].c_str()));
}

void MainWindow::UpdateTitle()
{
    std::wstring title = kAppTitle;
    if (system_.IsModified()) title += L" *";
    SetWindowTextW(hwnd_, title.c_str());
}

void MainWindow::SetStatus(const std::wstring& text)
{
    lastAction_ = text;
    UpdateStatusBar();
}

// ===========================================================================
// List-view helpers (virtual lists: the data lives in ListModel)
// ===========================================================================

ListModel* MainWindow::ModelFor(int controlId)
{
    switch (controlId) {
    case IDC_STU_LIST: return &studentsModel_;
    case IDC_CRS_LIST: return &coursesModel_;
    case IDC_ENR_STU_LIST: return &studentCoursesModel_;
    case IDC_ENR_VIEW_LIST: return &courseStudentsModel_;
    default: return nullptr;
    }
}

std::wstring MainWindow::SelectedKey(HWND list, const ListModel& model) const
{
    if (list == nullptr) return std::wstring();
    const int index = ListView_GetNextItem(list, -1, LVNI_SELECTED);
    if (index >= 0 && static_cast<size_t>(index) < model.rows.size())
        return model.rows[static_cast<size_t>(index)].cells[0];
    return std::wstring();
}

void MainWindow::SetRows(HWND list, ListModel& model, std::vector<ListRow> rows)
{
    if (list == nullptr) return;
    const std::wstring key = SelectedKey(list, model);  // must be read before the rows change
    model.rows = std::move(rows);
    SortRows(model);
    Reapply(list, model, key);
}

void MainWindow::Resort(HWND list, ListModel& model)
{
    const std::wstring key = SelectedKey(list, model);
    SortRows(model);
    Reapply(list, model, key);
}

void MainWindow::Reapply(HWND list, const ListModel& model, const std::wstring& selectedKey)
{
    SendMessageW(list, WM_SETREDRAW, FALSE, 0);

    ListView_SetItemCountEx(list, static_cast<int>(model.rows.size()), 0);
    {
        ListView_SetItemState(list, -1, 0, LVIS_SELECTED | LVIS_FOCUSED);
    }
    if (!selectedKey.empty()) {
        for (size_t i = 0; i < model.rows.size(); ++i) {
            if (model.rows[i].cells[0] != selectedKey) continue;
            {
                ListView_SetItemState(list, static_cast<int>(i), LVIS_SELECTED | LVIS_FOCUSED,
                                      LVIS_SELECTED | LVIS_FOCUSED);
            }
            ListView_EnsureVisible(list, static_cast<int>(i), FALSE);
            break;
        }
    }

    SendMessageW(list, WM_SETREDRAW, TRUE, 0);
    InvalidateRect(list, nullptr, TRUE);
    UpdateSortArrow(list, model);
}

void MainWindow::UpdateSortArrow(HWND list, const ListModel& model) const
{
    HWND header = ListView_GetHeader(list);
    const int count = Header_GetItemCount(header);
    for (int i = 0; i < count; ++i) {
        HDITEMW item{};
        item.mask = HDI_FORMAT;
        Header_GetItem(header, i, &item);
        item.fmt &= ~(HDF_SORTUP | HDF_SORTDOWN);
        if (i == model.sortColumn) item.fmt |= model.ascending ? HDF_SORTUP : HDF_SORTDOWN;
        Header_SetItem(header, i, &item);
    }
}
