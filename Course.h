// Course.h - simple data class describing one course.
#pragma once

#include <string>
#include <utility>

/// A course offered by the university. Immutable value type.
class Course {
public:
    Course(std::wstring id, std::wstring name, int capacity)
        : id_(std::move(id)), name_(std::move(name)), capacity_(capacity) {}

    const std::wstring& Id() const noexcept { return id_; }
    const std::wstring& Name() const noexcept { return name_; }
    int Capacity() const noexcept { return capacity_; }

private:
    std::wstring id_;    // unique among courses, no spaces, no '|'
    std::wstring name_;  // display name, no '|'
    int capacity_;       // maximum number of enrolled students (> 0)
};
