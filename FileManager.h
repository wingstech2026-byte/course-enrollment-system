// FileManager.h - loads/saves the three UTF-8 text files next to the .exe.
//
//   students.txt     id|name
//   courses.txt      id|name|capacity
//   enrollments.txt  studentId|courseId
//
// Loading validates every line with the same rules as manual entry by calling
// EnrollmentSystem's own methods, so nothing invalid can ever enter the system.
#pragma once

#include <filesystem>
#include <string>
#include <vector>

#include "EnrollmentSystem.h"

/// What happened while loading: counts plus one warning per skipped line.
struct LoadReport {
    std::vector<std::wstring> warnings;
    int studentsLoaded = 0;
    int coursesLoaded = 0;
    int enrollmentsLoaded = 0;

    /// Builds one text listing at most @p maxLines warnings (for a single message box).
    std::wstring Summary(size_t maxLines) const;
};

class FileManager {
public:
    /// Files are read from / written to @p directory.
    explicit FileManager(std::filesystem::path directory);

    /// Directory that contains the running .exe.
    static std::filesystem::path ExecutableDirectory();

    const std::filesystem::path& Directory() const noexcept { return directory_; }

    /// Replaces the contents of @p system with the data on disk.
    /// Missing files simply mean "no data". Bad lines are skipped and reported.
    LoadReport Load(EnrollmentSystem& system) const;

    /// Writes all three files (each one atomically via a temp file + rename).
    OperationResult Save(const EnrollmentSystem& system) const;

    /// UTF-8 <-> UTF-16 conversion helpers. @p valid (optional) is set to false
    /// when the input was not well-formed UTF-8 (bad bytes become U+FFFD).
    static std::wstring Utf8ToWide(const std::string& utf8, bool* valid = nullptr);
    static std::string WideToUtf8(const std::wstring& wide);

private:
    std::filesystem::path directory_;
};
