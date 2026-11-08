# Course Enrollment Management System

A Win32 desktop application (C++17, Visual Studio 2022, no third-party libraries) for managing students, courses and enrollments.

## 1. Files

| File | Purpose |
|---|---|
| `Student.h`, `Course.h`, `Enrollment.h` | Simple data classes |
| `EnrollmentSystem.h/.cpp` | Owns the data and **all** business rules. No GUI code. Every method returns `OperationResult` (success + message) |
| `FileManager.h/.cpp` | Loads/saves the three UTF-8 files, converts UTF-8 <-> `std::wstring`, validates every loaded line through `EnrollmentSystem` |
| `MainWindow.h/.cpp` | Window, tab control, list views, status bar, DPI handling, message handling |
| `Dialogs.h/.cpp` | Add Student / Add Course / About dialogs |
| `Win32Common.h` | Windows headers, `NOMINMAX`, `UNICODE`, library pragmas |
| `resource.h`, `app.rc`, `app.ico` | Menu, accelerators, dialogs, icon |
| `main.cpp` | `wWinMain`, Per-Monitor V2 DPI, Common Controls v6 manifest pragma, `InitCommonControlsEx` |
| `SampleData\*.txt` | Demo data (6 students, 4 courses, 12 enrollments) |
| `SampleData\InvalidTest\*.txt` | Deliberately broken files to demonstrate the load warning |
| `EnrollmentManagementSystem.sln/.vcxproj` | Ready-made VS 2022 project (see 2A) |

## 2. Visual Studio 2022 setup

Prerequisite: Visual Studio 2022 with the **Desktop development with C++** workload (the free *Build Tools* edition also works: build with `msbuild EnrollmentManagementSystem.vcxproj /p:Configuration=Release /p:Platform=x64`).

