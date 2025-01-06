# Copyright (c) 2020 by Robert Bosch GmbH. All rights reserved.
# Copyright (c) 2020 - 2022 by Apex.AI Inc. All rights reserved.
# Copyright (c) 2024 by Wei Long Meng. All rights reserved.
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#     http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.
#
# SPDX-License-Identifier: Apache-2.0

cmake_minimum_required(VERSION 3.16)

# check if iceoryx is in CMAKE_PREFIX_PATH
find_package(iceoryx_platform QUIET)
find_package(iceoryx_hoofs QUIET)
find_package(iceoryx_posh QUIET)

# Link iceoryx_hoofs and iceoryx_posh libraries statically
option(BUILD_SHARED_LIBS "Link libraries dynamically" OFF)

# fetch iceoryx if not found
if(NOT iceoryx_platform_FOUND OR NOT iceoryx_hoofs_FOUND OR NOT iceoryx_posh_FOUND)
    if(iceoryx_platform_FOUND)
        message(FATAL_ERROR "iceoryx_platform was not found with 'find_package' but other parts were found!")
    endif()
    if(iceoryx_hoofs_FOUND)
        message(FATAL_ERROR "iceoryx_hoofs was not found with 'find_package' but other parts were found!")
    endif()
    if(iceoryx_posh_FOUND)
        message(FATAL_ERROR "iceoryx_posh was not found with 'find_package' but other parts were found!")
    endif()

    include(FetchContent)
    FetchContent_Declare(
        iceoryx
        GIT_REPOSITORY https://github.com/eclipse-iceoryx/iceoryx.git
        GIT_TAG v2.95.3
    )
    FetchContent_GetProperties(iceoryx)
    if (NOT iceoryx_POPULATED)
        message(STATUS "updating: iceoryx" )
        FetchContent_Populate(iceoryx)
    endif()

    set(ICEORYX_WITH_FETCH_CONTENT true CACHE INTERNAL "")
    set(iceoryx_SOURCE_DIR ${iceoryx_SOURCE_DIR} CACHE INTERNAL "")
    set(iceoryx_BINARY_DIR ${iceoryx_BINARY_DIR} CACHE INTERNAL "")
    set(TEMP_DIR ${CMAKE_CURRENT_SOURCE_DIR})
    set(CMAKE_CURRENT_SOURCE_DIR ${iceoryx_SOURCE_DIR}/iceoryx_meta/ CACHE INTERNAL "")
    include(${iceoryx_SOURCE_DIR}/iceoryx_meta/tests.cmake)
    set(CMAKE_CURRENT_SOURCE_DIR ${TEMP_DIR} CACHE INTERNAL "")
endif()

if(ICEORYX_WITH_FETCH_CONTENT)
    # we do not need to build the examples
    set(EXAMPLES OFF)

    add_subdirectory(${iceoryx_SOURCE_DIR}/iceoryx_platform ${iceoryx_BINARY_DIR}/iceoryx_platform)
    add_subdirectory(${iceoryx_SOURCE_DIR}/iceoryx_hoofs ${iceoryx_BINARY_DIR}/iceoryx_hoofs)
    add_subdirectory(${iceoryx_SOURCE_DIR}/iceoryx_posh  ${iceoryx_BINARY_DIR}/iceoryx_posh)
    
    find_package(iceoryx_platform REQUIRED)
    find_package(iceoryx_hoofs REQUIRED)
    find_package(iceoryx_posh REQUIRED)
endif()

include(IceoryxPackageHelper)
include(IceoryxPlatform)

project(iceoryx_dds VERSION 0.1)

set(PREFIX iceoryx/v${CMAKE_PROJECT_VERSION})

if(CMAKE_SYSTEM_NAME MATCHES Linux OR CMAKE_SYSTEM_NAME MATCHES Darwin)
    option(BUILD_SHARED_LIBS "Create shared libraries by default" ON)
endif()

#
########## feature flags ##########
#
set(ALLOWED_VALUES CYCLONE_DDS FAST_DDS)
set(DDS_STACK CYCLONE_DDS CACHE STRING "Activate use of DDS stack")
set_property(CACHE DDS_STACK PROPERTY STRINGS CYCLONE_DDS FAST_DDS)
# Check that specified value is allowed
if(NOT DDS_STACK IN_LIST ALLOWED_VALUES)
    message(FATAL_ERROR, "Wrong configuration of DDS_STACK. Allowed values: ${ALLOWED_VALUES}")
endif()

if(${DDS_STACK} STREQUAL "CYCLONE_DDS")
    add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/cmake/cyclonedds ${CMAKE_BINARY_DIR}/dependencies/cyclonedds/prebuild)
    message(INFO " Using CycloneDDS stack")
    find_package(CycloneDDS CONFIG REQUIRED)
    find_package(CycloneDDS-CXX CONFIG REQUIRED)
elseif(${DDS_STACK} STREQUAL "FAST_DDS")
    add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/cmake/fastdds ${CMAKE_BINARY_DIR}/dependencies/fastdds/prebuild)
    message(INFO " Using FastDDS stack")
    find_package(fastrtps CONFIG REQUIRED)
endif()

#
########## build building-block library ##########
#
iox_add_library(
    NO_FIND_PACKAGE_SUPPORT
    TARGET                  iceoryx_dds
    NAMESPACE               iceoryx_dds
    PROJECT_PREFIX          ${PREFIX}
    BUILD_INTERFACE         ${CMAKE_CURRENT_SOURCE_DIR}/include
                            ${CMAKE_BINARY_DIR}/dependencies/install/include
    INSTALL_INTERFACE       include/${PREFIX}
    PRIVATE_LIBS            iceoryx_posh::iceoryx_posh
                            iceoryx_posh::iceoryx_posh_config
                            iceoryx_posh::iceoryx_posh_gateway
                            iceoryx_hoofs::iceoryx_hoofs
    )

