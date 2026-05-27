#ifndef COMMUNICATION_CONFIG_HPP
#define COMMUNICATION_CONFIG_HPP

#include <string>
#include <vector>

#include "odin/odin_comm/config/CommunicationEndpoint.hpp"
#include "odin/odin_comm/config/EndpointRole.hpp"
#include "odin/odin_comm/config/MessageId.hpp"
#include "odin/odin_comm/config/NodeId.hpp"
#include "odin/odin_comm/config/ReliabilityKind.hpp"
#include "odin/odin_comm/shmem/ShmemDefaults.hpp"
#include "odin/odin_comm/udp/NetworkDefaults.hpp"

namespace odin
{
namespace odin_comm
{

struct NodeConfig
{
    NodeId id{NodeId::Unknown};
    std::string ipAddress;

    std::vector<UdpEndpointConfig> udpEndpoints;
    std::vector<ShmemEndpointConfig> shmemEndpoints;
};

struct CommunicationConfig
{
    std::vector<NodeConfig> nodes;
};

[[nodiscard]] inline CommunicationConfig makeDefaultCommunicationConfig()
{
    CommunicationConfig config;

    config.nodes = {
        NodeConfig{
            .id = NodeId::Gui,
            .ipAddress = std::string{RoboticGuiIp},
            .udpEndpoints = {
                UdpEndpointConfig{
                    .role = EndpointRole::Writer,
                    .messageId = MessageId::ControlSelection,
                    .peerNode = NodeId::Arm,
                    .reliability = ReliabilityKind::Reliable,
                    .port = ControlSelectionPort
                },
                UdpEndpointConfig{
                    .role = EndpointRole::Reader,
                    .messageId = MessageId::DiagnosticData,
                    .peerNode = NodeId::Arm,
                    .reliability = ReliabilityKind::Reliable,
                    .port = DiagnosticSocketPort
                },
                UdpEndpointConfig{
                    .role = EndpointRole::Reader,
                    .messageId = MessageId::CameraPosition,
                    .peerNode = NodeId::Arm,
                    .reliability = ReliabilityKind::BestEffort,
                    .port = CameraPositionPort
                },
                UdpEndpointConfig{
                    .role = EndpointRole::Writer,
                    .messageId = MessageId::CameraPositionReady,
                    .peerNode = NodeId::Arm,
                    .reliability = ReliabilityKind::Reliable,
                    .port = CameraPositionReadyPort
                },
                UdpEndpointConfig{
                    .role = EndpointRole::Writer,
                    .messageId = MessageId::ScriptedMotionCommand,
                    .peerNode = NodeId::Arm,
                    .reliability = ReliabilityKind::Reliable,
                    .port = ScriptedMotionCommandPort
                },
                UdpEndpointConfig{
                    .role = EndpointRole::Reader,
                    .messageId = MessageId::ScriptedMotionStatus,
                    .peerNode = NodeId::Arm,
                    .reliability = ReliabilityKind::Reliable,
                    .port = ScriptedMotionStatusPort
                },
                UdpEndpointConfig{
                    .role = EndpointRole::Writer,
                    .messageId = MessageId::ServoStep,
                    .peerNode = NodeId::Arm,
                    .reliability = ReliabilityKind::Reliable,
                    .port = ServoStepPort
                }
            },
            .shmemEndpoints = {}
        },
        NodeConfig{
            .id = NodeId::Arm,
            .ipAddress = std::string{RoboticArmIp},
            .udpEndpoints = {
                UdpEndpointConfig{
                    .role = EndpointRole::Reader,
                    .messageId = MessageId::ControlSelection,
                    .peerNode = NodeId::Gui,
                    .reliability = ReliabilityKind::Reliable,
                    .port = ControlSelectionPort
                },
                UdpEndpointConfig{
                    .role = EndpointRole::Writer,
                    .messageId = MessageId::DiagnosticData,
                    .peerNode = NodeId::Gui,
                    .reliability = ReliabilityKind::Reliable,
                    .port = DiagnosticSocketPort
                },
                UdpEndpointConfig{
                    .role = EndpointRole::Writer,
                    .messageId = MessageId::CameraPosition,
                    .peerNode = NodeId::Gui,
                    .reliability = ReliabilityKind::Reliable,
                    .port = CameraPositionPort
                },
                UdpEndpointConfig{
                    .role = EndpointRole::Reader,
                    .messageId = MessageId::CameraPositionReady,
                    .peerNode = NodeId::Gui,
                    .reliability = ReliabilityKind::Reliable,
                    .port = CameraPositionReadyPort
                },
                UdpEndpointConfig{
                    .role = EndpointRole::Reader,
                    .messageId = MessageId::ScriptedMotionCommand,
                    .peerNode = NodeId::Gui,
                    .reliability = ReliabilityKind::Reliable,
                    .port = ScriptedMotionCommandPort
                },
                UdpEndpointConfig{
                    .role = EndpointRole::Writer,
                    .messageId = MessageId::ScriptedMotionStatus,
                    .peerNode = NodeId::Gui,
                    .reliability = ReliabilityKind::Reliable,
                    .port = ScriptedMotionStatusPort
                },
                UdpEndpointConfig{
                    .role = EndpointRole::Reader,
                    .messageId = MessageId::ServoStep,
                    .peerNode = NodeId::Gui,
                    .reliability = ReliabilityKind::Reliable,
                    .port = ServoStepPort
                }
            },
            .shmemEndpoints = {
                ShmemEndpointConfig{
                    .role = EndpointRole::Writer,
                    .messageId = MessageId::JoypadData,
                    .shmemName = std::string{JoypadDataShmemName}
                },
                ShmemEndpointConfig{
                    .role = EndpointRole::Reader,
                    .messageId = MessageId::JoypadData,
                    .shmemName = std::string{JoypadDataShmemName}
                },
                ShmemEndpointConfig{
                    .role = EndpointRole::Writer,
                    .messageId = MessageId::LedState,
                    .shmemName = std::string{LedShmemName}
                },
                ShmemEndpointConfig{
                    .role = EndpointRole::Reader,
                    .messageId = MessageId::LedState,
                    .shmemName = std::string{LedShmemName}
                }
            }
        }
    };

    return config;
}

} // namespace odin_comm
} // namespace odin

#endif // COMMUNICATION_CONFIG_HPP