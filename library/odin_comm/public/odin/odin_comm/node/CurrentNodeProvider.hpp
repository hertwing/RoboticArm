#ifndef CURRENT_NODE_PROVIDER_HPP
#define CURRENT_NODE_PROVIDER_HPP

#include <cstdlib>
#include <string_view>

#include "odin/odin_comm/config/NodeId.hpp"
#include "odin/odin_comm/core/Error.hpp"
#include "odin/odin_comm/core/Result.hpp"

namespace odin
{
namespace odin_comm
{

class CurrentNodeProvider
{
public:
    static constexpr std::string_view EnvironmentVariableName{"ODIN_NODE"};

    [[nodiscard]] static Result<NodeId> detect()
    {
        const char* rawNodeName = std::getenv(EnvironmentVariableName.data());

        if (rawNodeName == nullptr)
        {
            return Result<NodeId>::failure(CommError::EnvironmentVariableMissing);
        }

        return parseNodeId(rawNodeName);
    }

    [[nodiscard]] static Result<NodeId> parseNodeId(std::string_view nodeName)
    {
        if (nodeName == "Gui" || nodeName == "gui" || nodeName == "GUI") {
            return Result<NodeId>::success(NodeId::Gui);
        }

        if (nodeName == "Arm" || nodeName == "arm" || nodeName == "ARM") {
            return Result<NodeId>::success(NodeId::Arm);
        }

        if (nodeName == "Vision" || nodeName == "vision" || nodeName == "VISION") {
            return Result<NodeId>::success(NodeId::Vision);
        }

        if (nodeName == "Diagnostics" ||
            nodeName == "diagnostics" ||
            nodeName == "DIAGNOSTICS") {
            return Result<NodeId>::success(NodeId::Diagnostics);
        }

        return Result<NodeId>::failure(CommError::UnknownNode);
    }
};

} // namespace odin_comm
} // namespace odin

#endif // CURRENT_NODE_PROVIDER_HPP