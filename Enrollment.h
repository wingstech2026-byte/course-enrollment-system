// Enrollment.h - simple data class linking one student to one course.
#pragma once

#include <string>
#include <utility>

/// One enrollment record: "student X is taking course Y".
class Enrollment {
public:
    Enrollment(std::wstring studentId, std::wstring courseId)
        : studentId_(std::move(studentId)), courseId_(std::move(courseId)) {}

    const std::wstring& StudentId() const noexcept { return studentId_; }
    const std::wstring& CourseId() const noexcept { return courseId_; }

private:
    std::wstring studentId_;
    std::wstring courseId_;
};
