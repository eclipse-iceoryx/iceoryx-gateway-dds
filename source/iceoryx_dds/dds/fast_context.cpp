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

#include "iceoryx_dds/dds/fast_context.hpp"
#include "MempoolPubSubTypes.h"
#include "iox/logging.hpp"

#include <fastdds/dds/domain/DomainParticipantFactory.hpp>

iox::dds::FastContext& iox::dds::FastContext::getInstance()
{
    static iox::dds::FastContext context;
    return context;
}

eprosima::fastdds::dds::DomainParticipant* iox::dds::FastContext::getParticipant()
{
    return m_participant;
}

eprosima::fastdds::dds::Topic* iox::dds::FastContext::getTopic(const std::string& topicName)
{
    auto topic = m_participant->find_topic(topicName, {0, 0});
    if (topic != nullptr)
    {
        return topic;
    }
    else
    {
        IOX_LOG(WARN, "[FastDataReader] Failed to find topic: " << topicName=;
    }

    topic = m_participant->create_topic(topicName, "Mempool::Chunk", eprosima::fastdds::dds::TOPIC_QOS_DEFAULT);
    if (topic == nullptr)
    {
        IOX_LOG(ERROR, "[FastDataReader] Failed to create topic: " << topicName);
    }

    return topic;
}

iox::dds::FastContext::FastContext()
{
    auto factory = eprosima::fastdds::dds::DomainParticipantFactory::get_instance();
    m_participant = factory->create_participant(0, eprosima::fastdds::dds::PARTICIPANT_QOS_DEFAULT);
    if (m_participant == nullptr)
    {
        IOX_LOG(ERROR, "[FastContext] Failed to create participant");
        return;
    }

    m_type = eprosima::fastdds::dds::TypeSupport(new Mempool::ChunkPubSubType());
    m_type.register_type(m_participant);
}
