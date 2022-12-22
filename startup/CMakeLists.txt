cmake_minimum_required(VERSION 3.22)

configure_file(arm_startup.sh.in arm_startup.sh @ONLY)
configure_file(arm_shutdown.sh.in arm_shutdown.sh @ONLY)

install(FILES ${CMAKE_CURRENT_BINARY_DIR}/arm_startup.sh 
    RENAME startup.sh
    DESTINATION .
    PERMISSIONS OWNER_READ OWNER_WRITE OWNER_EXECUTE GROUP_READ GROUP_EXECUTE WORLD_READ WORLD_EXECUTE
    COMPONENT RoboticArm
)

install(FILES ${CMAKE_CURRENT_BINARY_DIR}/arm_shutdown.sh 
    RENAME shutdown.sh
    DESTINATION .
    PERMISSIONS OWNER_READ OWNER_WRITE OWNER_EXECUTE GROUP_READ GROUP_EXECUTE WORLD_READ WORLD_EXECUTE
    COMPONENT RoboticArm
)