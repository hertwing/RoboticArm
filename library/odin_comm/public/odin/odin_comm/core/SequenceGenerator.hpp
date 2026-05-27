#ifndef SEQUENCE_GENERATOR_HPP
#define SEQUENCE_GENERATOR_HPP

#include "odin/odin_comm/core/Types.hpp"

namespace odin
{
namespace odin_comm
{

class SequenceGenerator
{
public:
    constexpr SequenceGenerator() noexcept = default;

    explicit constexpr SequenceGenerator(SequenceId initialValue) noexcept
        : m_nextSequenceId{initialValue}
    {
    }

    [[nodiscard]] constexpr SequenceId next() noexcept
    {
        const auto current = m_nextSequenceId;

        ++m_nextSequenceId;

        if (m_nextSequenceId == 0) {
            m_nextSequenceId = 1;
        }

        return current;
    }

    [[nodiscard]] constexpr SequenceId current() const noexcept
    {
        return m_nextSequenceId;
    }

    constexpr void reset(SequenceId value = 1) noexcept
    {
        m_nextSequenceId = value == 0 ? 1 : value;
    }

private:
    SequenceId m_nextSequenceId{1};
};

} // namespace odin_comm
} // namespace odin

#endif // SEQUENCE_GENERATOR_HPP