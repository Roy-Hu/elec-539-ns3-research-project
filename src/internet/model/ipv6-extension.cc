/*
 * Copyright (c) 2007-2009 Strasbourg University
 *
 * SPDX-License-Identifier: GPL-2.0-only
 *
 * Author: David Gross <gdavid.devel@gmail.com>
 */

#include "ipv6-extension.h"

#include "icmpv6-l4-protocol.h"
#include "ipv6-extension-demux.h"
#include "ipv6-extension-header.h"
#include "ipv6-header.h"
#include "ipv6-l3-protocol.h"
#include "ipv6-option-demux.h"
#include "ipv6-option.h"
#include "ipv6-route.h"

#include "ns3/assert.h"
#include "ns3/ipv6-address.h"
#include "ns3/log.h"
#include "ns3/object-vector.h"
#include "ns3/trace-source-accessor.h"
#include "ns3/uinteger.h"

#include <ctime>
#include <list>

namespace ns3
{

NS_LOG_COMPONENT_DEFINE("Ipv6Extension");

NS_OBJECT_ENSURE_REGISTERED(Ipv6Extension);

TypeId
Ipv6Extension::GetTypeId()
{
    static TypeId tid = TypeId("ns3::Ipv6Extension")
                            .SetParent<Object>()
                            .SetGroupName("Internet")
                            .AddAttribute("ExtensionNumber",
                                          "The IPv6 extension number.",
                                          UintegerValue(0),
                                          MakeUintegerAccessor(&Ipv6Extension::GetExtensionNumber),
                                          MakeUintegerChecker<uint8_t>());
    return tid;
}

Ipv6Extension::Ipv6Extension()
{
    m_uvar = CreateObject<UniformRandomVariable>();
}

Ipv6Extension::~Ipv6Extension()
{
}

void
Ipv6Extension::SetNode(Ptr<Node> node)
{
    NS_LOG_FUNCTION(this << node);

    m_node = node;
}

Ptr<Node>
Ipv6Extension::GetNode() const
{
    return m_node;
}

uint8_t
Ipv6Extension::ProcessOptions(Ptr<Packet>& packet,
                              uint8_t offset,
                              uint8_t length,
                              const Ipv6Header& ipv6Header,
                              Ipv6Address dst,
                              uint8_t* nextHeader,
                              bool& stopProcessing,
                              bool& isDropped,
                              Ipv6L3Protocol::DropReason& dropReason)
{
    NS_LOG_FUNCTION(this << packet << offset << length << ipv6Header << dst << nextHeader
                         << isDropped);

    // For ICMPv6 Error packets
    Ptr<Packet> malformedPacket = packet->Copy();
    malformedPacket->AddHeader(ipv6Header);
    Ptr<Icmpv6L4Protocol> icmpv6 = GetNode()->GetObject<Ipv6L3Protocol>()->GetIcmpv6();

    Ptr<Packet> p = packet->Copy();
    p->RemoveAtStart(offset);

    Ptr<Ipv6OptionDemux> ipv6OptionDemux = GetNode()->GetObject<Ipv6OptionDemux>();
    Ptr<Ipv6Option> ipv6Option;

    uint8_t processedSize = 0;
    uint32_t size = p->GetSize();
    auto data = new uint8_t[size];
    p->CopyData(data, size);

    uint8_t optionType = 0;
    uint8_t optionLength = 0;

    while (length > processedSize && !isDropped)
    {
        optionType = *(data + processedSize);
        ipv6Option = ipv6OptionDemux->GetOption(optionType);

        if (!ipv6Option)
        {
            optionType >>= 6;
            switch (optionType)
            {
            case 0:
                optionLength = *(data + processedSize + 1) + 2;
                break;

            case 1:
                NS_LOG_LOGIC("Unknown Option. Drop!");
                optionLength = 0;
                isDropped = true;
                stopProcessing = true;
                dropReason = Ipv6L3Protocol::DROP_UNKNOWN_OPTION;
                break;

            case 2:
                NS_LOG_LOGIC("Unknown Option. Drop!");
                icmpv6->SendErrorParameterError(malformedPacket,
                                                ipv6Header.GetSource(),
                                                Icmpv6Header::ICMPV6_UNKNOWN_OPTION,
                                                offset + processedSize);
                optionLength = 0;
                isDropped = true;
                stopProcessing = true;
                dropReason = Ipv6L3Protocol::DROP_UNKNOWN_OPTION;
                break;

            case 3:
                NS_LOG_LOGIC("Unknown Option. Drop!");

                if (!ipv6Header.GetDestination().IsMulticast())
                {
                    icmpv6->SendErrorParameterError(malformedPacket,
                                                    ipv6Header.GetSource(),
                                                    Icmpv6Header::ICMPV6_UNKNOWN_OPTION,
                                                    offset + processedSize);
                }
                optionLength = 0;
                isDropped = true;
                stopProcessing = true;
                dropReason = Ipv6L3Protocol::DROP_UNKNOWN_OPTION;
                break;

            default:
                break;
            }
        }
        else
        {
            optionLength =
                ipv6Option->Process(packet, offset + processedSize, ipv6Header, isDropped);
        }

        processedSize += optionLength;
        p->RemoveAtStart(optionLength);
    }

    delete[] data;

    return processedSize;
}

int64_t
Ipv6Extension::AssignStreams(int64_t stream)
{
    NS_LOG_FUNCTION(this << stream);
    m_uvar->SetStream(stream);
    return 1;
}

NS_OBJECT_ENSURE_REGISTERED(Ipv6ExtensionHopByHop);

TypeId
Ipv6ExtensionHopByHop::GetTypeId()
{
    static TypeId tid = TypeId("ns3::Ipv6ExtensionHopByHop")
                            .SetParent<Ipv6Extension>()
                            .SetGroupName("Internet")
                            .AddConstructor<Ipv6ExtensionHopByHop>();
    return tid;
}

Ipv6ExtensionHopByHop::Ipv6ExtensionHopByHop()
{
}

Ipv6ExtensionHopByHop::~Ipv6ExtensionHopByHop()
{
}

uint8_t
Ipv6ExtensionHopByHop::GetExtensionNumber() const
{
    return EXT_NUMBER;
}

uint8_t
Ipv6ExtensionHopByHop::Process(Ptr<Packet>& packet,
                               uint8_t offset,
                               const Ipv6Header& ipv6Header,
                               Ipv6Address dst,
                               uint8_t* nextHeader,
                               bool& stopProcessing,
                               bool& isDropped,
                               Ipv6L3Protocol::DropReason& dropReason)
{
    NS_LOG_FUNCTION(this << packet << offset << ipv6Header << dst << nextHeader << isDropped);

    Ptr<Packet> p = packet->Copy();
    p->RemoveAtStart(offset);

    Ipv6ExtensionHopByHopHeader hopbyhopHeader;
    p->RemoveHeader(hopbyhopHeader);
    if (nextHeader)
    {
        *nextHeader = hopbyhopHeader.GetNextHeader();
    }

    uint8_t processedSize = hopbyhopHeader.GetOptionsOffset();
    offset += processedSize;
    uint8_t length = hopbyhopHeader.GetLength() - hopbyhopHeader.GetOptionsOffset();

    processedSize += ProcessOptions(packet,
                                    offset,
                                    length,
                                    ipv6Header,
                                    dst,
                                    nextHeader,
                                    stopProcessing,
                                    isDropped,
                                    dropReason);

    return processedSize;
}

NS_OBJECT_ENSURE_REGISTERED(Ipv6ExtensionDestination);

TypeId
Ipv6ExtensionDestination::GetTypeId()
{
    static TypeId tid = TypeId("ns3::Ipv6ExtensionDestination")
                            .SetParent<Ipv6Extension>()
                            .SetGroupName("Internet")
                            .AddConstructor<Ipv6ExtensionDestination>();
    return tid;
}

Ipv6ExtensionDestination::Ipv6ExtensionDestination()
{
}

Ipv6ExtensionDestination::~Ipv6ExtensionDestination()
{
}

uint8_t
Ipv6ExtensionDestination::GetExtensionNumber() const
{
    return EXT_NUMBER;
}

uint8_t
Ipv6ExtensionDestination::Process(Ptr<Packet>& packet,
                                  uint8_t offset,
                                  const Ipv6Header& ipv6Header,
                                  Ipv6Address dst,
                                  uint8_t* nextHeader,
                                  bool& stopProcessing,
                                  bool& isDropped,
                                  Ipv6L3Protocol::DropReason& dropReason)
{
    NS_LOG_FUNCTION(this << packet << offset << ipv6Header << dst << nextHeader << isDropped);

    Ptr<Packet> p = packet->Copy();
    p->RemoveAtStart(offset);

    Ipv6ExtensionDestinationHeader destinationHeader;
    p->RemoveHeader(destinationHeader);
    if (nextHeader)
    {
        *nextHeader = destinationHeader.GetNextHeader();
    }

    uint8_t processedSize = destinationHeader.GetOptionsOffset();
    offset += processedSize;
    uint8_t length = destinationHeader.GetLength() - destinationHeader.GetOptionsOffset();

    processedSize += ProcessOptions(packet,
                                    offset,
                                    length,
                                    ipv6Header,
                                    dst,
                                    nextHeader,
                                    stopProcessing,
                                    isDropped,
                                    dropReason);

    return processedSize;
}

NS_OBJECT_ENSURE_REGISTERED(Ipv6ExtensionFragment);

TypeId
Ipv6ExtensionFragment::GetTypeId()
{
    static TypeId tid =
        TypeId("ns3::Ipv6ExtensionFragment")
            .SetParent<Ipv6Extension>()
            .SetGroupName("Internet")
            .AddConstructor<Ipv6ExtensionFragment>()
            .AddAttribute("FragmentExpirationTimeout",
                          "When this timeout expires, the fragments "
                          "will be cleared from the buffer.",
                          TimeValue(Seconds(60)),
                          MakeTimeAccessor(&Ipv6ExtensionFragment::m_fragmentExpirationTimeout),
                          MakeTimeChecker());
    return tid;
}

Ipv6ExtensionFragment::Ipv6ExtensionFragment()
{
}

Ipv6ExtensionFragment::~Ipv6ExtensionFragment()
{
}

void
Ipv6ExtensionFragment::DoDispose()
{
    NS_LOG_FUNCTION(this);

    for (auto it = m_fragments.begin(); it != m_fragments.end(); it++)
    {
        it->second = nullptr;
    }

    m_fragments.clear();
    m_timeoutEventList.clear();
    if (m_timeoutEvent.IsPending())
    {
        m_timeoutEvent.Cancel();
    }
    Ipv6Extension::DoDispose();
}

uint8_t
Ipv6ExtensionFragment::GetExtensionNumber() const
{
    return EXT_NUMBER;
}

uint8_t
Ipv6ExtensionFragment::Process(Ptr<Packet>& packet,
                               uint8_t offset,
                               const Ipv6Header& ipv6Header,
                               Ipv6Address dst,
                               uint8_t* nextHeader,
                               bool& stopProcessing,
                               bool& isDropped,
                               Ipv6L3Protocol::DropReason& dropReason)
{
    NS_LOG_FUNCTION(this << packet << offset << ipv6Header << dst << nextHeader << isDropped);

    Ptr<Packet> p = packet->Copy();
    p->RemoveAtStart(offset);

    Ipv6ExtensionFragmentHeader fragmentHeader;
    p->RemoveHeader(fragmentHeader);

    if (nextHeader)
    {
        *nextHeader = fragmentHeader.GetNextHeader();
    }

    bool moreFragment = fragmentHeader.GetMoreFragment();
    uint16_t fragmentOffset = fragmentHeader.GetOffset();
    uint32_t identification = fragmentHeader.GetIdentification();
    Ipv6Address src = ipv6Header.GetSource();

    FragmentKey_t fragmentKey = FragmentKey_t(src, identification);
    Ptr<Fragments> fragments;

    Ipv6Header ipHeader = ipv6Header;
    ipHeader.SetNextHeader(fragmentHeader.GetNextHeader());

    auto it = m_fragments.find(fragmentKey);
    if (it == m_fragments.end())
    {
        fragments = Create<Fragments>();
        m_fragments.insert(std::make_pair(fragmentKey, fragments));
        NS_LOG_DEBUG("Insert new fragment key: src: "
                     << src << " IP hdr id " << identification << " m_fragments.size() "
                     << m_fragments.size() << " offset " << fragmentOffset);
        auto iter = SetTimeout(fragmentKey, ipHeader);
        fragments->SetTimeoutIter(iter);
    }
    else
    {
        fragments = it->second;
    }

    if (fragmentOffset == 0)
    {
        Ptr<Packet> unfragmentablePart = packet->Copy();
        unfragmentablePart->RemoveAtEnd(packet->GetSize() - offset);
        fragments->SetUnfragmentablePart(unfragmentablePart);
    }

    NS_LOG_DEBUG("Add fragment with IP hdr id " << identification << " offset " << fragmentOffset);
    fragments->AddFragment(p, fragmentOffset, moreFragment);

    if (fragments->IsEntire())
    {
        packet = fragments->GetPacket();
        m_timeoutEventList.erase(fragments->GetTimeoutIter());
        m_fragments.erase(fragmentKey);
        NS_LOG_DEBUG("Finished fragment with IP hdr id "
                     << fragmentKey.second
                     << " erase timeout, m_fragments.size(): " << m_fragments.size());
        stopProcessing = false;
    }
    else
    {
        stopProcessing = true;
    }

    return 0;
}

void
Ipv6ExtensionFragment::GetFragments(Ptr<Packet> packet,
                                    Ipv6Header ipv6Header,
                                    uint32_t maxFragmentSize,
                                    std::list<Ipv6PayloadHeaderPair>& listFragments)
{
    NS_LOG_FUNCTION(this << packet << ipv6Header << maxFragmentSize);
    Ptr<Packet> p = packet->Copy();

    uint8_t nextHeader = ipv6Header.GetNextHeader();
    uint8_t ipv6HeaderSize = ipv6Header.GetSerializedSize();

    uint8_t type;
    p->CopyData(&type, sizeof(type));

    bool moreHeader = true;
    if (!(nextHeader == Ipv6Header::IPV6_EXT_HOP_BY_HOP ||
          nextHeader == Ipv6Header::IPV6_EXT_ROUTING ||
          (nextHeader == Ipv6Header::IPV6_EXT_DESTINATION && type == Ipv6Header::IPV6_EXT_ROUTING)))
    {
        moreHeader = false;
        ipv6Header.SetNextHeader(Ipv6Header::IPV6_EXT_FRAGMENTATION);
    }

    std::list<std::pair<Ipv6ExtensionHeader*, uint8_t>> unfragmentablePart;
    uint32_t unfragmentablePartSize = 0;

    Ptr<Ipv6ExtensionDemux> extensionDemux = GetNode()->GetObject<Ipv6ExtensionDemux>();
    Ptr<Ipv6Extension> extension = extensionDemux->GetExtension(nextHeader);
    uint8_t extensionHeaderLength;

    while (moreHeader)
    {
        if (nextHeader == Ipv6Header::IPV6_EXT_HOP_BY_HOP)
        {
            auto hopbyhopHeader = new Ipv6ExtensionHopByHopHeader();
            p->RemoveHeader(*hopbyhopHeader);

            nextHeader = hopbyhopHeader->GetNextHeader();
            extensionHeaderLength = hopbyhopHeader->GetLength();

            uint8_t type;
            p->CopyData(&type, sizeof(type));

            if (!(nextHeader == Ipv6Header::IPV6_EXT_HOP_BY_HOP ||
                  nextHeader == Ipv6Header::IPV6_EXT_ROUTING ||
                  (nextHeader == Ipv6Header::IPV6_EXT_DESTINATION &&
                   type == Ipv6Header::IPV6_EXT_ROUTING)))
            {
                moreHeader = false;
                hopbyhopHeader->SetNextHeader(Ipv6Header::IPV6_EXT_FRAGMENTATION);
            }

            unfragmentablePart.emplace_back(hopbyhopHeader, Ipv6Header::IPV6_EXT_HOP_BY_HOP);
            unfragmentablePartSize += extensionHeaderLength;
        }
        else if (nextHeader == Ipv6Header::IPV6_EXT_ROUTING)
        {
            Ptr<Ipv6ExtensionRoutingDemux> ipv6ExtensionRoutingDemux =
                GetNode()->GetObject<Ipv6ExtensionRoutingDemux>();

            uint8_t buf[4];
            p->CopyData(buf, sizeof(buf));
            uint8_t routingType = buf[2];

            Ipv6ExtensionRoutingHeader* routingHeader =
                ipv6ExtensionRoutingDemux->GetExtensionRoutingHeaderPtr(routingType);

            p->RemoveHeader(*routingHeader);

            nextHeader = routingHeader->GetNextHeader();
            extensionHeaderLength = routingHeader->GetLength();

            uint8_t type;
            p->CopyData(&type, sizeof(type));
            if (!(nextHeader == Ipv6Header::IPV6_EXT_HOP_BY_HOP ||
                  nextHeader == Ipv6Header::IPV6_EXT_ROUTING ||
                  (nextHeader == Ipv6Header::IPV6_EXT_DESTINATION &&
                   type == Ipv6Header::IPV6_EXT_ROUTING)))
            {
                moreHeader = false;
                routingHeader->SetNextHeader(Ipv6Header::IPV6_EXT_FRAGMENTATION);
            }

            unfragmentablePart.emplace_back(routingHeader, Ipv6Header::IPV6_EXT_ROUTING);
            unfragmentablePartSize += extensionHeaderLength;
        }
        else if (nextHeader == Ipv6Header::IPV6_EXT_DESTINATION)
        {
            auto destinationHeader = new Ipv6ExtensionDestinationHeader();
            p->RemoveHeader(*destinationHeader);

            nextHeader = destinationHeader->GetNextHeader();
            extensionHeaderLength = destinationHeader->GetLength();

            uint8_t type;
            p->CopyData(&type, sizeof(type));
            if (!(nextHeader == Ipv6Header::IPV6_EXT_HOP_BY_HOP ||
                  nextHeader == Ipv6Header::IPV6_EXT_ROUTING ||
                  (nextHeader == Ipv6Header::IPV6_EXT_DESTINATION &&
                   type == Ipv6Header::IPV6_EXT_ROUTING)))
            {
                moreHeader = false;
                destinationHeader->SetNextHeader(Ipv6Header::IPV6_EXT_FRAGMENTATION);
            }

            unfragmentablePart.emplace_back(destinationHeader, Ipv6Header::IPV6_EXT_DESTINATION);
            unfragmentablePartSize += extensionHeaderLength;
        }
    }

    Ipv6ExtensionFragmentHeader fragmentHeader;
    uint8_t fragmentHeaderSize = fragmentHeader.GetSerializedSize();

    uint32_t maxFragmentablePartSize =
        maxFragmentSize - ipv6HeaderSize - unfragmentablePartSize - fragmentHeaderSize;
    uint32_t currentFragmentablePartSize = 0;

    bool moreFragment = true;
    auto identification = (uint32_t)m_uvar->GetValue(0, (uint32_t)-1);
    uint16_t offset = 0;

    do
    {
        if (p->GetSize() > offset + maxFragmentablePartSize)
        {
            moreFragment = true;
            currentFragmentablePartSize = maxFragmentablePartSize;
            currentFragmentablePartSize -= currentFragmentablePartSize % 8;
        }
        else
        {
            moreFragment = false;
            currentFragmentablePartSize = p->GetSize() - offset;
        }

        fragmentHeader.SetNextHeader(nextHeader);
        fragmentHeader.SetOffset(offset);
        fragmentHeader.SetMoreFragment(moreFragment);
        fragmentHeader.SetIdentification(identification);

        Ptr<Packet> fragment = p->CreateFragment(offset, currentFragmentablePartSize);
        offset += currentFragmentablePartSize;

        fragment->AddHeader(fragmentHeader);

        for (auto it = unfragmentablePart.begin(); it != unfragmentablePart.end(); it++)
        {
            if (it->second == Ipv6Header::IPV6_EXT_HOP_BY_HOP)
            {
                auto p = dynamic_cast<Ipv6ExtensionHopByHopHeader*>(it->first);
                NS_ASSERT(p != nullptr);
                fragment->AddHeader(*p);
            }
            else if (it->second == Ipv6Header::IPV6_EXT_ROUTING)
            {
                auto p = dynamic_cast<Ipv6ExtensionLooseRoutingHeader*>(it->first);
                NS_ASSERT(p != nullptr);
                fragment->AddHeader(*p);
            }
            else if (it->second == Ipv6Header::IPV6_EXT_DESTINATION)
            {
                auto p = dynamic_cast<Ipv6ExtensionDestinationHeader*>(it->first);
                NS_ASSERT(p != nullptr);
                fragment->AddHeader(*p);
            }
        }

        ipv6Header.SetPayloadLength(fragment->GetSize());

        std::ostringstream oss;
        oss << ipv6Header;
        fragment->Print(oss);

        listFragments.emplace_back(fragment, ipv6Header);
    } while (moreFragment);

    for (auto it = unfragmentablePart.begin(); it != unfragmentablePart.end(); it++)
    {
        delete it->first;
    }

    unfragmentablePart.clear();
}

void
Ipv6ExtensionFragment::HandleFragmentsTimeout(FragmentKey_t fragmentKey, Ipv6Header ipHeader)
{
    NS_LOG_FUNCTION(this << fragmentKey.first << fragmentKey.second << ipHeader);
    Ptr<Fragments> fragments;

    auto it = m_fragments.find(fragmentKey);
    NS_ASSERT_MSG(it != m_fragments.end(),
                  "IPv6 Fragment timeout reached for non-existent fragment");
    fragments = it->second;

    Ptr<Packet> packet = fragments->GetPartialPacket();

    // if we have at least 8 bytes, we can send an ICMP.
    if (packet && packet->GetSize() > 8)
    {
        Ptr<Packet> p = packet->Copy();
        p->AddHeader(ipHeader);
        Ptr<Icmpv6L4Protocol> icmp = GetNode()->GetObject<Icmpv6L4Protocol>();
        icmp->SendErrorTimeExceeded(p, ipHeader.GetSource(), Icmpv6Header::ICMPV6_FRAGTIME);
    }

    Ptr<Ipv6L3Protocol> ipL3 = GetNode()->GetObject<Ipv6L3Protocol>();
    ipL3->ReportDrop(ipHeader, packet, Ipv6L3Protocol::DROP_FRAGMENT_TIMEOUT);

    // clear the buffers
    m_fragments.erase(fragmentKey);
}

Ipv6ExtensionFragment::FragmentsTimeoutsListI_t
Ipv6ExtensionFragment::SetTimeout(FragmentKey_t key, Ipv6Header ipHeader)
{
    NS_LOG_FUNCTION(this << key.first << key.second << ipHeader);
    if (m_timeoutEventList.empty())
    {
        NS_LOG_DEBUG("Scheduling timeout for IP hdr id "
                     << key.second << " at time "
                     << (Simulator::Now() + m_fragmentExpirationTimeout).GetSeconds());
        m_timeoutEvent = Simulator::Schedule(m_fragmentExpirationTimeout,
                                             &Ipv6ExtensionFragment::HandleTimeout,
                                             this);
    }
    NS_LOG_DEBUG("Adding timeout at "
                 << (Simulator::Now() + m_fragmentExpirationTimeout).GetSeconds() << " with key "
                 << key.second);
    m_timeoutEventList.emplace_back(Simulator::Now() + m_fragmentExpirationTimeout, key, ipHeader);

    auto iter = --m_timeoutEventList.end();

    return iter;
}

void
Ipv6ExtensionFragment::HandleTimeout()
{
    NS_LOG_FUNCTION(this);
    Time now = Simulator::Now();

    // std::list Time, Fragment_key_t, Ipv6Header
    // Fragment key is a pair: Ipv6Address, uint32_t ipHeaderId
    for (auto& element : m_timeoutEventList)
    {
        NS_LOG_DEBUG("Handle time " << std::get<0>(element).GetSeconds() << " IP hdr id "
                                    << std::get<1>(element).second);
    }
    while (!m_timeoutEventList.empty() && std::get<0>(*m_timeoutEventList.begin()) == now)
    {
        HandleFragmentsTimeout(std::get<1>(*m_timeoutEventList.begin()),
                               std::get<2>(*m_timeoutEventList.begin()));
        m_timeoutEventList.pop_front();
    }

    if (m_timeoutEventList.empty())
    {
        return;
    }

    Time difference = std::get<0>(*m_timeoutEventList.begin()) - now;
    NS_LOG_DEBUG("Scheduling later HandleTimeout at " << (now + difference).GetSeconds());
    m_timeoutEvent = Simulator::Schedule(difference, &Ipv6ExtensionFragment::HandleTimeout, this);
}

Ipv6ExtensionFragment::Fragments::Fragments()
    : m_moreFragment(false)
{
}

Ipv6ExtensionFragment::Fragments::~Fragments()
{
}

void
Ipv6ExtensionFragment::Fragments::AddFragment(Ptr<Packet> fragment,
                                              uint16_t fragmentOffset,
                                              bool moreFragment)
{
    NS_LOG_FUNCTION(this << fragment << fragmentOffset << moreFragment);
    std::list<std::pair<Ptr<Packet>, uint16_t>>::iterator it;

    for (it = m_packetFragments.begin(); it != m_packetFragments.end(); it++)
    {
        if (it->second > fragmentOffset)
        {
            break;
        }
    }

    if (it == m_packetFragments.end())
    {
        m_moreFragment = moreFragment;
    }

    m_packetFragments.insert(it, std::pair<Ptr<Packet>, uint16_t>(fragment, fragmentOffset));
}

void
Ipv6ExtensionFragment::Fragments::SetUnfragmentablePart(Ptr<Packet> unfragmentablePart)
{
    NS_LOG_FUNCTION(this << unfragmentablePart);
    m_unfragmentable = unfragmentablePart;
}

bool
Ipv6ExtensionFragment::Fragments::IsEntire() const
{
    bool ret = !m_moreFragment && !m_packetFragments.empty();

    if (ret)
    {
        uint16_t lastEndOffset = 0;

        for (auto it = m_packetFragments.begin(); it != m_packetFragments.end(); it++)
        {
            if (lastEndOffset != it->second)
            {
                ret = false;
                break;
            }

            lastEndOffset += it->first->GetSize();
        }
    }

    return ret;
}

Ptr<Packet>
Ipv6ExtensionFragment::Fragments::GetPacket() const
{
    Ptr<Packet> p = m_unfragmentable->Copy();

    for (auto it = m_packetFragments.begin(); it != m_packetFragments.end(); it++)
    {
        p->AddAtEnd(it->first);
    }

    return p;
}

Ptr<Packet>
Ipv6ExtensionFragment::Fragments::GetPartialPacket() const
{
    Ptr<Packet> p;

    if (m_unfragmentable)
    {
        p = m_unfragmentable->Copy();
    }
    else
    {
        return p;
    }

    uint16_t lastEndOffset = 0;

    for (auto it = m_packetFragments.begin(); it != m_packetFragments.end(); it++)
    {
        if (lastEndOffset != it->second)
        {
            break;
        }
        p->AddAtEnd(it->first);
        lastEndOffset += it->first->GetSize();
    }

    return p;
}

void
Ipv6ExtensionFragment::Fragments::SetTimeoutIter(FragmentsTimeoutsListI_t iter)
{
    NS_LOG_FUNCTION(this);
    m_timeoutIter = iter;
}

Ipv6ExtensionFragment::FragmentsTimeoutsListI_t
Ipv6ExtensionFragment::Fragments::GetTimeoutIter()
{
    return m_timeoutIter;
}

NS_OBJECT_ENSURE_REGISTERED(Ipv6ExtensionRouting);

TypeId
Ipv6ExtensionRouting::GetTypeId()
{
    static TypeId tid = TypeId("ns3::Ipv6ExtensionRouting")
                            .SetParent<Ipv6Extension>()
                            .SetGroupName("Internet")
                            .AddConstructor<Ipv6ExtensionRouting>();
    return tid;
}

Ipv6ExtensionRouting::Ipv6ExtensionRouting()
{
}

Ipv6ExtensionRouting::~Ipv6ExtensionRouting()
{
}

uint8_t
Ipv6ExtensionRouting::GetExtensionNumber() const
{
    return EXT_NUMBER;
}

uint8_t
Ipv6ExtensionRouting::GetTypeRouting() const
{
    return 0;
}

Ipv6ExtensionRoutingHeader*
Ipv6ExtensionRouting::GetExtensionRoutingHeaderPtr()
{
    return nullptr;
}

uint8_t
Ipv6ExtensionRouting::Process(Ptr<Packet>& packet,
                              uint8_t offset,
                              const Ipv6Header& ipv6Header,
                              Ipv6Address dst,
                              uint8_t* nextHeader,
                              bool& stopProcessing,
                              bool& isDropped,
                              Ipv6L3Protocol::DropReason& dropReason)
{
    NS_LOG_FUNCTION(this << packet << offset << ipv6Header << dst << nextHeader << isDropped);

    // For ICMPv6 Error Packets
    Ptr<Packet> malformedPacket = packet->Copy();
    malformedPacket->AddHeader(ipv6Header);

    Ptr<Packet> p = packet->Copy();
    p->RemoveAtStart(offset);

    uint8_t buf[4];
    packet->CopyData(buf, sizeof(buf));

    uint8_t routingNextHeader = buf[0];
    uint8_t routingLength = buf[1];
    uint8_t routingTypeRouting = buf[2];
    uint8_t routingSegmentsLeft = buf[3];

    if (nextHeader)
    {
        *nextHeader = routingNextHeader;
    }

    Ptr<Icmpv6L4Protocol> icmpv6 = GetNode()->GetObject<Ipv6L3Protocol>()->GetIcmpv6();

    Ptr<Ipv6ExtensionRoutingDemux> ipv6ExtensionRoutingDemux =
        GetNode()->GetObject<Ipv6ExtensionRoutingDemux>();
    Ptr<Ipv6ExtensionRouting> ipv6ExtensionRouting =
        ipv6ExtensionRoutingDemux->GetExtensionRouting(routingTypeRouting);

    if (!ipv6ExtensionRouting)
    {
        if (routingSegmentsLeft == 0)
        {
            isDropped = false;
        }
        else
        {
            NS_LOG_LOGIC("Malformed header. Drop!");

            icmpv6->SendErrorParameterError(malformedPacket,
                                            ipv6Header.GetSource(),
                                            Icmpv6Header::ICMPV6_MALFORMED_HEADER,
                                            offset + 1);
            dropReason = Ipv6L3Protocol::DROP_MALFORMED_HEADER;
            isDropped = true;
            stopProcessing = true;
        }

        return routingLength;
    }

    return ipv6ExtensionRouting->Process(packet,
                                         offset,
                                         ipv6Header,
                                         dst,
                                         (uint8_t*)nullptr,
                                         stopProcessing,
                                         isDropped,
                                         dropReason);
}

NS_OBJECT_ENSURE_REGISTERED(Ipv6ExtensionRoutingDemux);

TypeId
Ipv6ExtensionRoutingDemux::GetTypeId()
{
    static TypeId tid =
        TypeId("ns3::Ipv6ExtensionRoutingDemux")
            .SetParent<Object>()
            .SetGroupName("Internet")
            .AddAttribute("RoutingExtensions",
                          "The set of IPv6 Routing extensions registered with this demux.",
                          ObjectVectorValue(),
                          MakeObjectVectorAccessor(&Ipv6ExtensionRoutingDemux::m_extensionsRouting),
                          MakeObjectVectorChecker<Ipv6ExtensionRouting>());
    return tid;
}

Ipv6ExtensionRoutingDemux::Ipv6ExtensionRoutingDemux()
{
}

Ipv6ExtensionRoutingDemux::~Ipv6ExtensionRoutingDemux()
{
}

void
Ipv6ExtensionRoutingDemux::DoDispose()
{
    NS_LOG_FUNCTION(this);
    for (auto it = m_extensionsRouting.begin(); it != m_extensionsRouting.end(); it++)
    {
        (*it)->Dispose();
        *it = nullptr;
    }
    m_extensionsRouting.clear();
    m_node = nullptr;
    Object::DoDispose();
}

void
Ipv6ExtensionRoutingDemux::SetNode(Ptr<Node> node)
{
    NS_LOG_FUNCTION(this << node);
    m_node = node;
}

void
Ipv6ExtensionRoutingDemux::Insert(Ptr<Ipv6ExtensionRouting> extensionRouting)
{
    NS_LOG_FUNCTION(this << extensionRouting);
    m_extensionsRouting.push_back(extensionRouting);
}

Ptr<Ipv6ExtensionRouting>
Ipv6ExtensionRoutingDemux::GetExtensionRouting(uint8_t typeRouting)
{
    for (const auto& extRouting : m_extensionsRouting)
    {
        if (extRouting->GetTypeRouting() == typeRouting)
        {
            return extRouting;
        }
    }

    return nullptr;
}

Ipv6ExtensionRoutingHeader*
Ipv6ExtensionRoutingDemux::GetExtensionRoutingHeaderPtr(uint8_t typeRouting)
{
    NS_LOG_FUNCTION(this << typeRouting);

    for (const auto& extRouting : m_extensionsRouting)
    {
        if (extRouting->GetTypeRouting() == typeRouting)
        {
            return extRouting->GetExtensionRoutingHeaderPtr();
        }
    }

    return nullptr;
}

void
Ipv6ExtensionRoutingDemux::Remove(Ptr<Ipv6ExtensionRouting> extensionRouting)
{
    NS_LOG_FUNCTION(this << extensionRouting);
    m_extensionsRouting.remove(extensionRouting);
}

NS_OBJECT_ENSURE_REGISTERED(Ipv6ExtensionLooseRouting);

TypeId
Ipv6ExtensionLooseRouting::GetTypeId()
{
    static TypeId tid = TypeId("ns3::Ipv6ExtensionLooseRouting")
                            .SetParent<Ipv6ExtensionRouting>()
                            .SetGroupName("Internet")
                            .AddConstructor<Ipv6ExtensionLooseRouting>();
    return tid;
}

Ipv6ExtensionLooseRouting::Ipv6ExtensionLooseRouting()
{
}

Ipv6ExtensionLooseRouting::~Ipv6ExtensionLooseRouting()
{
}

uint8_t
Ipv6ExtensionLooseRouting::GetTypeRouting() const
{
    return TYPE_ROUTING;
}

Ipv6ExtensionRoutingHeader*
Ipv6ExtensionLooseRouting::GetExtensionRoutingHeaderPtr()
{
    return new Ipv6ExtensionLooseRoutingHeader();
}

uint8_t
Ipv6ExtensionLooseRouting::Process(Ptr<Packet>& packet,
                                   uint8_t offset,
                                   const Ipv6Header& ipv6Header,
                                   Ipv6Address dst,
                                   uint8_t* nextHeader,
                                   bool& stopProcessing,
                                   bool& isDropped,
                                   Ipv6L3Protocol::DropReason& dropReason)
{
    NS_LOG_FUNCTION(this << packet << offset << ipv6Header << dst << nextHeader << isDropped);

    // For ICMPv6 Error packets
    Ptr<Packet> malformedPacket = packet->Copy();
    malformedPacket->AddHeader(ipv6Header);

    Ptr<Packet> p = packet->Copy();
    p->RemoveAtStart(offset);

    // Copy IPv6 Header : ipv6Header -> ipv6header
    Buffer tmp;
    tmp.AddAtStart(ipv6Header.GetSerializedSize());
    Buffer::Iterator it = tmp.Begin();
    Ipv6Header ipv6header;
    ipv6Header.Serialize(it);
    ipv6header.Deserialize(it);

    // Get the number of routers' address field
    uint8_t buf[2];
    p->CopyData(buf, sizeof(buf));
    Ipv6ExtensionLooseRoutingHeader routingHeader;
    p->RemoveHeader(routingHeader);

    if (nextHeader)
    {
        *nextHeader = routingHeader.GetNextHeader();
    }

    Ptr<Icmpv6L4Protocol> icmpv6 = GetNode()->GetObject<Ipv6L3Protocol>()->GetIcmpv6();

    Ipv6Address srcAddress = ipv6header.GetSource();
    Ipv6Address destAddress = ipv6header.GetDestination();
    uint8_t hopLimit = ipv6header.GetHopLimit();
    uint8_t segmentsLeft = routingHeader.GetSegmentsLeft();
    uint8_t length = (routingHeader.GetLength() >> 3) - 1;
    uint8_t nbAddress = length / 2;
    uint8_t nextAddressIndex;
    Ipv6Address nextAddress;

    if (segmentsLeft == 0)
    {
        isDropped = false;
        return routingHeader.GetSerializedSize();
    }

    if (length % 2 != 0)
    {
        NS_LOG_LOGIC("Malformed header. Drop!");
        icmpv6->SendErrorParameterError(malformedPacket,
                                        srcAddress,
                                        Icmpv6Header::ICMPV6_MALFORMED_HEADER,
                                        offset + 1);
        dropReason = Ipv6L3Protocol::DROP_MALFORMED_HEADER;
        isDropped = true;
        stopProcessing = true;
        return routingHeader.GetSerializedSize();
    }

    if (segmentsLeft > nbAddress)
    {
        NS_LOG_LOGIC("Malformed header. Drop!");
        icmpv6->SendErrorParameterError(malformedPacket,
                                        srcAddress,
                                        Icmpv6Header::ICMPV6_MALFORMED_HEADER,
                                        offset + 3);
        dropReason = Ipv6L3Protocol::DROP_MALFORMED_HEADER;
        isDropped = true;
        stopProcessing = true;
        return routingHeader.GetSerializedSize();
    }

    routingHeader.SetSegmentsLeft(segmentsLeft - 1);
    nextAddressIndex = nbAddress - segmentsLeft;
    nextAddress = routingHeader.GetRouterAddress(nextAddressIndex);

    if (nextAddress.IsMulticast() || destAddress.IsMulticast())
    {
        dropReason = Ipv6L3Protocol::DROP_MALFORMED_HEADER;
        isDropped = true;
        stopProcessing = true;
        return routingHeader.GetSerializedSize();
    }

    routingHeader.SetRouterAddress(nextAddressIndex, destAddress);
    ipv6header.SetDestination(nextAddress);

    if (hopLimit <= 1)
    {
        NS_LOG_LOGIC("Time Exceeded : Hop Limit <= 1. Drop!");
        icmpv6->SendErrorTimeExceeded(malformedPacket, srcAddress, Icmpv6Header::ICMPV6_HOPLIMIT);
        dropReason = Ipv6L3Protocol::DROP_MALFORMED_HEADER;
        isDropped = true;
        stopProcessing = true;
        return routingHeader.GetSerializedSize();
    }

    ipv6header.SetHopLimit(hopLimit - 1);
    p->AddHeader(routingHeader);

    /* short-circuiting routing stuff
     *
     * If we process this option,
     * the packet was for us so we resend it to
     * the new destination (modified in the header above).
     */

    Ptr<Ipv6L3Protocol> ipv6 = GetNode()->GetObject<Ipv6L3Protocol>();
    Ptr<Ipv6RoutingProtocol> ipv6rp = ipv6->GetRoutingProtocol();
    Socket::SocketErrno err;
    NS_ASSERT(ipv6rp);

    Ptr<Ipv6Route> rtentry = ipv6rp->RouteOutput(p, ipv6header, nullptr, err);

    if (rtentry)
    {
        /* we know a route exists so send packet now */
        ipv6->SendRealOut(rtentry, p, ipv6header);
    }
    else
    {
        NS_LOG_INFO("No route for next router");
    }

    /* as we directly send packet, mark it as dropped */
    isDropped = true;

    return routingHeader.GetSerializedSize();
}

NS_OBJECT_ENSURE_REGISTERED(Ipv6ExtensionESP);

TypeId
Ipv6ExtensionESP::GetTypeId()
{
    static TypeId tid = TypeId("ns3::Ipv6ExtensionESP")
                            .SetParent<Ipv6Extension>()
                            .SetGroupName("Internet")
                            .AddConstructor<Ipv6ExtensionESP>();
    return tid;
}

Ipv6ExtensionESP::Ipv6ExtensionESP()
{
}

Ipv6ExtensionESP::~Ipv6ExtensionESP()
{
}

uint8_t
Ipv6ExtensionESP::GetExtensionNumber() const
{
    return EXT_NUMBER;
}

uint8_t
Ipv6ExtensionESP::Process(Ptr<Packet>& packet,
                          uint8_t offset,
                          const Ipv6Header& ipv6Header,
                          Ipv6Address dst,
                          uint8_t* nextHeader,
                          bool& stopProcessing,
                          bool& isDropped,
                          Ipv6L3Protocol::DropReason& dropReason)
{
    NS_LOG_FUNCTION(this << packet << offset << ipv6Header << dst << nextHeader << isDropped);

    /** \todo */

    return 0;
}

NS_OBJECT_ENSURE_REGISTERED(Ipv6ExtensionAH);

TypeId
Ipv6ExtensionAH::GetTypeId()
{
    static TypeId tid = TypeId("ns3::Ipv6ExtensionAH")
                            .SetParent<Ipv6Extension>()
                            .SetGroupName("Internet")
                            .AddConstructor<Ipv6ExtensionAH>();
    return tid;
}

Ipv6ExtensionAH::Ipv6ExtensionAH()
{
}

Ipv6ExtensionAH::~Ipv6ExtensionAH()
{
}

uint8_t
Ipv6ExtensionAH::GetExtensionNumber() const
{
    return EXT_NUMBER;
}

uint8_t
Ipv6ExtensionAH::Process(Ptr<Packet>& packet,
                         uint8_t offset,
                         const Ipv6Header& ipv6Header,
                         Ipv6Address dst,
                         uint8_t* nextHeader,
                         bool& stopProcessing,
                         bool& isDropped,
                         Ipv6L3Protocol::DropReason& dropReason)
{
    NS_LOG_FUNCTION(this << packet << offset << ipv6Header << dst << nextHeader << isDropped);

    /** \todo */

    return 1;
}

} /* namespace ns3 */
