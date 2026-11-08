// EnrollmentSystem.h - owns all data and implements every business rule.
//
// This class (and its .cpp) contains NO GUI code. Every mutating method
// validates its input and returns an OperationResult (success flag + a
// human-readable message), so the rules are enforced no matter which front
// end (dialogs, drop-downs, file loader) calls them.
#pragma once

#include <string>
#include <utility>
#include <vector>

#include "Course.h"
#include "Enrollment.h"
#include "Student.h"

/// Which input field a failed validation refers to (lets a dialog focus it).
enum class InputField { None, Id, Name, Capacity };

/// Outcome of an operation: success flag + message for the user.
struct OperationResult {
    bool success = false;
    std::wstring message;
    InputField field = InputField::None;

    explicit operator bool() const noexcept { return success; }

    static OperationResult Ok(std::wstring text) {
        OperationResult r;
        r.success = true;
        r.message = std::move(text);
        return r;
    }
    static OperationResult Fail(std::wstring text, InputField where = InputField::None) {
        OperationResult r;
        r.success = false;
        r.message = std::move(text);
        r.field = where;
        return r;
    }
};

class EnrollmentSystem {
public:
    /// Upper bound for a course capacity (also protects against overflow).
    static constexpr int kMaxCapacity = 100000;

    // ---- Mutating operations (all validate and return a result) ----------

    /// Adds a student. Rejects empty/duplicate IDs, IDs with spaces or '|',
    /// empty names and names with '|'. The name is stored trimmed.
    OperationResult AddStudent(const std::wstring& id, const std::wstring& name);

    /// Adds a course. @p capacityText is the raw text typed by the user; it
    /// must be a positive whole number (so "0", "-3", "abc", "2.5" are rejected).
    OperationResult AddCourse(const std::wstring& id, const std::wstring& name,
                              const std::wstring& capacityText);

    /// Enrolls a student in a course. Rejects unknown student/course, duplicate
    /// enrollment and enrollment beyond the course capacity.
    OperationResult Enroll(const std::wstring& studentId, const std::wstring& courseId);

    /// Removes the matching enrollment record.
    OperationResult Withdraw(const std::wstring& studentId, const std::wstring& courseId);

    /// Removes all data (used before loading from disk). Resets the modified flag.
    void Clear();

    // ---- Queries ----------------------------------------------------------
    // Pointers returned below point into internal vectors: use them right
    // away and do not keep them across a call to a mutating method.

    const std::vector<Student>& Students() const noexcept { return students_; }
    const std::vector<Course>& Courses() const noexcept { return courses_; }
    const std::vector<Enrollment>& Enrollments() const noexcept { return enrollments_; }

    const Student* FindStudent(const std::wstring& id) const;
    const Course* FindCourse(const std::wstring& id) const;

    bool IsEnrolled(const std::wstring& studentId, const std::wstring& courseId) const;
    int EnrolledCount(const std::wstring& courseId) const;
    int RemainingPlaces(const std::wstring& courseId) const;
    int CourseCountOfStudent(const std::wstring& studentId) const;

    /// Courses selected by a student (function 5), in enrollment order.
    std::vector<const Course*> CoursesOfStudent(const std::wstring& studentId) const;
    /// Students enrolled in a course (function 6), in enrollment order.
    std::vector<const Student*> StudentsInCourse(const std::wstring& courseId) const;

    // ---- Unsaved-changes tracking ----------------------------------------
    bool IsModified() const noexcept { return modified_; }
    void MarkSaved() noexcept { modified_ = false; }

private:
    std::vector<Student> students_;
    std::vector<Course> courses_;
    std::vector<Enrollment> enrollments_;
    bool modified_ = false;
};
