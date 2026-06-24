cmake_minimum_required(VERSION 3.13)

project(libpolycall VERSION 1.0.0 LANGUAGES C)

# Set C standard
set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED True)

# Build type configuration
if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Release)
endif()

# Compiler flags
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wall -Wextra -Wpedantic")
set(CMAKE_C_FLAGS_DEBUG "${CMAKE_C_FLAGS_DEBUG} -g")
set(CMAKE_C_FLAGS_RELEASE "${CMAKE_C_FLAGS_RELEASE} -O3")

# Find required components
find_package(Threads REQUIRED)
find_package(OpenSSL REQUIRED)

# Define module source directories
set(CORE_SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/src/core")
set(CLI_SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/src/cli")
set(MICRO_SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/src/micro")
set(NETWORK_SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/src/network")
set(PARSER_SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/src/parser")
set(PROTOCOL_SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/src/protocol")
set(STATE_SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/src/state")

# Include directories
include_directories(
  "${CMAKE_CURRENT_SOURCE_DIR}/include"
  "${CMAKE_CURRENT_SOURCE_DIR}/include/core"
  "${CMAKE_CURRENT_SOURCE_DIR}/include/cli"
  "${CMAKE_CURRENT_SOURCE_DIR}/include/micro"
  "${CMAKE_CURRENT_SOURCE_DIR}/include/network"
  "${CMAKE_CURRENT_SOURCE_DIR}/include/parser"
  "${CMAKE_CURRENT_SOURCE_DIR}/include/protocol"
  "${CMAKE_CURRENT_SOURCE_DIR}/include/state"
)

# Source files for each module
file(GLOB CORE_SOURCES "${CORE_SOURCE_DIR}/*.c")
file(GLOB MICRO_SOURCES "${MICRO_SOURCE_DIR}/*.c")
file(GLOB NETWORK_SOURCES "${NETWORK_SOURCE_DIR}/*.c")
file(GLOB PARSER_SOURCES "${PARSER_SOURCE_DIR}/*.c")
file(GLOB PROTOCOL_SOURCES "${PROTOCOL_SOURCE_DIR}/*.c")
file(GLOB STATE_SOURCES "${STATE_SOURCE_DIR}/*.c")
file(GLOB CLI_SOURCES "${CLI_SOURCE_DIR}/*.c")

# Combine all library sources (excluding CLI)
set(LIB_SOURCES
  ${CORE_SOURCES}
  ${MICRO_SOURCES}
  ${NETWORK_SOURCES}
  ${PARSER_SOURCES}
  ${PROTOCOL_SOURCES}
  ${STATE_SOURCES}
)

# Library target
add_library(polycall ${LIB_SOURCES})

# Library properties
set_target_properties(polycall PROPERTIES
    VERSION ${PROJECT_VERSION}
    SOVERSION 1
    PUBLIC_HEADER "include/core/polycall.h"
)

# Link dependencies
target_link_libraries(polycall
  Threads::Threads
  OpenSSL::SSL
  OpenSSL::Crypto
)

# Define the CLI executable
add_executable(polycall_cli "${CLI_SOURCE_DIR}/main.c")

# Link CLI with the library
target_link_libraries(polycall_cli polycall)

# Rename CLI executable
set_target_properties(polycall_cli PROPERTIES
    OUTPUT_NAME polycall
)

# Installation targets
include(GNUInstallDirs)
set(INSTALL_CONFIGDIR ${CMAKE_INSTALL_LIBDIR}/cmake/${PROJECT_NAME})

# Install the library
install(TARGETS polycall
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
    PUBLIC_HEADER DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/polycall
)

# Install additional headers
install(DIRECTORY include/
    DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/polycall
    FILES_MATCHING PATTERN "*.h"
)

# Install the CLI executable
install(TARGETS polycall_cli
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)

# Export targets for downstream usage
install(EXPORT polycall-targets
    FILE PolycallTargets.cmake
    NAMESPACE Polycall::
    DESTINATION ${INSTALL_CONFIGDIR}
)

# Package configuration file
include(CMakePackageConfigHelpers)
configure_package_config_file(${CMAKE_CURRENT_SOURCE_DIR}/cmake/PolycallConfig.cmake.in
    ${CMAKE_CURRENT_BINARY_DIR}/PolycallConfig.cmake
    INSTALL_DESTINATION ${INSTALL_CONFIGDIR}
)

# Package version file
write_basic_package_version_file(
    ${CMAKE_CURRENT_BINARY_DIR}/PolycallConfigVersion.cmake
    VERSION ${PROJECT_VERSION}
    COMPATIBILITY AnyNewerVersion
)

install(FILES
    ${CMAKE_CURRENT_BINARY_DIR}/PolycallConfig.cmake
    ${CMAKE_CURRENT_BINARY_DIR}/PolycallConfigVersion.cmake
    DESTINATION ${INSTALL_CONFIGDIR}
)

# Export package for use from the build tree
export(EXPORT polycall-targets FILE ${CMAKE_CURRENT_BINARY_DIR}/PolycallTargets.cmake NAMESPACE Polycall::)