if(${DDS_STACK} STREQUAL "CYCLONE_DDS")
    target_sources(iceoryx_dds
        PRIVATE
        source/iceoryx_dds/dds/cyclone_context.cpp
        source/iceoryx_dds/dds/cyclone_data_reader.cpp
        source/iceoryx_dds/dds/cyclone_data_writer.cpp
        source/iceoryx_dds/dds/iox_chunk_datagram_header.cpp
    )

    # Generate IDL at configure time
    set(MESSAGE_DEFINITION_DIR "${CMAKE_CURRENT_SOURCE_DIR}/msg")
    get_filename_component(MEMPOOL_IDL "${MESSAGE_DEFINITION_DIR}/Mempool.idl" ABSOLUTE)

    add_custom_command(
        OUTPUT "Mempool.hpp"
        COMMAND CycloneDDS::idlc
        ARGS -l $<TARGET_FILE:CycloneDDS-CXX::idlcxx> ${MEMPOOL_IDL}
        DEPENDS CycloneDDS::idlc CycloneDDS-CXX::idlcxx
        WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
        COMMENT "Generating serialization for Mempool bytestream"
        VERBATIM
    )

    add_custom_target(mempool_header ALL DEPENDS ${CMAKE_CURRENT_BINARY_DIR}/Mempool.hpp)

    add_dependencies(iceoryx_dds mempool_header)

    target_include_directories(iceoryx_dds PUBLIC
        $<BUILD_INTERFACE:${CMAKE_BINARY_DIR}>
        $<INSTALL_INTERFACE:include/${PREFIX}>
    )

    target_compile_definitions(iceoryx_dds PUBLIC -DUSE_CYCLONE_DDS)
    target_link_libraries(iceoryx_dds
        PUBLIC
        CycloneDDS-CXX::ddscxx
        iceoryx_hoofs::iceoryx_hoofs
    )
elseif(${DDS_STACK} STREQUAL "FAST_DDS")
    # Generate IDL at configure time
    set(MESSAGE_DEFINITION_DIR "${CMAKE_CURRENT_SOURCE_DIR}/msg")
    get_filename_component(MEMPOOL_IDL "${MESSAGE_DEFINITION_DIR}/Mempool.idl" ABSOLUTE)

    find_program(FASTDDS_GEN "fastddsgen")
    if(NOT FASTDDS_GEN)
        message(FATAL_ERROR "Could not find program fastddsgen in path")
    endif()

    add_custom_command(
        OUTPUT "Mempool.h" "Mempool.cxx" "MempoolPubSubTypes.h" "MempoolPubSubTypes.cxx"
        COMMAND ${FASTDDS_GEN}
        ARGS -replace ${MEMPOOL_IDL}
        WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
        COMMENT "Generating serialization for Mempool bytestream"
        VERBATIM
    )

    add_custom_target(mempool_files ALL DEPENDS
        ${CMAKE_CURRENT_BINARY_DIR}/Mempool.h
        ${CMAKE_CURRENT_BINARY_DIR}/Mempool.cxx
        ${CMAKE_CURRENT_BINARY_DIR}/MempoolPubSubTypes.h
        ${CMAKE_CURRENT_BINARY_DIR}/MempoolPubSubTypes.cxx
    )

    add_dependencies(iceoryx_dds mempool_files)

    target_sources(iceoryx_dds
        PRIVATE
        ${CMAKE_CURRENT_BINARY_DIR}/Mempool.cxx
        ${CMAKE_CURRENT_BINARY_DIR}/MempoolPubSubTypes.cxx
        source/iceoryx_dds/dds/fast_context.cpp
        source/iceoryx_dds/dds/fast_data_reader.cpp
        source/iceoryx_dds/dds/fast_data_writer.cpp
        source/iceoryx_dds/dds/iox_chunk_datagram_header.cpp
    )

    target_include_directories(iceoryx_dds PUBLIC
        $<BUILD_INTERFACE:${CMAKE_BINARY_DIR}>
        $<INSTALL_INTERFACE:include/${PREFIX}>
    )

    target_compile_definitions(iceoryx_dds PUBLIC -DUSE_FAST_DDS)
    target_link_libraries(iceoryx_dds
        PUBLIC
        fastrtps
        iceoryx_hoofs::iceoryx_hoofs
    )
endif()

#
########## build gateway app ##########
#
iox_add_executable(
    NO_PACKAGE_SETUP
    NO_FIND_PACKAGE_SUPPORT
    TARGET              iox-dds-gateway
    LIBS                iceoryx_posh::iceoryx_posh
                        iceoryx_posh::iceoryx_posh_gateway
                        iceoryx_posh::iceoryx_posh_config
                        iceoryx_dds::iceoryx_dds
    FILES
        source/gateway/main.cpp
    )

#
########## build test executables ##########
#
if(BUILD_TEST)
    add_subdirectory(test)
endif()

if(ICEORYX_WITH_FETCH_CONTENT)
message("
#############################################################
The project was build by obtaining iceoryx with FetchContent.
If you want to use an existing installation use
'-DCMAKE_PREFIX_PATH=/path/to/installed/iceoryx'!
#############################################################
")
endif()
