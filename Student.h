// Student.h - simple data class describing one student.
#pragma once

#include <string>
#include <utility>

/// A student registered in the system. Immutable value type.
class Student {
public:
    Student(std::wstring id, std::wstring name)
        : id_(std::move(id)), name_(std::move(name)) {}

    const std::wstring& Id() const noexcept { return id_; }
    const std::wstring& Name() const noexcept { return name_; }

private:
    std::wstring id_;    // unique among students, no spaces, no '|'
    std::wstring name_;  // display name, no '|'
};
