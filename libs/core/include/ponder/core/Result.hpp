#pragma once

#include <ponder/core/StackTrace.hpp>

#include <concepts>
#include <cstdint>
#include <expected>
#include <format>
#include <memory>
#include <ostream>
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

template <typename ErrorType>
concept ConvertToErrorCode =
    std::same_as<std::remove_cvref_t<ErrorType>, ErrorCode> ||
    requires(ErrorType error) {
        { ToErrorCode(error) } -> std::same_as<ErrorCode>;
    };

[[nodiscard]] inline bool operator==(const Error& error, ErrorCode code) noexcept
{
    return error.GetCode() == code;
}

[[nodiscard]] inline bool operator==(ErrorCode code, const Error& error) noexcept
{
    return error == code;
}

template <typename ErrorType>
    requires(!std::same_as<std::remove_cvref_t<ErrorType>, ErrorCode> &&
             ConvertToErrorCode<ErrorType>)
[[nodiscard]] bool operator==(const Error& error, ErrorType code) noexcept(
    noexcept(ToErrorCode(code)))
{
    return error.GetCode() == ToErrorCode(code);
}

template <typename ErrorType>
    requires(!std::same_as<std::remove_cvref_t<ErrorType>, ErrorCode> &&
             ConvertToErrorCode<ErrorType>)
[[nodiscard]] bool operator==(ErrorType code, const Error& error) noexcept(noexcept(error == code))
{
    return error == code;
}

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

inline std::ostream& operator<<(std::ostream& output, ErrorCategory category)
{
    return output << GetErrorCategoryName(category);
}

inline std::ostream& operator<<(std::ostream& output, ErrorCode code)
{
    return output << code.GetCategory() << ':' << code.GetValue();
}

inline std::ostream& operator<<(std::ostream& output, const Error& error)
{
    return output << '[' << error.GetCode() << "] " << error.GetMessage() << " ("
                  << FormatSourceLocation(error.GetLocation()) << ')';
}


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

namespace detail
{
template <typename Value>
concept StreamableResultValue = requires(std::ostream& output, const Value& value)
{
    output << value;
};

template <typename Value>
concept FormattableResultValue = requires(const Value& value)
{
    std::format("{}", value);
};
} // namespace detail

template <typename Value>
inline std::ostream& operator<<(std::ostream& output, const Result<Value>& result)
{
    if (result)
    {
        if constexpr (detail::StreamableResultValue<Value>)
        {
            return output << "Result{value=" << result.GetValue() << '}';
        }
        else
        {
            return output << "Result{value=<unprintable>}";
        }
    }

    return output << "Result{error=" << result.GetError() << '}';
}

inline std::ostream& operator<<(std::ostream& output, const Result<void>& result)
{
    if (result)
    {
        return output << "Result{success}";
    }

    return output << "Result{error=" << result.GetError() << '}';
}

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

namespace std
{
template <>
struct formatter<pond::core::ErrorCategory> : formatter<string_view>
{
    template <typename FormatContext>
    auto format(pond::core::ErrorCategory category, FormatContext& context) const
    {
        return formatter<string_view>::format(pond::core::GetErrorCategoryName(category), context);
    }
};

template <>
struct formatter<pond::core::ErrorCode> : formatter<string>
{
    template <typename FormatContext>
    auto format(pond::core::ErrorCode code, FormatContext& context) const
    {
        return formatter<string>::format(
            std::format("{}:{}", pond::core::GetErrorCategoryName(code.GetCategory()),
                        code.GetValue()),
            context);
    }
};

template <>
struct formatter<pond::core::Error> : formatter<string>
{
    template <typename FormatContext>
    auto format(const pond::core::Error& error, FormatContext& context) const
    {
        return formatter<string>::format(
            std::format("[{}] {} ({})", error.GetCode(), error.GetMessage(),
                   pond::core::FormatSourceLocation(error.GetLocation())),
            context);
    }
};

template <typename Value>
struct formatter<pond::core::Result<Value>> : formatter<string>
{
    template <typename FormatContext>
    auto format(const pond::core::Result<Value>& result, FormatContext& context) const
    {
        if (result)
        {
            if constexpr (pond::core::detail::FormattableResultValue<Value>)
            {
                return formatter<string>::format(
                    std::format("Result{{value={}}}", result.GetValue()), context);
            }
            else
            {
                return formatter<string>::format("Result{value=<unprintable>}", context);
            }
        }

        return formatter<string>::format(std::format("Result{{error={}}}", result.GetError()),
                                         context);
    }
};

template <>
struct formatter<pond::core::Result<void>> : formatter<string>
{
    template <typename FormatContext>
    auto format(const pond::core::Result<void>& result, FormatContext& context) const
    {
        if (result)
        {
            return formatter<string>::format("Result{success}", context);
        }

        return formatter<string>::format(std::format("Result{{error={}}}", result.GetError()),
                                         context);
    }
};
} // namespace std

