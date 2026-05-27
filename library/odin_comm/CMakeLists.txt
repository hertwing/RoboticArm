project(odin_comm)

set(HEADERS
    public/odin/odin_comm/config/CommunicationConfig.hpp
    public/odin/odin_comm/config/CommunicationConfigRegistry.hpp
    public/odin/odin_comm/config/CommunicationEndpoint.hpp
    public/odin/odin_comm/config/EndpointRole.hpp
    public/odin/odin_comm/config/MessageId.hpp
    public/odin/odin_comm/config/NodeId.hpp
    public/odin/odin_comm/config/ReliabilityKind.hpp
    public/odin/odin_comm/config/TransportKind.hpp
    public/odin/odin_comm/core/Checksum.hpp
    public/odin/odin_comm/core/Error.hpp
    public/odin/odin_comm/core/Message.hpp
    public/odin/odin_comm/core/MessageCodec.hpp
    public/odin/odin_comm/core/MessageFactory.hpp
    public/odin/odin_comm/core/MessageFlags.hpp
    public/odin/odin_comm/core/MessageHeader.hpp
    public/odin/odin_comm/core/MessageType.hpp
    public/odin/odin_comm/core/MessageValidation.hpp
    public/odin/odin_comm/core/Result.hpp
    public/odin/odin_comm/core/Serializer.hpp
    public/odin/odin_comm/core/SequenceGenerator.hpp
    public/odin/odin_comm/core/Status.hpp
    public/odin/odin_comm/core/Types.hpp
    public/odin/odin_comm/messages/CameraPosition.hpp
    public/odin/odin_comm/messages/CameraPositionReady.hpp
    public/odin/odin_comm/messages/ControlSelection.hpp
    public/odin/odin_comm/messages/DiagnosticData.hpp
    public/odin/odin_comm/messages/JoypadData.hpp
    public/odin/odin_comm/messages/LedState.hpp
    public/odin/odin_comm/messages/ScriptedMotionCommand.hpp
    public/odin/odin_comm/messages/ScriptedMotionStatus.hpp
    public/odin/odin_comm/messages/ServoStep.hpp
    public/odin/odin_comm/messaging/Messenger.hpp
    public/odin/odin_comm/messaging/MessageTraits.hpp
    public/odin/odin_comm/messaging/Publisher.hpp
    public/odin/odin_comm/messaging/RetryPolicy.hpp
    public/odin/odin_comm/messaging/Subscriber.hpp
    public/odin/odin_comm/node/CommunicationNode.hpp
    public/odin/odin_comm/node/CurrentNodeProvider.hpp
    public/odin/odin_comm/node/EndpointFactory.hpp
    public/odin/odin_comm/shmem/ShmemChannelConfig.hpp
    public/odin/odin_comm/shmem/ShmemDefaults.hpp
    public/odin/odin_comm/shmem/ShmemDuplexTransport.hpp
    public/odin/odin_comm/shmem/ShmemDuplexTransportConfig.hpp
    public/odin/odin_comm/shmem/ShmemTransport.hpp
    public/odin/odin_comm/shmem/ShmemTransportConfig.hpp
    public/odin/odin_comm/transport/ITransport.hpp
    public/odin/odin_comm/transport/TransportTraits.hpp
    public/odin/odin_comm/udp/NetworkDefaults.hpp
    public/odin/odin_comm/udp/UdpChannel.hpp
    public/odin/odin_comm/udp/UdpChannelConfig.hpp
    public/odin/odin_comm/udp/UdpEndpoint.hpp
    public/odin/odin_comm/udp/UdpTransport.hpp
    public/odin/odin_comm/udp/UdpTransportConfig.hpp
    public/odin/odin_comm/OdinComm.hpp
    public/odin/odin_comm/OdinCommReader.hpp
    public/odin/odin_comm/OdinCommWriter.hpp
    )

set(SOURCES
    private/Checksum.cpp
    private/MessageCodec.cpp
    private/MessageFactory.cpp
    private/MessageValidation.cpp
    private/ShmemDuplexTransport.cpp
    private/ShmemTransport.cpp
    private/UdpTransport.cpp
)

add_library(library_${PROJECT_NAME}
    ${SOURCES}
    ${HEADERS}
    )

target_link_libraries(library_${PROJECT_NAME}
    PUBLIC
        pthread
        rt
)

target_include_directories(library_${PROJECT_NAME}
    PUBLIC public
    )

target_compile_features(library_${PROJECT_NAME}
    PUBLIC
        cxx_std_20
)

# ============================================================
# Test / example executables
# ============================================================

add_executable(test_${PROJECT_NAME}_publisher
    test/Publisher.cpp
)

target_link_libraries(test_${PROJECT_NAME}_publisher
    PRIVATE
        library_${PROJECT_NAME}
)

install(TARGETS test_${PROJECT_NAME}_publisher COMPONENT RoboticGui)

add_executable(test_${PROJECT_NAME}_subscriber
    test/Subscriber.cpp
)

target_link_libraries(test_${PROJECT_NAME}_subscriber
    PRIVATE
        library_${PROJECT_NAME}
)

install(TARGETS test_${PROJECT_NAME}_subscriber COMPONENT RoboticArm)

add_executable(test_${PROJECT_NAME}_shmem_reader
    test/ShmemReader.cpp
)

target_link_libraries(test_${PROJECT_NAME}_shmem_reader
    PRIVATE
        library_${PROJECT_NAME}
)

install(TARGETS test_${PROJECT_NAME}_shmem_reader COMPONENT RoboticArm)

add_executable(test_${PROJECT_NAME}_shmem_writer
    test/ShmemWriter.cpp
)

target_link_libraries(test_${PROJECT_NAME}_shmem_writer
    PRIVATE
        library_${PROJECT_NAME}
)

install(TARGETS test_${PROJECT_NAME}_shmem_writer COMPONENT RoboticArm)