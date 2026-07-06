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
enum class ErrorCategory
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

[[nodiscard]] std::string_view GetErrorCategoryName(ErrorCategory category) noexcept;
[[nodiscard]] std::string FormatErrorCode(ErrorCode code);
[[nodiscard]] std::string FormatError(const Error& error);

template <typename Value> class [[nodiscard]] Result final
{
public:
    using ValueType = Value;
    using ErrorType = Error;
    using ExpectedType = std::expected<ValueType, ErrorType>;

    Result()
        requires std::default_initializable<ValueType>
    = default;
    Result(const Result&) = default;
    Result(Result&&) noexcept(std::is_nothrow_move_constructible_v<ExpectedType>) = default;
    Result& operator=(const Result&) = default;
    Result& operator=(Result&&) noexcept(std::is_nothrow_move_assignable_v<ExpectedType>) = default;
    ~Result() = default;

    template <typename Input>
        requires(!std::same_as<std::remove_cvref_t<Input>, Result> &&
                 !std::same_as<std::remove_cvref_t<Input>, std::unexpected<ErrorType>> &&
                 std::constructible_from<ValueType, Input &&>)
    Result(Input&& value) : m_expected(std::in_place, std::forward<Input>(value))
    {
    }

    Result(std::unexpected<ErrorType> unexpected) : m_expected(std::move(unexpected)) {}

    template <typename... Args>
        requires std::constructible_from<ValueType, Args&&...>
    [[nodiscard]] static Result FromValue(Args&&... args)
    {
        return Result{std::in_place, std::forward<Args>(args)...};
    }

    [[nodiscard]] static Result FromError(ErrorType error)
    {
        return Result{std::unexpected<ErrorType>{std::move(error)}};
    }

    [[nodiscard]] bool HasValue() const noexcept
    {
        return m_expected.has_value();
    }

    [[nodiscard]] explicit operator bool() const noexcept
    {
        return HasValue();
    }

    [[nodiscard]] ValueType& GetValue() &
    {
        return m_expected.value();
    }

    [[nodiscard]] const ValueType& GetValue() const&
    {
        return m_expected.value();
    }

    [[nodiscard]] ValueType&& GetValue() &&
    {
        return std::move(m_expected).value();
    }

    [[nodiscard]] const ValueType&& GetValue() const&&
    {
        return std::move(m_expected).value();
    }

    [[nodiscard]] ErrorType& GetError() &
    {
        return m_expected.error();
    }

    [[nodiscard]] const ErrorType& GetError() const&
    {
        return m_expected.error();
    }

    [[nodiscard]] ErrorType&& GetError() &&
    {
        return std::move(m_expected).error();
    }

    [[nodiscard]] const ErrorType&& GetError() const&&
    {
        return std::move(m_expected).error();
    }

    [[nodiscard]] ValueType& operator*() & noexcept
    {
        return *m_expected;
    }

    [[nodiscard]] const ValueType& operator*() const& noexcept
    {
        return *m_expected;
    }

    [[nodiscard]] ValueType&& operator*() && noexcept
    {
        return *std::move(m_expected);
    }

    [[nodiscard]] const ValueType&& operator*() const&& noexcept
    {
        return *std::move(m_expected);
    }

    [[nodiscard]] ValueType* operator->() noexcept
    {
        return std::addressof(*m_expected);
    }

    [[nodiscard]] const ValueType* operator->() const noexcept
    {
        return std::addressof(*m_expected);
    }

private:
    template <typename... Args>
        requires std::constructible_from<ValueType, Args&&...>
    explicit Result(std::in_place_t, Args&&... args)
        : m_expected(std::in_place, std::forward<Args>(args)...)
    {
    }

    ExpectedType m_expected;
};

template <> class [[nodiscard]] Result<void> final
{
public:
    using ValueType = void;
    using ErrorType = Error;
    using ExpectedType = std::expected<void, ErrorType>;

    Result() = default;
    Result(const Result&) = default;
    Result(Result&&) noexcept(std::is_nothrow_move_constructible_v<ExpectedType>) = default;
    Result& operator=(const Result&) = default;
    Result& operator=(Result&&) noexcept(std::is_nothrow_move_assignable_v<ExpectedType>) = default;
    ~Result() = default;

    Result(std::unexpected<ErrorType> unexpected) : m_expected(std::move(unexpected)) {}

    [[nodiscard]] static Result Success()
    {
        return Result{};
    }

    [[nodiscard]] static Result FromError(ErrorType error)
    {
        return Result{std::unexpected<ErrorType>{std::move(error)}};
    }

    [[nodiscard]] bool HasValue() const noexcept
    {
        return m_expected.has_value();
    }

    [[nodiscard]] explicit operator bool() const noexcept
    {
        return HasValue();
    }

    void GetValue() const
    {
        m_expected.value();
    }

    [[nodiscard]] ErrorType& GetError() &
    {
        return m_expected.error();
    }

    [[nodiscard]] const ErrorType& GetError() const&
    {
        return m_expected.error();
    }

    [[nodiscard]] ErrorType&& GetError() &&
    {
        return std::move(m_expected).error();
    }

    [[nodiscard]] const ErrorType&& GetError() const&&
    {
        return std::move(m_expected).error();
    }

private:
    ExpectedType m_expected;
};

using VoidResult = Result<void>;

template <typename... Args> [[nodiscard]] std::unexpected<Error> MakeUnexpected(Args&&... args)
{
    return std::unexpected<Error>(Error(std::forward<Args>(args)...));
}
} // namespace pond::core
