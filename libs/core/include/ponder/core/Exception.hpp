#pragma once

#include <ponder/core/StackTrace.hpp>

#include <format>
#include <ostream>
#include <source_location>
#include <string>
#include <string_view>
#include <type_traits>
#include <utility>

namespace ponder::core
{
class Exception
{
public:
    constexpr explicit Exception(std::string message, std::source_location location = std::source_location::current()) :
        m_message(std::move(message)),
        m_location(location)
    {
        if consteval
        {
        }
        else
        {
            m_stackTrace = CaptureStackTrace();
        }
    }

    constexpr Exception(std::string message, StackTrace stackTrace, std::source_location location = std::source_location::current()) :
        m_message(std::move(message)),
        m_location(location),
        m_stackTrace(std::move(stackTrace))
    {
    }

    constexpr Exception(const Exception&) = default;
    constexpr Exception(Exception&&) noexcept = default;
    constexpr Exception& operator=(const Exception&) = default;
    constexpr Exception& operator=(Exception&&) noexcept = default;
    virtual constexpr ~Exception() = default;

    [[nodiscard]] constexpr std::string_view GetMessage() const noexcept
    {
        return m_message;
    }

    [[nodiscard]] constexpr const std::source_location& GetLocation() const noexcept
    {
        return m_location;
    }

    [[nodiscard]] constexpr const StackTrace& GetStackTrace() const noexcept
    {
        return m_stackTrace;
    }

private:
    std::string m_message;
    std::source_location m_location;
    StackTrace m_stackTrace;
};

template <typename Data>
class ExceptionWithData final : public Exception
{
    static_assert(std::is_same_v<Data, std::decay_t<Data>>, "ExceptionWithData requires a decayed data type");

public:
    using DataType = Data;

    constexpr ExceptionWithData(std::string message, Data data, std::source_location location = std::source_location::current()) :
        Exception(std::move(message), location),
        m_data(std::move(data))
    {
    }

    constexpr ExceptionWithData(std::string message, Data data, StackTrace stackTrace,
                                std::source_location location = std::source_location::current()) :
        Exception(std::move(message), std::move(stackTrace), location),
        m_data(std::move(data))
    {
    }

    [[nodiscard]] constexpr Data& GetData() & noexcept
    {
        return m_data;
    }

    [[nodiscard]] constexpr const Data& GetData() const& noexcept
    {
        return m_data;
    }

    [[nodiscard]] constexpr Data&& GetData() && noexcept
    {
        return std::move(m_data);
    }

private:
    Data m_data;
};

[[nodiscard]] constexpr Exception MakeException(std::string message, std::source_location location = std::source_location::current())
{
    if consteval
    {
        return Exception{std::move(message), StackTrace{}, location};
    }
    else
    {
        return Exception{std::move(message), CaptureStackTrace(), location};
    }
}

template <typename Data>
[[nodiscard]] constexpr auto MakeExceptionWithData(std::string message, Data&& data, std::source_location location = std::source_location::current())
    -> ExceptionWithData<std::decay_t<Data>>
{
    using StoredData = std::decay_t<Data>;

    if consteval
    {
        return ExceptionWithData<StoredData>{std::move(message), std::forward<Data>(data), StackTrace{}, location};
    }
    else
    {
        return ExceptionWithData<StoredData>{std::move(message), std::forward<Data>(data), CaptureStackTrace(), location};
    }
}

template <typename... Args>
[[nodiscard]] Exception MakeFormattedException(std::source_location location, std::format_string<Args...> messageFormat, Args&&... args)
{
    return MakeException(std::format(messageFormat, std::forward<Args>(args)...), location);
}

template <typename Data, typename... Args>
[[nodiscard]] auto MakeFormattedExceptionWithData(std::source_location location, Data&& data, std::format_string<Args...> messageFormat,
                                                  Args&&... args) -> ExceptionWithData<std::decay_t<Data>>
{
    return MakeExceptionWithData(std::format(messageFormat, std::forward<Args>(args)...), std::forward<Data>(data), location);
}

namespace detail
{
template <typename Data>
concept StreamableExceptionData = requires(std::ostream& output, const Data& data) { output << data; };

template <typename Data>
concept FormattableExceptionData = std::formattable<const Data&, char>;
} // namespace detail

template <typename Data>
std::ostream& operator<<(std::ostream& output, const ExceptionWithData<Data>& exception)
{
    output << exception.GetMessage() << " (data: ";
    if constexpr (detail::StreamableExceptionData<Data>)
    {
        output << exception.GetData();
    }
    else
    {
        output << "<unprintable>";
    }
    return output << ')';
}
} // namespace ponder::core

namespace std
{
template <typename Data>
struct formatter<ponder::core::ExceptionWithData<Data>> : formatter<string>
{
    template <typename FormatContext>
    auto format(const ponder::core::ExceptionWithData<Data>& exception, FormatContext& context) const
    {
        if constexpr (ponder::core::detail::FormattableExceptionData<Data>)
        {
            return formatter<string>::format(std::format("{} (data: {})", exception.GetMessage(), exception.GetData()), context);
        }
        else
        {
            return formatter<string>::format(std::format("{} (data: <unprintable>)", exception.GetMessage()), context);
        }
    }
};
} // namespace std

#define PONDER_EXCEPTION(...) ::ponder::core::MakeFormattedException(std::source_location::current(), __VA_ARGS__)

#define PONDER_EXCEPTION_WITH_DATA(data, ...) ::ponder::core::MakeFormattedExceptionWithData(std::source_location::current(), (data), __VA_ARGS__)
