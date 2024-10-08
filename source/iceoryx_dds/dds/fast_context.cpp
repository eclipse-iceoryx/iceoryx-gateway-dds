// Copyright (c) 2020 by Robert Bosch GmbH. All rights reserved.
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

#include <fastdds/dds/domain/DomainParticipantFactory.hpp>

eprosima::fastdds::dds::DomainParticipant* iox::dds::FastContext::m_participant = nullptr;
eprosima::fastdds::dds::TypeSupport iox::dds::FastContext::m_type;

eprosima::fastdds::dds::DomainParticipant* iox::dds::FastContext::getParticipant()
{
    static std::once_flag initialized;
    std::call_once(initialized, iox::dds::FastContext::initialize);

    return m_participant;
}

void iox::dds::FastContext::initialize()
{
    auto factory = eprosima::fastdds::dds::DomainParticipantFactory::get_instance();
    m_participant = factory->create_participant(0, eprosima::fastdds::dds::PARTICIPANT_QOS_DEFAULT);
    if (m_participant == nullptr)
    {
        return;
    }

    m_type = eprosima::fastdds::dds::TypeSupport(new Mempool::ChunkPubSubType());
    m_type.register_type(m_participant);
}