### 2A. Quickest: open the supplied project
1. Double-click `EnrollmentManagementSystem.sln`.
2. Choose **Debug | x64** (or Release) and press **Ctrl+F5**.
3. The post-build step copies `SampleData\*.txt` next to the `.exe` (`x64\Debug\`) **only if** no data files are there yet.

The project already has: C++17, Unicode, Subsystem **Windows**, warning level **/W4**, `/utf-8`, `comctl32.lib` + `uxtheme.lib`, and `app.rc`.

### 2B. From scratch (if your instructor wants you to create the project yourself)
1. **File > New > Project > Empty Project** (C++, Windows). *(Do not pick "Windows Desktop Application": it generates its own `wWinMain`, resources and files that you would have to delete.)* Name it `EnrollmentManagementSystem`.
2. Copy all `.h`, `.cpp`, `app.rc`, `app.ico` files into the project folder. In Solution Explorer: right-click project > **Add > Existing Item...** and add them all (the `.rc` file goes in automatically as a *Resource File*; the `.ico` is referenced by it).
3. Right-click project > **Properties**, with *Configuration: All Configurations*, *Platform: All Platforms*:
   - **General > C++ Language Standard** = *ISO C++17 Standard (/std:c++17)*
   - **Advanced > Character Set** = *Use Unicode Character Set*
   - **C/C++ > General > Warning Level** = *Level4 (/W4)*
   - **C/C++ > Command Line > Additional Options**: `/utf-8`
   - **Linker > System > SubSystem** = *Windows (/SUBSYSTEM:WINDOWS)*
   - **Linker > Input > Additional Dependencies**: add `comctl32.lib;uxtheme.lib;` (the headers also add them via `#pragma comment`)
4. Build (**Ctrl+Shift+B**) and run (**Ctrl+F5**).
5. Copy the three files from `SampleData` into the folder containing the built `.exe` (e.g. `x64\Debug`) if you want demo data.

Notes: DPI awareness is enabled in code (`SetProcessDpiAwarenessContext`), and the Common Controls v6 manifest by a linker pragma in `main.cpp`, so **no manifest file is needed**.

## 3. Behaviour notes (things worth knowing for a demo)
- Data files live next to the `.exe` (Debug build: `x64\Debug\`). Missing files = empty data.
- Names are stored **trimmed**. IDs are **case-sensitive** (`s001` and `S001` are different IDs).
- Capacity must be a whole number from 1 to 100000 (the upper limit protects against overflow).
- When both rules apply, an enrollment is reported as *duplicate* before it is reported as *full*.
- In the Add dialogs an error shows **both** the error message box and the same text inline (red), then focuses the offending field; the dialog stays open.
- Status classes: **Open** < 80 %, **Almost full** >= 80 % and < 100 %, **Full** = 100 %.
- Click any list column header to sort (click again to reverse). Numbers inside text sort naturally (`S2` before `S10`).
- Unsaved changes: the title shows `*`; closing asks **Save changes?** (Yes / No / Cancel).

## 4. Sample data (SampleData\)

| Course | Capacity | Enrolled | Status |
|---|---|---|---|
| CS101 Introduction to Programming | 5 | 5 (S001-S005) | **Full** |
| CS201 Data Structures | 5 | 4 (S001-S004) | **Almost full** (80 %) |
| MA101 Calculus I | 30 | 2 (S001, S006) | Open |
| EN101 Academic Writing | 4 | 1 (S002) | Open |

Students: S001 Alice Johnson, S002 Brian Smith, S003 Chen Wei, S004 Diana Lopez, S005 Ethan Brown, S006 Fatima Khan.

## 5. Test checklist

Start from the sample data: copy `SampleData\*.txt` next to the `.exe` (delete any older data files first), then launch.
Tick each box as you demonstrate it.

### A. Startup and display
- [ ] **A1** Launch. *Expected:* window titled `Course Enrollment Management System` (no `*`), no warning box. Status bar: `Students: 6 | Courses: 4 | Enrollments: 12 | Loaded 6 students, 4 courses, 12 enrollments.`
- [ ] **A2** Resize the window and drag it to the minimum. *Expected:* it cannot go below about 1000 x 650; list views, buttons and group boxes follow the window.
- [ ] **A3** **Students** tab. *Expected:* 6 rows. Courses Taken: S001 = 3, S002 = 3, S003 = 2, S004 = 2, S005 = 1, S006 = 1.
- [ ] **A4** **Courses** tab (function 2). *Expected:* CS101 5 / 5 / 0 **Full** (red row); CS201 5 / 4 / 1 **Almost full** (yellow row); MA101 30 / 2 / 28 **Open** (green row); EN101 4 / 1 / 3 **Open** (green row).
- [ ] **A5** Click the column header **Capacity** twice. *Expected:* rows sort ascending then descending, with an arrow in the header.

### B. Add student / add course (functions 1, validation)
Students tab > **Add Student...**

| # | ID | Name | Expected |
|---|---|---|---|
| B1 | `S007` | `Gina Park` | Dialog closes; row appears; status `Student added: S007 (Gina Park).`; title shows `*`; status bar students = 7 |
| B2 | `S007` | `Someone` | Error box "Student ID 'S007' already exists..."; dialog stays open; focus in the ID box |
| B3 | *(empty)* | `Hank` | Error "Student ID must not be empty."; focus in ID |
| B4 | `S 008` | `Hank` | Error "Student ID must not contain spaces."; focus in ID |
| B5 | `S|008` | `Hank` | Error "Student ID must not contain the '|' character." |
| B6 | `S008` | *(empty or only spaces)* | Error "Student name must not be empty."; focus in Name |
| B7 | `S008` | `Ha|nk` | Error "Student name must not contain the '|' character."; focus in Name |
| B8 | `S008` | `Hank` then press **Esc** instead of OK | Dialog closes, nothing added |

Courses tab > **Add Course...** (the capacity box accepts digits only and has a spin control)

| # | ID | Name | Capacity | Expected |
|---|---|---|---|---|
| B9 | `PH101` | `Physics` | `20` (or use the spin arrows) | Added; row green "Open"; status updated |
| B10 | `PH101` | `Other` | `10` | Error "Course ID 'PH101' already exists..." |
| B11 | `S001` *(same as a student ID)* | `Shared ID` | `10` | **Accepted**: a student and a course may share an ID |
| B12 | `CH101` | `Chem` | `0` | Error "Capacity must be greater than zero."; focus in Capacity |
| B13 | `CH101` | `Chem` | clear the box | Error "Capacity must not be empty..." |
| B14 | `CH101` | `Chem` | type `-5` | The box ignores `-` (digits only). *Via file test, see F6, "negative" and "non-numeric" are rejected too* |
| B15 | `CH101` | `Chem` | `999999` | Error "Capacity must not exceed 100000." |
| B16 | *(empty)* / `C H` / `C|H` | `Chem` | `10` | The same three ID errors as B3 to B5, with "Course ID" |
| B17 | `CH101` | `Ch|em` or empty | `10` | Name errors as B6/B7 with "Course name" |

(Zero, negative and non-numeric capacity are also enforced in `EnrollmentSystem` itself, so they are rejected when read from a file too: see F6.)

### C. Search / filter
- [ ] **C1** Students tab, type `an` in the search box. *Expected:* the list filters as you type (matches ID or name, case-insensitive). Clear the box: all rows return.
- [ ] **C2** Courses tab, type `cs`. *Expected:* only CS101 and CS201 remain.

### D. Enrollment tab (functions 3 to 6)
Switch to the **Enrollment** tab. *Expected at first:* **Enroll** and **Withdraw Selected** are both disabled; the right label says "Select a course...".

- [ ] **D1** (function 5) Left "Student:" drop-down > `S001 - Alice Johnson`. *Expected:* list shows CS101, CS201, MA101 with seats `5 / 5`, `4 / 5`, `2 / 30`. **Withdraw Selected** still disabled (no row selected).
- [ ] **D2** (function 6) Right "Course:" drop-down > `CS201 ...`. *Expected:* label `Enrolled 4 / 5 — 1 place remaining`; progress bar 80 % in **yellow**; list shows S001 to S004. Choose `CS101`: `Enrolled 5 / 5 — 0 places remaining`, bar **red**. Choose `MA101`: `Enrolled 2 / 30 — 28 places remaining`, bar green.
- [ ] **D3** (function 3, success) Left "Student:" > `S006 - Fatima Khan`; lower "Course:" > `EN101 ...`; click **Enroll**. *Expected:* status bar `Enrolled S006 (Fatima Khan) in EN101 (Academic Writing). Places remaining: 2.`; counters update (enrollments 13 + any you added); title `*`; the student's list gains EN101; Courses tab EN101 shows 2 enrolled; Students tab S006 shows 2.
- [ ] **D4** (reject duplicate) Same student and course again, click **Enroll**. *Expected:* error box "Student S006 (Fatima Khan) is already enrolled in course EN101...".
- [ ] **D5** (reject over capacity) Student `S006`, course `CS101 ... (full)`, click **Enroll**. *Expected:* error box "Course CS101 (...) is full: all 5 places are taken."; nothing changes.
- [ ] **D6** (almost full becomes full) Student `S006`, course `CS201`, **Enroll**. *Expected:* success, then CS201 turns **red / Full** on the Courses tab and the right panel (if CS201 is selected there) shows `5 / 5` with a red bar.
- [ ] **D7** (function 4, withdraw) Student `S006`: select the `CS201` row, click **Withdraw Selected**. *Expected:* confirmation question "Withdraw Fatima Khan (S006) from Data Structures (CS201)?"; **No** keeps it; **Yes** removes it, status `Withdrew S006 (Fatima Khan) from CS201 (Data Structures).`, CS201 is back to **Almost full**; the button is disabled again.
- [ ] **D8** Pick a student or course in a drop-down and clear nothing: confirm that **Enroll** is only enabled when **both** the student and the course drop-downs have a selection.
- [ ] **D9** Add a new student on the Students tab, switch to the Enrollment tab. *Expected:* the new student is already in the "Student:" drop-down (all lists/drop-downs refresh immediately).

### E. Keyboard
- [ ] **E1** **Tab** / **Shift+Tab** move through the controls of the current page in a logical order (search box > button > list ..., or student drop-down > list > course drop-down > Enroll > Withdraw), then to the tab strip. **Ctrl+Tab** changes tabs. **Alt+S**, **Alt+E**, **Alt+W** use the underlined mnemonics.
- [ ] **E2** In an Add dialog: **Enter** presses OK, **Esc** cancels; the first field has focus; the capacity field also reacts to the **Up/Down arrow keys**.
- [ ] **E3** **Ctrl+S** saves (see F1).

### F. Persistence
- [ ] **F1** After some changes, press **Ctrl+S** (or File > Save). *Expected:* `*` disappears from the title; status `Saved to <folder>`. Open `students.txt`, `courses.txt`, `enrollments.txt` next to the `.exe` in Notepad: formats `id|name`, `id|name|capacity`, `studentId|courseId`.
- [ ] **F2** Make a change (title shows `*`), then **File > Exit**. *Expected:* "You have unsaved changes. Save changes?" with Yes / No / Cancel. **Cancel** = stays open. **No** = closes without saving. Repeat and choose **Yes** = saves and closes.
- [ ] **F3** Start the program again. *Expected:* the saved data comes back automatically.
- [ ] **F4** Close the program, delete the three `.txt` files, start it. *Expected:* empty lists, no error.
- [ ] **F5** UTF-8: add a student with name `José Ñandú 李雷`, save, restart. *Expected:* the name is displayed correctly.
- [ ] **F6** Invalid-file test: close the program and replace the three data files with the ones in `SampleData\InvalidTest\`, then launch. *Expected:* **one** warning box (exclamation icon) listing 13 skipped lines, among them:
  - students.txt: line 2 duplicate ID `S001`; line 3 ID `S003 ` contains a space; line 4 empty name; line 5 malformed (3 fields)
  - courses.txt: line 2 duplicate `CS101`; line 3 capacity `0`; line 4 capacity `abc`; line 5 capacity `-4`
  - enrollments.txt: line 2 duplicate; line 3 course `CS101` full (capacity 1); line 4 student `S999` does not exist; line 5 course `XX999` does not exist; line 6 malformed

  Afterwards the app shows 2 students (S001, S006), 1 course (CS101, capacity 1) and 1 enrollment (S001 in CS101).
- [ ] **F7** Close the program, delete the data files and restore the originals from `SampleData\` before the next demo.

### G. Miscellaneous
- [ ] **G1** **Help > About** opens a small dialog; OK / Esc closes it.
- [ ] **G2** DPI: move the window between monitors of different scaling, or change Windows scaling to 150 %. *Expected:* text and controls stay sharp and proportionally sized (Segoe UI 9 pt scaled).
- [ ] **G3** Status bar format: `Students: N | Courses: N | Enrollments: N | last action` (four panes). Errors show a message box; successes only update the status bar.
