project(sonar)

set(HEADERS
    public/libSonar.h
    )

set(SOURCES
    private/libSonar.cpp
    )

add_library(library_${PROJECT_NAME}
    ${SOURCES}
    ${HEADERS}
    )

target_include_directories(library_${PROJECT_NAME}
    PUBLIC public
    )