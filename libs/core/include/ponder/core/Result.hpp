#pragma once

#include <ponder/core/StackTrace.hpp>

#include <concepts>
#include <cstdint>
#include <expected>
#include <memory>
#include <source_location>
#include <string>
#include <string_view>
#include <type_traits>
#include <utility>

namespace pond::core
{
enum class ErrorCategory : std::uint8_t
{
    General,
    InvalidArgument,
    NotFound,
    Io,
    Parse,
    Unsupported,
    Internal
};

using ErrorCodeValue = std::int32_t;

class ErrorCode final
{
public:
    constexpr ErrorCode() noexcept = default;
    constexpr ErrorCode(ErrorCategory category, ErrorCodeValue value) noexcept
        : m_category(category), m_value(value)
    {
    }

    [[nodiscard]] constexpr ErrorCategory GetCategory() const noexcept
    {
        return m_category;
    }

    [[nodiscard]] constexpr ErrorCodeValue GetValue() const noexcept
    {
        return m_value;
    }

    [[nodiscard]] friend constexpr bool operator==(const ErrorCode& lhs,
                                                   const ErrorCode& rhs) noexcept = default;

private:
    ErrorCategory m_category{ErrorCategory::General};
    ErrorCodeValue m_value{0};
};

class Error final
{
public:
    explicit Error(std::string message,
                   std::source_location location = std::source_location::current());
    Error(ErrorCode code, std::string message,
          std::source_location location = std::source_location::current());
    Error(ErrorCode code, std::string message, StackTrace stackTrace,
          std::source_location location = std::source_location::current());

    [[nodiscard]] ErrorCode GetCode() const noexcept;
    [[nodiscard]] std::string_view GetMessage() const noexcept;
    [[nodiscard]] const std::source_location& GetLocation() const noexcept;
    [[nodiscard]] const StackTrace& GetStackTrace() const noexcept;

private:
    ErrorCode m_code;
    std::string m_message;
    std::source_location m_location;
    StackTrace m_stackTrace;
};

[[nodiscard]] constexpr std::string_view GetErrorCategoryName(ErrorCategory category) noexcept
{
    switch (category)
    {
    case ErrorCategory::General:
        return "general";
    case ErrorCategory::InvalidArgument:
        return "invalid_argument";
    case ErrorCategory::NotFound:
        return "not_found";
    case ErrorCategory::Io:
        return "io";
    case ErrorCategory::Parse:
        return "parse";
    case ErrorCategory::Unsupported:
        return "unsupported";
    case ErrorCategory::Internal:
        return "internal";
    }

    return "unknown";
}

[[nodiscard]] std::string FormatErrorCode(ErrorCode code);
[[nodiscard]] std::string FormatError(const Error& error);

namespace detail
{
[[noreturn]] consteval void RejectConstexprErrorConstruction()
{
    throw "pond::core::Error is runtime-only and cannot be constructed during constant evaluation.";
}
} // namespace detail

template <typename Value>
class [[nodiscard]] Result final
{
public:
    using ValueType = Value;
    using ErrorType = Error;
    using ExpectedType = std::expected<ValueType, ErrorType>;

    constexpr Result()
        requires std::default_initializable<ValueType>
    = default;
    constexpr Result(const Result&) = default;
    constexpr Result(Result&&) noexcept(std::is_nothrow_move_constructible_v<ExpectedType>) =
        default;
    constexpr Result& operator=(const Result&) = default;
    constexpr Result& operator=(Result&&) noexcept(
        std::is_nothrow_move_assignable_v<ExpectedType>) = default;
    ~Result() = default;

    template <typename Input>
        requires(!std::same_as<std::remove_cvref_t<Input>, Result> &&
                 !std::same_as<std::remove_cvref_t<Input>, std::unexpected<ErrorType>> &&
                 std::constructible_from<ValueType, Input &&>)
    constexpr Result(Input&& value) noexcept(std::is_nothrow_constructible_v<ValueType, Input&&>)
        : m_expected(std::in_place, std::forward<Input>(value))
    {
    }

    constexpr Result(std::unexpected<ErrorType> unexpected) noexcept(
        std::is_nothrow_constructible_v<ExpectedType, std::unexpected<ErrorType>&&>)
        : m_expected(std::move(unexpected))
    {
    }

    template <typename... Args>
        requires std::constructible_from<ValueType, Args&&...>
    [[nodiscard]] static constexpr Result FromValue(Args&&... args) noexcept(
        std::is_nothrow_constructible_v<ValueType, Args&&...>)
    {
        return Result{std::in_place, std::forward<Args>(args)...};
    }

    [[nodiscard]] static constexpr Result FromError(ErrorType error) noexcept(
        std::is_nothrow_constructible_v<ExpectedType, std::unexpected<ErrorType>&&>)
    {
        if consteval
        {
            detail::RejectConstexprErrorConstruction();
        }
        return Result{std::unexpected<ErrorType>{std::move(error)}};
    }

