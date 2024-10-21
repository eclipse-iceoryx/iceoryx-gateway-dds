// Copyright (c) 2024 by Wei Long Meng. All rights reserved.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
//
// SPDX-License-Identifier: Apache-2.0

#ifndef IOX_DDS_DDS_CYCLONE_DATA_WRITER_HPP
#define IOX_DDS_DDS_CYCLONE_DATA_WRITER_HPP

#include "iceoryx_dds/dds/data_writer.hpp"

#include <fastdds/dds/publisher/DataWriter.hpp>

namespace iox
{
namespace dds
{
/// @brief Implementation of the DataWriter abstraction using the fastdds implementation.
class FastDataWriter : public iox::dds::DataWriter
{
  public:
    FastDataWriter() = delete;

    /// @brief Constructor to set fast data writer object from given IDs
    /// @param[in] serviceId ID of the service
    /// @param[in] instanceId ID of the instance of the service
    /// @param[in] eventId ID of the event
    FastDataWriter(const capro::IdString_t& serviceId,
                   const capro::IdString_t& instanceId,
                   const capro::IdString_t& eventId) noexcept;

    virtual ~FastDataWriter();

    FastDataWriter(const FastDataWriter&) = delete;
    FastDataWriter& operator=(const FastDataWriter&) = delete;
    FastDataWriter(FastDataWriter&& rhs) = default;
    FastDataWriter& operator=(FastDataWriter&& rhs) = default;

    /// @brief connect fast data writer to the underlying DDS network
    void connect() noexcept override;
    void write(iox::dds::IoxChunkDatagramHeader datagramHeader,
               const uint8_t* const userHeaderBytes,
               const uint8_t* const userPayloadBytes) noexcept override;

    capro::IdString_t getServiceId() const noexcept override;
    capro::IdString_t getInstanceId() const noexcept override;
    capro::IdString_t getEventId() const noexcept override;

  private:
    capro::IdString_t m_serviceId{""};
    capro::IdString_t m_instanceId{""};
    capro::IdString_t m_eventId{""};

    eprosima::fastdds::dds::Publisher* m_publisher = nullptr;
    eprosima::fastdds::dds::Topic* m_topic = nullptr;
    eprosima::fastdds::dds::DataWriter* m_writer = nullptr;
};

} // namespace dds
} // namespace iox

#endif // IOX_DDS_DDS_CYCLONE_DATA_WRITER_HPP
