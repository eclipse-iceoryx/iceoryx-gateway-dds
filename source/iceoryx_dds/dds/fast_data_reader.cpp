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

#include "iceoryx_dds/dds/fast_data_reader.hpp"
#include "Mempool.h"
#include "iceoryx_dds/dds/fast_context.hpp"
#include "iceoryx_dds/internal/log/logging.hpp"
#include "iceoryx_posh/mepoo/chunk_header.hpp"

#include <fastdds/dds/subscriber/Subscriber.hpp>

iox::dds::FastDataReader::FastDataReader(const capro::IdString_t serviceId,
                                         const capro::IdString_t instanceId,
                                         const capro::IdString_t eventId) noexcept
    : m_serviceId(serviceId)
    , m_instanceId(instanceId)
    , m_eventId(eventId)
{
    LogDebug() << "[FastDataReader] Created FastDataReader.";
}

iox::dds::FastDataReader::~FastDataReader()
{
    if (m_reader != nullptr)
    {
        m_subscriber->delete_datareader(m_reader);
    }
    if (m_subscriber != nullptr)
    {
        FastContext::getParticipant()->delete_subscriber(m_subscriber);
    }
    LogDebug() << "[FastDataReader] Destroyed FastDataReader.";
}

void iox::dds::FastDataReader::connect() noexcept
{
    if (!m_isConnected.load(std::memory_order_relaxed))
    {
        auto participant = FastContext::getParticipant();
        if (participant == nullptr)
        {
            LogError() << "[FastDataReader] Failed to get participant";
            return;
        }

        auto topicString =
            "/" + std::string(m_serviceId) + "/" + std::string(m_instanceId) + "/" + std::string(m_eventId);
        m_topic = participant->find_topic(topicString, {0, 0});
        if (m_topic == nullptr)
        {
            LogError() << "[FastDataReader] Failed to find topic: " << topicString;
            return;
        }

        m_subscriber = participant->create_subscriber(eprosima::fastdds::dds::SUBSCRIBER_QOS_DEFAULT);
        if (m_subscriber == nullptr)
        {
            LogError() << "[FastDataReader] Failed to create subscriber";
            return;
        }

        /// Is required for the Gateway. When two iceoryx publisher are publishing on the same
        /// topic and one publisher is located on a remote iceoryx instance connected via a
        /// bidirectional dds gateway (iceoryx2dds & dds2iceoryx) then every sample is delivered
        /// twice to the local subscriber.
        /// Once via the local iceoryx publisher and once via dds2iceoryx which received the
        /// sample from the iceoryx2dds gateway. But when we ignore the local dds writer the
        /// sample is not forwarded to the local dds gateway and delivered a second time.
        auto pqos = participant->get_qos();
        pqos.properties().properties().emplace_back("fastdds.ignore_local_endpoints", "true");
        participant->set_qos(pqos);

        auto qos = eprosima::fastdds::dds::DATAREADER_QOS_DEFAULT;
        qos.history().kind = eprosima::fastdds::dds::KEEP_ALL_HISTORY_QOS;
        m_reader = m_subscriber->create_datareader(m_topic, qos);
        if (m_reader == nullptr)
        {
            LogError() << "[FastDataReader] Failed to create datareader";
            return;
        }

        LogDebug() << "[FastDataReader] Connected to topic: " << topicString;

        m_isConnected.store(true, std::memory_order_relaxed);
    }
}

