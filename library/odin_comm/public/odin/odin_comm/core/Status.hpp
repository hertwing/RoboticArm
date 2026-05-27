#ifndef STATUS_HPP
#define STATUS_HPP

#include "odin/odin_comm/core/Error.hpp"

namespace odin
{
namespace odin_comm
{

class Status
{
public:
    constexpr Status() noexcept = default;

    constexpr explicit Status(CommError error) noexcept
        : m_error{error}
    {}

    [[nodiscard]] static constexpr Status ok() noexcept
    {
        return Status{CommError::None};
    }

    [[nodiscard]] static constexpr Status failure(CommError error) noexcept
    {
        return Status{error};
    }

    [[nodiscard]] constexpr bool isOk() const noexcept
    {
        return m_error == CommError::None;
    }

    [[nodiscard]] constexpr explicit operator bool() const noexcept
    {
        return isOk();
    }

    [[nodiscard]] constexpr CommError error() const noexcept
    {
        return m_error;
    }

private:
    CommError m_error{CommError::None};
};

} // namespace odin_comm
} // namespace odin

#endif // STATUS_HPP