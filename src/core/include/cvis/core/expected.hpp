#pragma once

#include <utility>
#include <variant>

#include "cvis/core/status.hpp"

namespace cvis::core {

/// Minimal, move-friendly result type: holds either a value of type T or a
/// Status describing a failure. Vendored because std::expected requires C++23
/// and this project targets C++20 (design interfaces §1). Behaviour mirrors the
/// subset of std::expected we use across module boundaries.
///
/// T must not be Status (the error type) so the two constructors are
/// unambiguous; every use site in the project satisfies this.
template <class T>
class Expected {
public:
    Expected(T value) : slot_(std::in_place_index<0>, std::move(value)) {}
    Expected(Status status) : slot_(std::in_place_index<1>, std::move(status)) {}

    Expected(const Expected&) = default;
    Expected(Expected&&) noexcept = default;
    Expected& operator=(const Expected&) = default;
    Expected& operator=(Expected&&) noexcept = default;

    [[nodiscard]] bool has_value() const noexcept { return slot_.index() == 0; }
    explicit operator bool() const noexcept { return has_value(); }

    T& value() & { return std::get<0>(slot_); }
    const T& value() const& { return std::get<0>(slot_); }
    T&& value() && { return std::get<0>(std::move(slot_)); }

    T& operator*() & { return std::get<0>(slot_); }
    const T& operator*() const& { return std::get<0>(slot_); }
    T&& operator*() && { return std::get<0>(std::move(slot_)); }

    T* operator->() { return &std::get<0>(slot_); }
    const T* operator->() const { return &std::get<0>(slot_); }

    [[nodiscard]] const Status& error() const& { return std::get<1>(slot_); }
    [[nodiscard]] Status&& error() && { return std::get<1>(std::move(slot_)); }

private:
    std::variant<T, Status> slot_;
};

} // namespace cvis::core