iox::cxx::optional<iox::dds::IoxChunkDatagramHeader> iox::dds::FastDataReader::peekNextIoxChunkDatagramHeader() noexcept
{
    constexpr iox::cxx::nullopt_t NO_VALID_SAMPLE_AVAILABLE;

    FASTDDS_CONST_SEQUENCE(DataSeq, Mempool::Chunk);

    DataSeq dataSeq;
    eprosima::fastdds::dds::SampleInfoSeq infoSeq;

    // ensure to only read sample - do not take
    auto ret = m_reader->read(dataSeq, infoSeq, 1);
    if (ret != eprosima::fastrtps::types::ReturnCode_t::RETCODE_OK)
    {
        LogError() << "[FastDataReader] Failed to read sample, return code: " << ret();
        return NO_VALID_SAMPLE_AVAILABLE;
    }

    auto dropSample = [&] {
        // Return the sample back
        auto ret = m_reader->return_loan(dataSeq, infoSeq);
        if (ret != eprosima::fastrtps::types::ReturnCode_t::RETCODE_OK)
        {
            LogError() << "[FastDataReader] Failed to return loan, return code: " << ret();
            return NO_VALID_SAMPLE_AVAILABLE;
        }

        ret = m_reader->take(dataSeq, infoSeq, 1);
        if (ret != eprosima::fastrtps::types::ReturnCode_t::RETCODE_OK)
        {
            LogError() << "[FastDataReader] Failed to take sample, return code: " << ret();
            return NO_VALID_SAMPLE_AVAILABLE;
        }

        ret = m_reader->return_loan(dataSeq, infoSeq);
        if (ret != eprosima::fastrtps::types::ReturnCode_t::RETCODE_OK)
        {
            LogError() << "[FastDataReader] Failed to return loan, return code: " << ret();
            return NO_VALID_SAMPLE_AVAILABLE;
        }

        return NO_VALID_SAMPLE_AVAILABLE;
    };

    if (infoSeq.length() == 0)
    {
        LogError() << "[FastDataReader] received no sample! Dropped sample!";
        return dropSample();
    }

    if (!infoSeq[0].valid_data)
    {
        LogError() << "[FastDataReader] received invalid sample! Dropped sample!";
        return dropSample();
    }

    const auto& nextSample = dataSeq[0];
    auto& nextSamplePayload = nextSample.payload();
    auto nextSampleSize = nextSamplePayload.size();

    // Ignore samples with no payload
    if (nextSampleSize == 0)
    {
        LogError() << "[FastDataReader] received sample with size zero! Dropped sample!";
        return dropSample();
    }

    // Ignore Invalid IoxChunkDatagramHeader
    if (nextSampleSize < sizeof(iox::dds::IoxChunkDatagramHeader))
    {
        auto log = LogError();
        log << "[FastDataReader] invalid sample size! Must be at least sizeof(IoxChunkDatagramHeader) = "
            << sizeof(iox::dds::IoxChunkDatagramHeader) << " but got " << nextSampleSize;
        if (nextSampleSize >= 1)
        {
            log << "! Potential datagram version is " << static_cast<uint16_t>(nextSamplePayload[0])
                << "! Dropped sample!";
        }
        return dropSample();
    }

    iox::dds::IoxChunkDatagramHeader::Serialized_t serializedDatagramHeader;
    for (uint64_t i = 0U; i < serializedDatagramHeader.capacity(); ++i)
    {
        serializedDatagramHeader.emplace_back(nextSamplePayload[i]);
    }

    auto datagramHeader = iox::dds::IoxChunkDatagramHeader::deserialize(serializedDatagramHeader);

    if (datagramHeader.datagramVersion != iox::dds::IoxChunkDatagramHeader::DATAGRAM_VERSION)
    {
        LogError() << "[FastDataReader] received sample with incompatible IoxChunkDatagramHeader version! Received '"
                   << static_cast<uint16_t>(datagramHeader.datagramVersion) << "', expected '"
                   << static_cast<uint16_t>(iox::dds::IoxChunkDatagramHeader::DATAGRAM_VERSION) << "'! Dropped sample!";
        return dropSample();
    }

    if (datagramHeader.endianness != getEndianess())
    {
        LogError() << "[FastDataReader] received sample with incompatible endianess! Received '"
                   << EndianessString[static_cast<uint64_t>(datagramHeader.endianness)] << "', expected '"
                   << EndianessString[static_cast<uint64_t>(getEndianess())] << "'! Dropped sample!";
        return dropSample();
    }

    return datagramHeader;
}

bool iox::dds::FastDataReader::hasSamples() noexcept
{
    eprosima::fastdds::dds::SampleInfo info;
    return m_reader->get_first_untaken_info(&info) == eprosima::fastrtps::types::ReturnCode_t::RETCODE_OK;
}

