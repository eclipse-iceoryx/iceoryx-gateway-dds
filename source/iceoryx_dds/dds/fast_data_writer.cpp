// Copyright (c) 2020 by Robert Bosch GmbH. All rights reserved.
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

#include "iceoryx_dds/dds/fast_data_writer.hpp"
#include "Mempool.h"
#include "iceoryx_dds/dds/fast_context.hpp"
#include "iceoryx_dds/internal/log/logging.hpp"
#include "iceoryx_posh/mepoo/chunk_header.hpp"

#include <fastdds/dds/publisher/Publisher.hpp>
#include <string>

iox::dds::FastDataWriter::FastDataWriter(const capro::IdString_t serviceId,
                                         const capro::IdString_t instanceId,
                                         const capro::IdString_t eventId) noexcept
    : m_serviceId(serviceId)
    , m_instanceId(instanceId)
    , m_eventId(eventId)
{
    LogDebug() << "[FastDataWriter] Created FastDataWriter.";
}

iox::dds::FastDataWriter::~FastDataWriter()
{
    if (m_writer != nullptr)
    {
        m_publisher->delete_datawriter(m_writer);
    }
    if (m_topic != nullptr)
    {
        FastContext::getParticipant()->delete_topic(m_topic);
    }
    if (m_publisher != nullptr)
    {
        FastContext::getParticipant()->delete_publisher(m_publisher);
    }
    LogDebug() << "[FastDataWriter] Destroyed FastDataWriter.";
}

void iox::dds::FastDataWriter::connect() noexcept
{
    auto participant = FastContext::getParticipant();
    if (participant == nullptr)
    {
        LogError() << "[FastDataWriter] Failed to get participant";
        return;
    }

    m_publisher = participant->create_publisher(eprosima::fastdds::dds::PUBLISHER_QOS_DEFAULT);
    if (m_publisher == nullptr)
    {
        LogError() << "[FastDataWriter] Failed to create publisher";
        return;
    }

    auto topic = "/" + std::string(m_serviceId) + "/" + std::string(m_instanceId) + "/" + std::string(m_eventId);
    m_topic = participant->create_topic(topic, "Mempool::Chunk", eprosima::fastdds::dds::TOPIC_QOS_DEFAULT);
    if (m_topic == nullptr)
    {
        LogError() << "[FastDataWriter] Failed to create topic: " << topic;
        return;
    }

    m_writer = m_publisher->create_datawriter(m_topic, eprosima::fastdds::dds::DATAWRITER_QOS_DEFAULT);
    if (m_writer == nullptr)
    {
        LogError() << "[FastDataWriter] Failed to create datawriter";
        return;
    }

    LogDebug() << "[FastDataWriter] Connected to topic: " << topic;
}

void iox::dds::FastDataWriter::write(iox::dds::IoxChunkDatagramHeader datagramHeader,
                                     const uint8_t* const userHeaderBytes,
                                     const uint8_t* const userPayloadBytes) noexcept
{
    if (datagramHeader.userHeaderSize > 0
        && (datagramHeader.userHeaderId == iox::mepoo::ChunkHeader::NO_USER_HEADER || userHeaderBytes == nullptr))
    {
        LogError() << "[FastDataWriter] invalid user-header parameter! Dropping chunk!";
        return;
    }
    if (datagramHeader.userPayloadSize > 0 && userPayloadBytes == nullptr)
    {
        LogError() << "[FastDataWriter] invalid user-payload parameter! Dropping chunk!";
        return;
    }

    datagramHeader.endianness = getEndianess();

    auto serializedDatagramHeader = iox::dds::IoxChunkDatagramHeader::serialize(datagramHeader);
    auto datagramSize =
        serializedDatagramHeader.size() + datagramHeader.userHeaderSize + datagramHeader.userPayloadSize;

    auto chunk = Mempool::Chunk();
    chunk.payload().reserve(datagramSize);

    std::copy(serializedDatagramHeader.data(),
              serializedDatagramHeader.data() + serializedDatagramHeader.size(),
              std::back_inserter(chunk.payload()));
    if (datagramHeader.userHeaderSize > 0 && userHeaderBytes != nullptr)
    {
        std::copy(
            userHeaderBytes, userHeaderBytes + datagramHeader.userHeaderSize, std::back_inserter(chunk.payload()));
    }
    if (datagramHeader.userPayloadSize > 0 && userPayloadBytes != nullptr)
    {
        std::copy(
            userPayloadBytes, userPayloadBytes + datagramHeader.userPayloadSize, std::back_inserter(chunk.payload()));
    }

    if (!m_writer->write(&chunk))
    {
        LogError() << "[FastDataWriter] Failed to write chunk";
    }
}

iox::capro::IdString_t iox::dds::FastDataWriter::getServiceId() const noexcept
{
    return m_serviceId;
}

iox::capro::IdString_t iox::dds::FastDataWriter::getInstanceId() const noexcept
{
    return m_instanceId;
}

iox::capro::IdString_t iox::dds::FastDataWriter::getEventId() const noexcept
{
    return m_eventId;
}
