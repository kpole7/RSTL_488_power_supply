cmake_minimum_required(VERSION 3.10)
project(powerSource200A_Svedberg)

# Compiler settings
set(CMAKE_CXX_STANDARD 11)
set(CMAKE_C_STANDARD 99)

# Flags for various configurations
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Debug) # Domyślna konfiguracja
endif()

set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -Wall -Wextra -g")
set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} -Wall -Wextra")
#set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} -Wall -Wextra -O3")

set(CMAKE_C_FLAGS_DEBUG "${CMAKE_C_FLAGS_DEBUG} -Wall -pthread -g")
set(CMAKE_C_FLAGS_RELEASE "${CMAKE_C_FLAGS_RELEASE} -Wall -pthread")
#set(CMAKE_C_FLAGS_RELEASE "${CMAKE_C_FLAGS_RELEASE} -Wall -pthread -O3")

# Required libraries
find_package(Threads REQUIRED)
find_package(FLTK REQUIRED)
find_package(X11 REQUIRED)

# Header file paths
include_directories(
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/freeModbus/include
    ${CMAKE_CURRENT_SOURCE_DIR}/freeModbus/tcp
    ${CMAKE_CURRENT_SOURCE_DIR}/freeModbus/port
)

# Source files
set(CSRC
    modbusTcpSlave.c
    freeModbus/port/portother.c
    freeModbus/port/portevent.c
    freeModbus/port/porttcp.c
    freeModbus/mb.c
    freeModbus/tcp/mbtcp.c
    freeModbus/functions/mbfunccoils.c
    freeModbus/functions/mbfuncdiag.c
    freeModbus/functions/mbfuncholding.c
    freeModbus/functions/mbfuncinput.c
    freeModbus/functions/mbfuncother.c
    freeModbus/functions/mbfuncdisc.c
    freeModbus/functions/mbutils.c
)

set(CCSRC
    powerSource200A_Svedberg.cpp
    modbusRtuMaster.cpp
    multiChannel.cpp
    dataSharingInterface.cpp
    graphicalUserInterface.cpp
    modbusTcpMaster.cpp
    git_revision.cpp
)

# The main executable
add_executable(powerSource200A_Svedberg ${CCSRC} ${CSRC})

# Libraries linking
target_link_libraries(powerSource200A_Svedberg
    ${FLTK_LIBRARIES}
    ${CMAKE_THREAD_LIBS_INIT}
    ${X11_LIBRARIES}
)

