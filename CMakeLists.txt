# Copyright (c) 2020 by Robert Bosch GmbH. All rights reserved.
# Copyright (c) 2020 - 2022 by Apex.AI Inc. All rights reserved.
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
find_package(iceoryx_hoofs QUIET)
find_package(iceoryx_dust QUIET)
find_package(iceoryx_posh QUIET)

# Link iceoryx_hoofs and iceoryx_posh libraries statically
option(BUILD_SHARED_LIBS "Link libraries dynamically" OFF)

# fetch iceoryx if not found
if(NOT iceoryx_hoofs_FOUND OR NOT iceoryx_posh_FOUND OR NOT iceoryx_dust_FOUND)
    if(iceoryx_hoofs_FOUND)
        message(FATAL_ERROR "iceoryx_hoofs was not found with 'find_package' but other parts were found!")
    endif()
    if(iceoryx_dust_FOUND)
        message(FATAL_ERROR "iceoryx_dust was not found with 'find_package' but other parts were found!")
    endif()
    if(iceoryx_posh_FOUND)
        message(FATAL_ERROR "iceoryx_posh was not found with 'find_package' but other parts were found!")
    endif()

    include(FetchContent)
    FetchContent_Declare(
        iceoryx
        GIT_REPOSITORY https://github.com/eclipse-iceoryx/iceoryx.git
        # Recent commit used here to be able to use the CMake macros. Change this to v3.x once released.
        GIT_TAG 048bf7d9b44a1b170cb892e2c374b460185e326a
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
    add_subdirectory(${iceoryx_SOURCE_DIR}/iceoryx_hoofs ${iceoryx_BINARY_DIR}/iceoryx_hoofs)
    add_subdirectory(${iceoryx_SOURCE_DIR}/iceoryx_dust  ${iceoryx_BINARY_DIR}/iceoryx_dust)
    add_subdirectory(${iceoryx_SOURCE_DIR}/iceoryx_posh  ${iceoryx_BINARY_DIR}/iceoryx_posh)
    
    find_package(iceoryx_hoofs REQUIRED)
    find_package(iceoryx_dust REQUIRED)
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
option(USE_CYCLONE_DDS "Bind to CycloneDDS implementation" ON)


if(USE_CYCLONE_DDS)
    add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/cmake/cyclonedds ${CMAKE_BINARY_DIR}/dependencies/cyclonedds/prebuild)
    message(INFO " Using CycloneDDS stack")
    find_package(CycloneDDS CONFIG REQUIRED)
    find_package(CycloneDDS-CXX CONFIG REQUIRED)
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
    FILES
        source/iceoryx_dds/log/logging.cpp
    )

if(USE_CYCLONE_DDS)
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
add_subdirectory(test)

if(ICEORYX_WITH_FETCH_CONTENT)
message("
#############################################################
The project was build by obtaining iceoryx with FetchContent.
If you want to use an existing installation use
'-DCMAKE_PREFIX_PATH=/path/to/installed/iceoryx'!
#############################################################
")
endif()

