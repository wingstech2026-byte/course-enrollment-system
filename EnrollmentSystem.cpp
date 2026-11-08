// EnrollmentSystem.cpp - business logic (no GUI code in this file).
#include "EnrollmentSystem.h"

#include <algorithm>
#include <cwctype>

namespace {

/// Removes leading and trailing whitespace.
std::wstring Trim(const std::wstring& text)
{
    size_t first = 0;
    size_t last = text.size();
    while (first < last && std::iswspace(text[first])) ++first;
    while (last > first && std::iswspace(text[last - 1])) --last;
    return text.substr(first, last - first);
}

/// Validates the syntax of an ID (not uniqueness).
/// @p label is e.g. L"Student ID" and is used to build the message.
OperationResult ValidateId(const std::wstring& id, const std::wstring& label)
{
    if (id.empty())
        return OperationResult::Fail(label + L" must not be empty.", InputField::Id);

    for (const wchar_t ch : id) {
        if (std::iswspace(ch))
            return OperationResult::Fail(label + L" must not contain spaces.", InputField::Id);
        if (ch == L'|')
            return OperationResult::Fail(label + L" must not contain the '|' character.", InputField::Id);
        if (std::iswcntrl(ch))
            return OperationResult::Fail(label + L" must not contain control characters.", InputField::Id);
    }
    return OperationResult::Ok(L"");
}

/// Validates a name. On success @p cleaned receives the trimmed name.
OperationResult ValidateName(const std::wstring& name, const std::wstring& label, std::wstring& cleaned)
{
    cleaned = Trim(name);
    if (cleaned.empty())
        return OperationResult::Fail(label + L" must not be empty.", InputField::Name);

    for (const wchar_t ch : cleaned) {
        if (ch == L'|')
            return OperationResult::Fail(label + L" must not contain the '|' character.", InputField::Name);
        if (std::iswcntrl(ch))
            return OperationResult::Fail(label + L" must not contain control characters.", InputField::Name);
    }
    return OperationResult::Ok(L"");
}

/// Parses capacity text. Only a positive whole number (1..kMaxCapacity) passes.
OperationResult ParseCapacity(const std::wstring& rawText, int& capacity)
{
    capacity = 0;
    const std::wstring text = Trim(rawText);

    if (text.empty())
        return OperationResult::Fail(L"Capacity must not be empty. Enter a positive whole number.",
                                     InputField::Capacity);

    size_t pos = 0;
    bool negative = false;
    if (text[0] == L'-') {
        negative = true;
        pos = 1;
    }
    if (pos >= text.size())
        return OperationResult::Fail(L"Capacity must be a whole number (digits only).", InputField::Capacity);

    long long value = 0;
    bool tooBig = false;
    for (size_t i = pos; i < text.size(); ++i) {
        const wchar_t ch = text[i];
        if (ch < L'0' || ch > L'9')
            return OperationResult::Fail(L"Capacity must be a whole number (digits only).",
                                         InputField::Capacity);
        if (!tooBig) {
            value = value * 10 + (ch - L'0');
            if (value > EnrollmentSystem::kMaxCapacity) tooBig = true;  // stop accumulating: no overflow
        }
    }

    if (negative)
        return OperationResult::Fail(L"Capacity must not be negative. Enter a positive whole number.",
                                     InputField::Capacity);
    if (tooBig)
        return OperationResult::Fail(L"Capacity must not exceed " + std::to_wstring(EnrollmentSystem::kMaxCapacity) + L".",
                                     InputField::Capacity);
    if (value == 0)
        return OperationResult::Fail(L"Capacity must be greater than zero.", InputField::Capacity);

    capacity = static_cast<int>(value);
    return OperationResult::Ok(L"");
}

/// "S001 (Alice)" style description used in messages.
std::wstring Describe(const std::wstring& id, const std::wstring& name)
{
    return id + L" (" + name + L")";
}

}  // namespace

// ---------------------------------------------------------------------------
// Mutating operations
// ---------------------------------------------------------------------------

OperationResult EnrollmentSystem::AddStudent(const std::wstring& id, const std::wstring& name)
{
    OperationResult check = ValidateId(id, L"Student ID");
    if (!check) return check;

    std::wstring cleanName;
    check = ValidateName(name, L"Student name", cleanName);
    if (!check) return check;

    if (FindStudent(id) != nullptr)
        return OperationResult::Fail(L"Student ID '" + id + L"' already exists. Student IDs must be unique.",
                                     InputField::Id);

    students_.emplace_back(id, cleanName);
    modified_ = true;
    return OperationResult::Ok(L"Student added: " + Describe(id, cleanName) + L".");
}

OperationResult EnrollmentSystem::AddCourse(const std::wstring& id, const std::wstring& name,
                                            const std::wstring& capacityText)
{
    OperationResult check = ValidateId(id, L"Course ID");
    if (!check) return check;

    std::wstring cleanName;
    check = ValidateName(name, L"Course name", cleanName);
    if (!check) return check;

    int capacity = 0;
    check = ParseCapacity(capacityText, capacity);
    if (!check) return check;

    if (FindCourse(id) != nullptr)
        return OperationResult::Fail(L"Course ID '" + id + L"' already exists. Course IDs must be unique.",
                                     InputField::Id);

    courses_.emplace_back(id, cleanName, capacity);
    modified_ = true;
    return OperationResult::Ok(L"Course added: " + Describe(id, cleanName) + L", capacity " +
                               std::to_wstring(capacity) + L".");
}

