cmake_minimum_required(VERSION 3.10)
project(vnetd C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wall -Wextra -O2 -D_GNU_SOURCE")

find_package(PkgConfig REQUIRED)
pkg_check_modules(SODIUM REQUIRED libsodium)

# Source files
set(SOURCES
    src/main.c
    src/tun.c
    src/udp.c
    src/packet.c
    src/buffer.c
    src/reactor.c
    src/mtu.c
    src/crypto.c
    src/replay.c
    src/peer.c
    src/handshake.c
    src/timers.c
    src/route.c
    src/system.c
)

add_executable(vnetd ${SOURCES})
target_link_libraries(vnetd ${SODIUM_LIBRARIES})
target_include_directories(vnetd PUBLIC ${SODIUM_INCLUDE_DIRS} src)
