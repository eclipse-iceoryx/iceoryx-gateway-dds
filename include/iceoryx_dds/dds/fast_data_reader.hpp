// Copyright (c) 2020 - 2021 by Robert Bosch GmbH. All rights reserved.
// Copyright (c) 2021 by Apex.AI Inc. All rights reserved.
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

#ifndef IOX_DDS_DDS_FAST_DATA_READER_HPP
#define IOX_DDS_DDS_FAST_DATA_READER_HPP

#include "iceoryx_dds/dds/data_reader.hpp"

#include <atomic>
#include <fastdds/dds/subscriber/DataReader.hpp>
#include <fastdds/dds/topic/Topic.hpp>

namespace iox
{
namespace dds
{
/// @brief Implementation of the DataReader abstraction using the fastdds implementation.
class FastDataReader : public DataReader
{
  public:
    FastDataReader() = delete;

    /// @brief Constructor to set fast data reader object from given IDs
    /// @param[in] serviceId ID of the service
    /// @param[in] instanceId ID of the instance of the service
    /// @param[in] eventId ID of the event
    FastDataReader(const capro::IdString_t serviceId,
                   const capro::IdString_t instanceId,
                   const capro::IdString_t eventId) noexcept;

    virtual ~FastDataReader();

    FastDataReader(const FastDataReader&) = delete;
    FastDataReader& operator=(const FastDataReader&) = delete;
    FastDataReader(FastDataReader&&) = delete;
    FastDataReader& operator=(FastDataReader&&) = delete;

    /// @brief Connect fast data reader to the underlying DDS network
    void connect() noexcept override;

    iox::cxx::optional<IoxChunkDatagramHeader> peekNextIoxChunkDatagramHeader() noexcept override;
    bool hasSamples() noexcept override;
    iox::cxx::expected<DataReaderError> takeNext(const IoxChunkDatagramHeader datagramHeader,
                                                 uint8_t* const userHeaderBuffer,
                                                 uint8_t* const userPayloadBuffer) noexcept override;

    capro::IdString_t getServiceId() const noexcept override;
    capro::IdString_t getInstanceId() const noexcept override;
    capro::IdString_t getEventId() const noexcept override;

  private:
    capro::IdString_t m_serviceId{""};
    capro::IdString_t m_instanceId{""};
    capro::IdString_t m_eventId{""};

    eprosima::fastdds::dds::Subscriber* m_subscriber = nullptr;
    eprosima::fastdds::dds::Topic* m_topic = nullptr;
    eprosima::fastdds::dds::DataReader* m_reader = nullptr;

    std::atomic_bool m_isConnected{false};
};

} // namespace dds
} // namespace iox

#endif // IOX_DDS_DDS_CYCLONE_DATA_READER_HPP