#define PONDER_CORE_DETAIL_CONCAT_IMPL(lhs, rhs) lhs##rhs
#define PONDER_CORE_DETAIL_CONCAT(lhs, rhs) PONDER_CORE_DETAIL_CONCAT_IMPL(lhs, rhs)

#define RETURN_VOID_IF_FAILED(resultExpr)                                                        \
    PONDER_CORE_DETAIL_RETURN_VOID_IF_FAILED_IMPL(                                                \
        resultExpr, PONDER_CORE_DETAIL_CONCAT(ponderResult_, __COUNTER__))

#define PONDER_CORE_DETAIL_RETURN_VOID_IF_FAILED_IMPL(resultExpr, resultName)                     \
    do                                                                                            \
    {                                                                                             \
        auto&& resultName = (resultExpr);                                                         \
        if (!resultName) [[unlikely]]                                                            \
        {                                                                                         \
            return;                                                                               \
        }                                                                                         \
    } while (false)

#define RETURN_VOID_IF_FAILED_FN(resultExpr, ...)                                                 \
    PONDER_CORE_DETAIL_RETURN_VOID_IF_FAILED_FN_IMPL(                                             \
        resultExpr, PONDER_CORE_DETAIL_CONCAT(ponderResult_, __COUNTER__),                        \
        PONDER_CORE_DETAIL_CONCAT(ponderError_, __COUNTER__), __VA_ARGS__)

#define PONDER_CORE_DETAIL_RETURN_VOID_IF_FAILED_FN_IMPL(resultExpr, resultName, errorName, ...)  \
    do                                                                                            \
    {                                                                                             \
        auto&& resultName = (resultExpr);                                                         \
        if (!resultName) [[unlikely]]                                                            \
        {                                                                                         \
            const ::pond::core::Error& errorName = resultName.GetError();                         \
            (__VA_ARGS__)(errorName);                                                            \
            return;                                                                               \
        }                                                                                         \
    } while (false)

#define RETURN_ERROR_IF_FAILED(resultExpr)                                                        \
    PONDER_CORE_DETAIL_RETURN_ERROR_IF_FAILED_IMPL(                                               \
        resultExpr, PONDER_CORE_DETAIL_CONCAT(ponderResult_, __COUNTER__))

#define PONDER_CORE_DETAIL_RETURN_ERROR_IF_FAILED_IMPL(resultExpr, resultName)                    \
    do                                                                                            \
    {                                                                                             \
        auto&& resultName = (resultExpr);                                                         \
        if (!resultName) [[unlikely]]                                                            \
        {                                                                                         \
            return std::unexpected<::pond::core::Error>{std::move(resultName).GetError()};        \
        }                                                                                         \
    } while (false)

#define RETURN_ERROR_IF_FAILED_FN(resultExpr, ...)                                                \
    PONDER_CORE_DETAIL_RETURN_ERROR_IF_FAILED_FN_IMPL(                                            \
        resultExpr, PONDER_CORE_DETAIL_CONCAT(ponderResult_, __COUNTER__),                        \
        PONDER_CORE_DETAIL_CONCAT(ponderError_, __COUNTER__), __VA_ARGS__)

#define PONDER_CORE_DETAIL_RETURN_ERROR_IF_FAILED_FN_IMPL(resultExpr, resultName, errorName, ...) \
    do                                                                                            \
    {                                                                                             \
        auto&& resultName = (resultExpr);                                                         \
        if (!resultName) [[unlikely]]                                                            \
        {                                                                                         \
            const ::pond::core::Error& errorName = resultName.GetError();                         \
            (__VA_ARGS__)(errorName);                                                            \
            return std::unexpected<::pond::core::Error>{std::move(resultName).GetError()};        \
        }                                                                                         \
    } while (false)
