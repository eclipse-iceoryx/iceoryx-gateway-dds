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

#ifndef FAST_CONTEXT_HPP
#define FAST_CONTEXT_HPP

#include <fastdds/dds/domain/DomainParticipant.hpp>

namespace iox
{
namespace dds
{
///
/// @brief The FastContext manages fast configurations and singleton artifacts shared throughout an application.
///
class FastContext
{
  public:
    ///
    /// @brief getParticipant Get the DDS Domain Participant for the current runtime.
    /// @return The DDS Domain Participant.
    ///
    static eprosima::fastdds::dds::DomainParticipant* getParticipant();

  private:
    static void initialize();

    static eprosima::fastdds::dds::DomainParticipant* m_participant;
    static eprosima::fastdds::dds::TypeSupport m_type;
};

} // namespace dds
} // namespace iox

#endif // CYCLONE_CONTEXT_HPP
