// FileManager.cpp - persistence of the three UTF-8 text files.
#include "FileManager.h"

#include <fstream>
#include <iterator>
#include <system_error>

#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>

namespace fs = std::filesystem;

namespace {

constexpr wchar_t kStudentsFile[] = L"students.txt";
constexpr wchar_t kCoursesFile[] = L"courses.txt";
constexpr wchar_t kEnrollmentsFile[] = L"enrollments.txt";

/// One non-blank line of a data file together with its 1-based line number.
struct FileLine {
    size_t number;
    std::wstring text;
};

/// Splits "a|b|c" into {"a","b","c"}. A line without '|' yields one field.
std::vector<std::wstring> SplitFields(const std::wstring& line)
{
    std::vector<std::wstring> fields;
    size_t start = 0;
    for (;;) {
        const size_t bar = line.find(L'|', start);
        if (bar == std::wstring::npos) {
            fields.push_back(line.substr(start));
            break;
        }
        fields.push_back(line.substr(start, bar - start));
        start = bar + 1;
    }
    return fields;
}

/// Reads a whole file and returns its non-blank lines.
/// A missing file is not an error (returns nothing).
std::vector<FileLine> ReadLines(const fs::path& path, const std::wstring& label, LoadReport& report)
{
    std::vector<FileLine> lines;

    std::error_code ec;
    if (!fs::exists(path, ec)) return lines;

    std::ifstream in(path, std::ios::binary);
    if (!in) {
        report.warnings.push_back(label + L": the file exists but could not be opened.");
        return lines;
    }
    std::string bytes((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());

    // Strip an optional UTF-8 byte order mark (Notepad may add one).
    if (bytes.size() >= 3 && static_cast<unsigned char>(bytes[0]) == 0xEF &&
        static_cast<unsigned char>(bytes[1]) == 0xBB && static_cast<unsigned char>(bytes[2]) == 0xBF) {
        bytes.erase(0, 3);
    }

    bool valid = true;
    const std::wstring text = FileManager::Utf8ToWide(bytes, &valid);
    if (!valid)
        report.warnings.push_back(label + L": contains invalid UTF-8; some characters were replaced.");

    size_t start = 0;
    size_t number = 1;
    while (start <= text.size()) {
        size_t end = text.find(L'\n', start);
        if (end == std::wstring::npos) end = text.size();

        std::wstring line = text.substr(start, end - start);
        if (!line.empty() && line.back() == L'\r') line.pop_back();
        if (line.find_first_not_of(L" \t") != std::wstring::npos)  // skip blank lines
            lines.push_back(FileLine{number, line});

        ++number;
        start = end + 1;
    }
    return lines;
}

/// Records a skipped line in the report.
void Skip(LoadReport& report, const std::wstring& file, size_t lineNumber, const std::wstring& reason)
{
    report.warnings.push_back(file + L", line " + std::to_wstring(lineNumber) + L": " + reason);
}

/// Writes @p bytes to a temp file and then moves it over @p path, so a crash
/// while saving can never leave a half-written data file behind.
OperationResult WriteFileAtomically(const fs::path& path, const std::string& bytes)
{
    fs::path temp = path;
    temp += L".tmp";

    {
        std::ofstream out(temp, std::ios::binary | std::ios::trunc);
        if (!out) return OperationResult::Fail(L"Could not create the file:\n" + temp.wstring());
        out.write(bytes.data(), static_cast<std::streamsize>(bytes.size()));
        out.flush();
        if (!out) return OperationResult::Fail(L"Could not write the file:\n" + temp.wstring());
    }  // stream closed here (RAII)

    if (!MoveFileExW(temp.c_str(), path.c_str(), MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH)) {
        const DWORD error = GetLastError();
        DeleteFileW(temp.c_str());
        return OperationResult::Fail(L"Could not replace the file:\n" + path.wstring() + L"\n(Windows error " +
                                     std::to_wstring(error) + L")");
    }
    return OperationResult::Ok(L"");
}

}  // namespace

// ---------------------------------------------------------------------------
// LoadReport
// ---------------------------------------------------------------------------

std::wstring LoadReport::Summary(size_t maxLines) const
{
    std::wstring text = L"Some data in the files could not be loaded and was skipped:\n\n";
    size_t shown = 0;
    for (const std::wstring& warning : warnings) {
        if (shown == maxLines) break;
        text += L"• " + warning + L"\n";
        ++shown;
    }
    if (warnings.size() > shown)
        text += L"\n...and " + std::to_wstring(warnings.size() - shown) + L" more.";
    return text;
}

// ---------------------------------------------------------------------------
// FileManager
// ---------------------------------------------------------------------------

FileManager::FileManager(fs::path directory) : directory_(std::move(directory)) {}

fs::path FileManager::ExecutableDirectory()
{
    std::vector<wchar_t> buffer(MAX_PATH);
    for (;;) {
        const DWORD length = GetModuleFileNameW(nullptr, buffer.data(), static_cast<DWORD>(buffer.size()));
        if (length == 0) return fs::current_path();  // very unlikely; fall back
        if (length < buffer.size()) return fs::path(buffer.data()).parent_path();
        buffer.resize(buffer.size() * 2);  // path was truncated: retry with a bigger buffer
    }
}

LoadReport FileManager::Load(EnrollmentSystem& system) const
{
    LoadReport report;
    system.Clear();

    // 1. Students: id|name
    for (const FileLine& line : ReadLines(directory_ / kStudentsFile, kStudentsFile, report)) {
        const std::vector<std::wstring> fields = SplitFields(line.text);
        if (fields.size() != 2) {
            Skip(report, kStudentsFile, line.number, L"malformed line (expected: id|name).");
            continue;
        }
        const OperationResult result = system.AddStudent(fields[0], fields[1]);
        if (result) ++report.studentsLoaded;
        else Skip(report, kStudentsFile, line.number, result.message);
    }

    // 2. Courses: id|name|capacity
    for (const FileLine& line : ReadLines(directory_ / kCoursesFile, kCoursesFile, report)) {
        const std::vector<std::wstring> fields = SplitFields(line.text);
        if (fields.size() != 3) {
            Skip(report, kCoursesFile, line.number, L"malformed line (expected: id|name|capacity).");
            continue;
        }
        const OperationResult result = system.AddCourse(fields[0], fields[1], fields[2]);
        if (result) ++report.coursesLoaded;
        else Skip(report, kCoursesFile, line.number, result.message);
    }

    // 3. Enrollments: studentId|courseId (students and courses must already be loaded)
    for (const FileLine& line : ReadLines(directory_ / kEnrollmentsFile, kEnrollmentsFile, report)) {
        const std::vector<std::wstring> fields = SplitFields(line.text);
        if (fields.size() != 2) {
            Skip(report, kEnrollmentsFile, line.number, L"malformed line (expected: studentId|courseId).");
            continue;
        }
        const OperationResult result = system.Enroll(fields[0], fields[1]);
        if (result) ++report.enrollmentsLoaded;
        else Skip(report, kEnrollmentsFile, line.number, result.message);
    }

    system.MarkSaved();  // what is in memory now matches what was loaded
    return report;
}

OperationResult FileManager::Save(const EnrollmentSystem& system) const
{
    std::wstring students;
    for (const Student& s : system.Students()) students += s.Id() + L"|" + s.Name() + L"\r\n";

    std::wstring courses;
    for (const Course& c : system.Courses())
        courses += c.Id() + L"|" + c.Name() + L"|" + std::to_wstring(c.Capacity()) + L"\r\n";

    std::wstring enrollments;
    for (const Enrollment& e : system.Enrollments()) enrollments += e.StudentId() + L"|" + e.CourseId() + L"\r\n";

    std::error_code ec;
    fs::create_directories(directory_, ec);  // normally exists already

    OperationResult result = WriteFileAtomically(directory_ / kStudentsFile, WideToUtf8(students));
    if (!result) return result;
    result = WriteFileAtomically(directory_ / kCoursesFile, WideToUtf8(courses));
    if (!result) return result;
    result = WriteFileAtomically(directory_ / kEnrollmentsFile, WideToUtf8(enrollments));
    if (!result) return result;

    return OperationResult::Ok(L"Saved to " + directory_.wstring());
}

std::wstring FileManager::Utf8ToWide(const std::string& utf8, bool* valid)
{
    if (valid != nullptr) *valid = true;
    if (utf8.empty()) return std::wstring();

    const int inputLength = static_cast<int>(utf8.size());
    DWORD flags = MB_ERR_INVALID_CHARS;  // strict first, so we can detect bad input
    int length = MultiByteToWideChar(CP_UTF8, flags, utf8.data(), inputLength, nullptr, 0);
    if (length == 0) {
        if (valid != nullptr) *valid = false;
        flags = 0;  // lenient retry: invalid bytes become U+FFFD
        length = MultiByteToWideChar(CP_UTF8, flags, utf8.data(), inputLength, nullptr, 0);
        if (length == 0) return std::wstring();
    }

    std::wstring wide(static_cast<size_t>(length), L'\0');
    MultiByteToWideChar(CP_UTF8, flags, utf8.data(), inputLength, wide.data(), length);
    return wide;
}

std::string FileManager::WideToUtf8(const std::wstring& wide)
{
    if (wide.empty()) return std::string();

    const int inputLength = static_cast<int>(wide.size());
    const int length = WideCharToMultiByte(CP_UTF8, 0, wide.data(), inputLength, nullptr, 0, nullptr, nullptr);
    if (length == 0) return std::string();

    std::string utf8(static_cast<size_t>(length), '\0');
    WideCharToMultiByte(CP_UTF8, 0, wide.data(), inputLength, utf8.data(), length, nullptr, nullptr);
    return utf8;
}