    [[nodiscard]] constexpr bool HasValue() const noexcept
    {
        return m_expected.has_value();
    }

    [[nodiscard]] constexpr explicit operator bool() const noexcept
    {
        return HasValue();
    }

    [[nodiscard]] constexpr ValueType& GetValue() &
    {
        return m_expected.value();
    }

    [[nodiscard]] constexpr const ValueType& GetValue() const&
    {
        return m_expected.value();
    }

    [[nodiscard]] constexpr ValueType&& GetValue() &&
    {
        return std::move(m_expected).value();
    }

    [[nodiscard]] constexpr const ValueType&& GetValue() const&&
    {
        return std::move(m_expected).value();
    }

    [[nodiscard]] constexpr ErrorType& GetError() &
    {
        return m_expected.error();
    }

    [[nodiscard]] constexpr const ErrorType& GetError() const&
    {
        return m_expected.error();
    }

    [[nodiscard]] constexpr ErrorType&& GetError() &&
    {
        return std::move(m_expected).error();
    }

    [[nodiscard]] constexpr const ErrorType&& GetError() const&&
    {
        return std::move(m_expected).error();
    }

    [[nodiscard]] constexpr ValueType& operator*() & noexcept
    {
        return *m_expected;
    }

    [[nodiscard]] constexpr const ValueType& operator*() const& noexcept
    {
        return *m_expected;
    }

    [[nodiscard]] constexpr ValueType&& operator*() && noexcept
    {
        return *std::move(m_expected);
    }

    [[nodiscard]] constexpr const ValueType&& operator*() const&& noexcept
    {
        return *std::move(m_expected);
    }

    [[nodiscard]] constexpr ValueType* operator->() noexcept
    {
        return std::addressof(*m_expected);
    }

    [[nodiscard]] constexpr const ValueType* operator->() const noexcept
    {
        return std::addressof(*m_expected);
    }

private:
    template <typename... Args>
        requires std::constructible_from<ValueType, Args&&...>
    constexpr explicit Result(std::in_place_t, Args&&... args) noexcept(
        std::is_nothrow_constructible_v<ValueType, Args&&...>)
        : m_expected(std::in_place, std::forward<Args>(args)...)
    {
    }

    ExpectedType m_expected;
};

template <>
class [[nodiscard]] Result<void> final
{
public:
    using ValueType = void;
    using ErrorType = Error;
    using ExpectedType = std::expected<void, ErrorType>;

    constexpr Result() = default;
    constexpr Result(const Result&) = default;
    constexpr Result(Result&&) noexcept(std::is_nothrow_move_constructible_v<ExpectedType>) =
        default;
    constexpr Result& operator=(const Result&) = default;
    constexpr Result& operator=(Result&&) noexcept(
        std::is_nothrow_move_assignable_v<ExpectedType>) = default;
    ~Result() = default;

    constexpr Result(std::unexpected<ErrorType> unexpected) noexcept(
        std::is_nothrow_constructible_v<ExpectedType, std::unexpected<ErrorType>&&>)
        : m_expected(std::move(unexpected))
    {
    }

    [[nodiscard]] static constexpr Result Success() noexcept
    {
        return Result{};
    }

    [[nodiscard]] static constexpr Result FromError(ErrorType error) noexcept(
        std::is_nothrow_constructible_v<ExpectedType, std::unexpected<ErrorType>&&>)
    {
        if consteval
        {
            detail::RejectConstexprErrorConstruction();
        }
        return Result{std::unexpected<ErrorType>{std::move(error)}};
    }

    [[nodiscard]] constexpr bool HasValue() const noexcept
    {
        return m_expected.has_value();
    }

    [[nodiscard]] constexpr explicit operator bool() const noexcept
    {
        return HasValue();
    }

    constexpr void GetValue() const
    {
        m_expected.value();
    }

    [[nodiscard]] constexpr ErrorType& GetError() &
    {
        return m_expected.error();
    }

    [[nodiscard]] constexpr const ErrorType& GetError() const&
    {
        return m_expected.error();
    }

    [[nodiscard]] constexpr ErrorType&& GetError() &&
    {
        return std::move(m_expected).error();
    }

    [[nodiscard]] constexpr const ErrorType&& GetError() const&&
    {
        return std::move(m_expected).error();
    }

private:
    ExpectedType m_expected;
};

using VoidResult = Result<void>;

template <typename... Args>
[[nodiscard]] constexpr std::unexpected<Error> MakeUnexpected(Args&&... args)
{
    if consteval
    {
        detail::RejectConstexprErrorConstruction();
    }
    return std::unexpected<Error>(Error(std::forward<Args>(args)...));
}
} // namespace pond::core