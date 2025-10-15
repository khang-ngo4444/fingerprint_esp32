cmake_minimum_required(VERSION 3.10)
project(SecureFingerprintStorage CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Find dependencies
find_package(SEAL 3.7 REQUIRED)
find_package(PahoMqttCpp REQUIRED)
find_package(PkgConfig REQUIRED)
pkg_check_modules(PQXX REQUIRED libpqxx)

# Add executable
add_executable(secure_fingerprint_storage main.cpp)

# Link libraries
target_link_libraries(secure_fingerprint_storage
    PRIVATE
    SEAL::seal
    PahoMqttCpp::paho-mqttpp3
    ${PQXX_LIBRARIES}
)

# Include directories
target_include_directories(secure_fingerprint_storage
    PRIVATE
    ${SEAL_INCLUDE_DIRS}
    ${PahoMqttCpp_INCLUDE_DIRS}
    ${PQXX_INCLUDE_DIRS}
)

# Set C++ flags for Raspberry Pi (optional, but recommended)
if(CMAKE_SYSTEM_PROCESSOR MATCHES "arm" OR CMAKE_SYSTEM_PROCESSOR MATCHES "aarch64")
    message(STATUS "Configuring for Raspberry Pi (ARM architecture)")
    target_compile_options(secure_fingerprint_storage PRIVATE -O3 -march=native)
endif()