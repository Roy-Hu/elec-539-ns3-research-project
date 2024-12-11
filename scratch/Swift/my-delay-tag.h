#ifndef MY_DELAY_TAG_H
#define MY_DELAY_TAG_H

#include "ns3/tag.h"
#include "ns3/nstime.h"
#include "ns3/type-id.h"

namespace ns3 {

class MyDelayTag : public Tag
{
public:
  static TypeId GetTypeId (void)
  {
    static TypeId tid = TypeId ("ns3::MyDelayTag")
      .SetParent<Tag> ()
      .AddConstructor<MyDelayTag> ();
    return tid;
  }

  virtual TypeId GetInstanceTypeId (void) const
  {
    return GetTypeId ();
  }

  MyDelayTag ()
    : m_lastHopTimestampNs (0),
      m_maxDelayNs (0),
      m_hops (0)
  {
  }

  void SetLastHopTimestamp (uint64_t t)
  {
    m_lastHopTimestampNs = t;
  }

  uint64_t GetLastHopTimestamp () const
  {
    return m_lastHopTimestampNs;
  }

  void SetMaxDelay (uint64_t d)
  {
    m_maxDelayNs = d;
  }

  uint64_t GetMaxDelay () const
  {
    return m_maxDelayNs;
  }

  void AddHops ()
  {
    m_hops += 1;
  }

  uint64_t GetHops() const
  {
    return m_hops;
  }

  virtual uint32_t GetSerializedSize () const
  {
    // We are storing:
    // - m_lastHopTimestampNs: uint64_t (8 bytes)
    // - m_maxDelayNs: uint64_t (8 bytes)
    // - m_hops: uint16_t (2 bytes)
    // Total = 8 + 8 + 2 = 18 bytes
    return 24;
  }

  virtual void Serialize (TagBuffer i) const
  {
    i.WriteU64 (m_lastHopTimestampNs);
    i.WriteU64 (m_maxDelayNs);
    i.WriteU64 (m_hops);
  }

  virtual void Deserialize (TagBuffer i)
  {
    m_lastHopTimestampNs = i.ReadU64 ();
    m_maxDelayNs = i.ReadU64 ();
    m_hops = i.ReadU64 ();
  }

  virtual void Print (std::ostream &os) const
  {
    os << "LastHopTs=" << m_lastHopTimestampNs << "ns, MaxDelay=" << m_maxDelayNs << "ns, Hops=" << m_hops;
  }

private:
  uint64_t m_lastHopTimestampNs; // Time when we received at the last hop
  uint64_t m_maxDelayNs;         // Maximum per-hop delay encountered so far
  uint64_t m_hops;               // Number of hops traversed
};

} // namespace ns3

#endif // MY_DELAY_TAG_H