iox::cxx::expected<iox::dds::DataReaderError>
iox::dds::FastDataReader::takeNext(const iox::dds::IoxChunkDatagramHeader datagramHeader,
                                   uint8_t* const userHeaderBuffer,
                                   uint8_t* const userPayloadBuffer) noexcept
{
    // validation checks
    if (!m_isConnected.load())
    {
        return iox::cxx::error<iox::dds::DataReaderError>(iox::dds::DataReaderError::NOT_CONNECTED);
    }
    // it is assume that peekNextIoxChunkDatagramHeader was called beforehand and that the provided datagramHeader
    // belongs to this sample
    if (datagramHeader.userHeaderSize > 0
        && (datagramHeader.userHeaderId == iox::mepoo::ChunkHeader::NO_USER_HEADER || userHeaderBuffer == nullptr))
    {
        return iox::cxx::error<iox::dds::DataReaderError>(
            iox::dds::DataReaderError::INVALID_BUFFER_PARAMETER_FOR_USER_HEADER);
    }
    if (datagramHeader.userPayloadSize > 0 && userPayloadBuffer == nullptr)
    {
        return iox::cxx::error<iox::dds::DataReaderError>(
            iox::dds::DataReaderError::INVALID_BUFFER_PARAMETER_FOR_USER_PAYLOAD);
    }

    // take next sample and copy into buffer
    FASTDDS_CONST_SEQUENCE(DataSeq, Mempool::Chunk);

    DataSeq dataSeq;
    eprosima::fastdds::dds::SampleInfoSeq infoSeq;
    auto ret = m_reader->take(dataSeq, infoSeq, 1);
    if (ret != eprosima::fastrtps::types::ReturnCode_t::RETCODE_OK)
    {
        // no samples available
        return iox::cxx::success<>();
    }

    auto returnSample = [&] { m_reader->return_loan(dataSeq, infoSeq); };

    if (infoSeq.length() == 0)
    {
        returnSample();
        // no samples available
        return iox::cxx::success<>();
    }

    if (!infoSeq[0].valid_data)
    {
        returnSample();
        return iox::cxx::error<iox::dds::DataReaderError>(iox::dds::DataReaderError::INVALID_DATA);
    }

    // valid size
    auto nextSample = dataSeq[0];
    auto samplePayload = nextSample.payload();
    auto sampleSize = samplePayload.size();
    if (sampleSize == 0)
    {
        returnSample();
        return iox::cxx::error<iox::dds::DataReaderError>(iox::dds::DataReaderError::INVALID_DATA);
    }
    if (sampleSize < sizeof(iox::dds::IoxChunkDatagramHeader))
    {
        returnSample();
        return iox::cxx::error<iox::dds::DataReaderError>(iox::dds::DataReaderError::INVALID_DATAGRAM_HEADER_SIZE);
    }

    iox::dds::IoxChunkDatagramHeader::Serialized_t serializedDatagramHeader;
    for (uint64_t i = 0U; i < serializedDatagramHeader.capacity(); ++i)
    {
        serializedDatagramHeader.emplace_back(samplePayload[i]);
    }

    auto actualDatagramHeader = iox::dds::IoxChunkDatagramHeader::deserialize(serializedDatagramHeader);

    iox::cxx::Ensures(datagramHeader.userHeaderId == actualDatagramHeader.userHeaderId);
    iox::cxx::Ensures(datagramHeader.userHeaderSize == actualDatagramHeader.userHeaderSize);
    iox::cxx::Ensures(datagramHeader.userPayloadSize == actualDatagramHeader.userPayloadSize);
    iox::cxx::Ensures(datagramHeader.userPayloadAlignment == actualDatagramHeader.userPayloadAlignment);

    auto dataSize = sampleSize - sizeof(iox::dds::IoxChunkDatagramHeader);
    auto bufferSize = datagramHeader.userHeaderSize + datagramHeader.userPayloadSize;

    if (bufferSize != dataSize)
    {
        returnSample();
        // provided buffer don't match
        return iox::cxx::error<iox::dds::DataReaderError>(iox::dds::DataReaderError::BUFFER_SIZE_MISMATCH);
    }

    // copy data into the provided buffer
    if (userHeaderBuffer)
    {
        auto userHeaderBytes = &samplePayload.data()[sizeof(iox::dds::IoxChunkDatagramHeader)];
        std::memcpy(userHeaderBuffer, userHeaderBytes, datagramHeader.userHeaderSize);
    }

    if (userPayloadBuffer)
    {
        auto userPayloadBytes =
            &samplePayload.data()[sizeof(iox::dds::IoxChunkDatagramHeader) + datagramHeader.userHeaderSize];
        std::memcpy(userPayloadBuffer, userPayloadBytes, datagramHeader.userPayloadSize);
    }

    returnSample();
    return iox::cxx::success<>();
}

iox::capro::IdString_t iox::dds::FastDataReader::getServiceId() const noexcept
{
    return m_serviceId;
}

iox::capro::IdString_t iox::dds::FastDataReader::getInstanceId() const noexcept
{
    return m_instanceId;
}

iox::capro::IdString_t iox::dds::FastDataReader::getEventId() const noexcept
{
    return m_eventId;
}
