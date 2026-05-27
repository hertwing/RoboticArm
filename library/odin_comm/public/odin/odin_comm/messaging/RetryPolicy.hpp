#ifndef RETRY_POLICY_HPP
#define RETRY_POLICY_HPP

#include <chrono>
#include <cstdint>

namespace odin
{
namespace odin_comm
{

struct RetryPolicy
{
    std::uint32_t maxAttempts{3};
    std::chrono::milliseconds ackTimeout{50};
};

} // namespace odin_comm
} // namespace odin

#endif // RETRY_POLICY_HPP