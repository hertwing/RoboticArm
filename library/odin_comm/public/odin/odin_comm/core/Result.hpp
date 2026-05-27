#ifndef RESULT_HPP
#define RESULT_HPP

#include <optional>
#include <utility>

#include "odin/odin_comm/core/Error.hpp"

namespace odin
{
namespace odin_comm
{

template <typename T>
class Result
{
public:
    Result() = delete;

    [[nodiscard]] static Result success(T value)
    {
        return Result{std::move(value)};
    }

    [[nodiscard]] static Result failure(CommError error)
    {
        return Result{error};
    }

    [[nodiscard]] bool isOk() const noexcept
    {
        return m_value.has_value() && m_error == CommError::None;
    }

    [[nodiscard]] explicit operator bool() const noexcept
    {
        return isOk();
    }

    [[nodiscard]] CommError error() const noexcept
    {
        return m_error;
    }

    [[nodiscard]] T& value()
    {
        return *m_value;
    }

    [[nodiscard]] const T& value() const
    {
        return *m_value;
    }

private:
    explicit Result(T value)
        : m_value{std::move(value)}
        , m_error{CommError::None}
    {
    }

    explicit Result(CommError error)
        : m_value{std::nullopt}
        , m_error{error}
    {
    }

    std::optional<T> m_value;
    CommError m_error{CommError::None};
};

} // namespace odin_comm
} // namespace odin

#endif // RESULT_HPP