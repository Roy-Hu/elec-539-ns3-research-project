#ifndef TCP_SWIFT_H
#define TCP_SWIFT_H

#include "ns3/tcp-congestion-ops.h"
#include "ns3/tcp-socket-state.h"
#include "ns3/nstime.h"
#include "ns3/log.h"
#include <cmath>

namespace ns3 {


class TcpSwift : public TcpCongestionOps
{
public:
  static TypeId GetTypeId (void);

  TcpSwift ();
  TcpSwift (const TcpSwift &other);

  virtual std::string GetName () const override;
  virtual Ptr<TcpCongestionOps> Fork () override;

  virtual void IncreaseWindow (Ptr<TcpSocketState> tcb, uint32_t segmentsAcked) override;
  virtual void PktsAcked (Ptr<TcpSocketState> tcb, uint32_t packetsAcked, const Time &rtt) override;
  virtual uint32_t GetSsThresh (Ptr<const TcpSocketState> tcb, uint32_t bytesInFlight) override;
  virtual void CongestionStateSet (Ptr<TcpSocketState> tcb, const TcpSocketState::TcpCongState_t newState) override;
  virtual void CwndEvent (Ptr<TcpSocketState> tcb, const TcpSocketState::TcpCAEvent_t event) override;
  virtual void Init (Ptr<TcpSocketState> tcb) override;
  void SetFlowIndex (uint32_t flowIndex) { m_flowIndex = flowIndex; }
  uint32_t GetFlowIndex () const { return m_flowIndex; }

private:
  void ComputeTargetDelay (Ptr<TcpSocketState> tcb, double &targetDelay);
  void SwiftUpdateCwndOnAck (Ptr<TcpSocketState> tcb, uint32_t segmentsAcked);
  bool CanDecrease ();
  uint32_t m_flowIndex;

  double m_ai;          // Additive increment
  double m_beta;        // Multiplicative decrease factor (for delay-based MD)
  double m_maxMdf;      // Maximum multiplicative decrease factor
  Time m_baseTarget;    // Base target delay
  uint32_t m_retxResetThreshold; 
  uint32_t m_retransmitCount;
  std::ofstream g_cwndFiles;
  double m_alpha;
  Time m_rtt;           
  bool m_canDecrease;   
  Time m_tLastDecrease; // Last time we performed MD
  
  // Minimum cwnd in segments, usually 1 segment
  uint32_t MinCwnd (Ptr<TcpSocketState> tcb) const { return tcb->m_segmentSize; }
};

} // namespace ns3

#endif // TCP_SWIFT_H