OperationResult EnrollmentSystem::Enroll(const std::wstring& studentId, const std::wstring& courseId)
{
    const Student* student = FindStudent(studentId);
    if (student == nullptr)
        return OperationResult::Fail(L"Student '" + studentId + L"' does not exist.");

    const Course* course = FindCourse(courseId);
    if (course == nullptr)
        return OperationResult::Fail(L"Course '" + courseId + L"' does not exist.");

    if (IsEnrolled(studentId, courseId))
        return OperationResult::Fail(L"Student " + Describe(student->Id(), student->Name()) +
                                     L" is already enrolled in course " +
                                     Describe(course->Id(), course->Name()) + L".");

    if (EnrolledCount(courseId) >= course->Capacity())
        return OperationResult::Fail(L"Course " + Describe(course->Id(), course->Name()) +
                                     L" is full: all " + std::to_wstring(course->Capacity()) +
                                     (course->Capacity() == 1 ? L" place is taken." : L" places are taken."));

    enrollments_.emplace_back(studentId, courseId);
    modified_ = true;
    return OperationResult::Ok(L"Enrolled " + Describe(student->Id(), student->Name()) + L" in " +
                               Describe(course->Id(), course->Name()) + L". Places remaining: " +
                               std::to_wstring(RemainingPlaces(courseId)) + L".");
}

OperationResult EnrollmentSystem::Withdraw(const std::wstring& studentId, const std::wstring& courseId)
{
    const Student* student = FindStudent(studentId);
    if (student == nullptr)
        return OperationResult::Fail(L"Student '" + studentId + L"' does not exist.");

    const Course* course = FindCourse(courseId);
    if (course == nullptr)
        return OperationResult::Fail(L"Course '" + courseId + L"' does not exist.");

    const auto it = std::find_if(enrollments_.begin(), enrollments_.end(), [&](const Enrollment& e) {
        return e.StudentId() == studentId && e.CourseId() == courseId;
    });
    if (it == enrollments_.end())
        return OperationResult::Fail(L"Student " + Describe(student->Id(), student->Name()) +
                                     L" is not enrolled in course " +
                                     Describe(course->Id(), course->Name()) + L".");

    // Build the message before the iterator/strings are invalidated.
    const std::wstring message = L"Withdrew " + Describe(student->Id(), student->Name()) + L" from " +
                                 Describe(course->Id(), course->Name()) + L".";
    enrollments_.erase(it);
    modified_ = true;
    return OperationResult::Ok(message);
}

void EnrollmentSystem::Clear()
{
    students_.clear();
    courses_.clear();
    enrollments_.clear();
    modified_ = false;
}

// ---------------------------------------------------------------------------
// Queries
// ---------------------------------------------------------------------------

const Student* EnrollmentSystem::FindStudent(const std::wstring& id) const
{
    const auto it = std::find_if(students_.begin(), students_.end(),
                                 [&](const Student& s) { return s.Id() == id; });
    return it == students_.end() ? nullptr : &*it;
}

const Course* EnrollmentSystem::FindCourse(const std::wstring& id) const
{
    const auto it = std::find_if(courses_.begin(), courses_.end(),
                                 [&](const Course& c) { return c.Id() == id; });
    return it == courses_.end() ? nullptr : &*it;
}

bool EnrollmentSystem::IsEnrolled(const std::wstring& studentId, const std::wstring& courseId) const
{
    return std::any_of(enrollments_.begin(), enrollments_.end(), [&](const Enrollment& e) {
        return e.StudentId() == studentId && e.CourseId() == courseId;
    });
}

int EnrollmentSystem::EnrolledCount(const std::wstring& courseId) const
{
    return static_cast<int>(std::count_if(enrollments_.begin(), enrollments_.end(),
                                          [&](const Enrollment& e) { return e.CourseId() == courseId; }));
}

int EnrollmentSystem::RemainingPlaces(const std::wstring& courseId) const
{
    const Course* course = FindCourse(courseId);
    if (course == nullptr) return 0;
    return (std::max)(0, course->Capacity() - EnrolledCount(courseId));
}

int EnrollmentSystem::CourseCountOfStudent(const std::wstring& studentId) const
{
    return static_cast<int>(std::count_if(enrollments_.begin(), enrollments_.end(),
                                          [&](const Enrollment& e) { return e.StudentId() == studentId; }));
}

std::vector<const Course*> EnrollmentSystem::CoursesOfStudent(const std::wstring& studentId) const
{
    std::vector<const Course*> result;
    for (const Enrollment& e : enrollments_) {
        if (e.StudentId() != studentId) continue;
        if (const Course* course = FindCourse(e.CourseId())) result.push_back(course);
    }
    return result;
}

std::vector<const Student*> EnrollmentSystem::StudentsInCourse(const std::wstring& courseId) const
{
    std::vector<const Student*> result;
    for (const Enrollment& e : enrollments_) {
        if (e.CourseId() != courseId) continue;
        if (const Student* student = FindStudent(e.StudentId())) result.push_back(student);
    }
    return result;
}
