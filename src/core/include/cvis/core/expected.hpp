#pragma once

/**
 * @file expected.hpp
 * @brief Vendored `Expected<T>` result type (value-or-Status) for C++20.
 * @ingroup cvis_core
 */

#include <utility>
#include <variant>

#include "cvis/core/status.hpp"

namespace cvis::core {

/**
 * @brief Minimal, move-friendly result type: holds either a value of type T or
 *        a Status describing a failure.
 *
 * Vendored because `std::expected` requires C++23 and this project targets
 * C++20 (design interfaces §1). Behaviour mirrors the subset of `std::expected`
 * used across module boundaries.
 *
 * @tparam T Success value type. Must not be Status (the error type) so the two
 *           constructors stay unambiguous; every use site satisfies this.
 * @ingroup cvis_core
 */
template <class T>
class Expected {
public:
    /// @brief Construct holding a success @p value.
    Expected(T value) : slot_(std::in_place_index<0>, std::move(value)) {}
    /// @brief Construct holding a failure @p status.
    Expected(Status status) : slot_(std::in_place_index<1>, std::move(status)) {}

    Expected(const Expected&) = default;
    Expected(Expected&&) noexcept = default;
    Expected& operator=(const Expected&) = default;
    Expected& operator=(Expected&&) noexcept = default;

    /// @brief True when a success value is held.
    [[nodiscard]] bool has_value() const noexcept { return slot_.index() == 0; }
    /// @brief Same as has_value(); enables `if (expected)` checks.
    explicit operator bool() const noexcept { return has_value(); }

    /// @brief Access the success value. @pre has_value() is true.
    T& value() & { return std::get<0>(slot_); }
    const T& value() const& { return std::get<0>(slot_); }
    T&& value() && { return std::get<0>(std::move(slot_)); }

    T& operator*() & { return std::get<0>(slot_); }
    const T& operator*() const& { return std::get<0>(slot_); }
    T&& operator*() && { return std::get<0>(std::move(slot_)); }

    T* operator->() { return &std::get<0>(slot_); }
    const T* operator->() const { return &std::get<0>(slot_); }

    /// @brief Access the failure status. @pre has_value() is false.
    [[nodiscard]] const Status& error() const& { return std::get<1>(slot_); }
    [[nodiscard]] Status&& error() && { return std::get<1>(std::move(slot_)); }

private:
    std::variant<T, Status> slot_;
};

} // namespace cvis::core
