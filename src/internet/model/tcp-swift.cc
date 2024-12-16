#include "tcp-swift.h"
#include "ns3/simulator.h"
#include "ns3/log.h"

namespace ns3 {

NS_LOG_COMPONENT_DEFINE ("TcpSwift");
NS_OBJECT_ENSURE_REGISTERED (TcpSwift);

TypeId
TcpSwift::GetTypeId (void)
{
  static TypeId tid = TypeId ("ns3::TcpSwift")
    .SetParent<TcpCongestionOps> ()
    .AddConstructor<TcpSwift> ()
    .AddAttribute ("AI", "Additive increment",
                   DoubleValue (2.0),
                   MakeDoubleAccessor (&TcpSwift::m_ai),
                   MakeDoubleChecker<double> ())
    .AddAttribute ("MaxMdf", "Maximum multiplicative decrease factor",
                   DoubleValue (1.0),
                   MakeDoubleAccessor (&TcpSwift::m_maxMdf),
                   MakeDoubleChecker<double> ())
    .AddAttribute ("BaseTarget", "Base target delay",
                   TimeValue (MicroSeconds (300)),
                   MakeTimeAccessor (&TcpSwift::m_baseTarget),
                   MakeTimeChecker ())
    .AddAttribute ("RetrxResetThreshold", "Reset threshold for retransmissions",
                   UintegerValue (3),
                   MakeUintegerAccessor (&TcpSwift::m_retxResetThreshold),
                   MakeUintegerChecker<uint32_t> ());
  return tid;
}

TcpSwift::TcpSwift ()
  : m_retransmitCount (0),
    m_canDecrease (false),
    m_tLastDecrease (Seconds (0))
{
  double denom = (1.0 / std::sqrt(0.1)) - (1.0 / std::sqrt(32.0));
  m_alpha = 4.0 / denom;
  m_beta = -1.0 * m_alpha / std::sqrt(32.0);

  NS_LOG_INFO("TcpSwift: alpha=" << m_alpha << ", beta=" << m_beta);
}

TcpSwift::TcpSwift (const TcpSwift &other)
  : TcpCongestionOps (other),
    m_ai (other.m_ai),
    m_beta (other.m_beta),
    m_maxMdf (other.m_maxMdf),
    m_baseTarget (other.m_baseTarget),
    m_retxResetThreshold (other.m_retxResetThreshold),
    m_retransmitCount (other.m_retransmitCount),
    m_alpha (other.m_alpha),
    m_rtt (other.m_rtt),
    m_canDecrease (other.m_canDecrease),
    m_tLastDecrease (other.m_tLastDecrease)
{
}

std::string
TcpSwift::GetName () const
{
  return "TcpSwift";
}

Ptr<TcpCongestionOps>
TcpSwift::Fork ()
{
  return CreateObject<TcpSwift> (*this);
}

void
TcpSwift::Init (Ptr<TcpSocketState> tcb)
{
  TcpCongestionOps::Init (tcb);
  m_retransmitCount = 0;
  m_canDecrease = false;
  m_tLastDecrease = Simulator::Now();
}

bool
TcpSwift::CanDecrease ()
{
  // can_decrease = (now - t_last_decrease > rtt)
  Time now = Simulator::Now();
  if (m_rtt.IsZero())
    {
      // If no RTT sample yet, assume we cannot decrease
      return false;
    }

  if (now - m_tLastDecrease >= m_rtt)
    {
      return true;
    }
  return false;
}

void
TcpSwift::ComputeTargetDelay (Ptr<TcpSocketState> tcb, double &targetDelay)
{
  double cwnd = (double) tcb->GetCwndInSegments();
  double term = (m_alpha / std::sqrt(cwnd)) + m_beta;

  term = std::min(4.0, term); // clamp at 0 minimum
  double baseTarget = m_baseTarget.GetSeconds();
  targetDelay = baseTarget * (1. + term) + (tcb->m_hops) * 8.25e-5 ;  // Simplified; add more logic if needed
  // targetDelay = baseTarget;
  // std::cout << "Computed target delay: " << targetDelay << std::endl;
  tcb->m_target_delay =  targetDelay;
}

void
TcpSwift::PktsAcked (Ptr<TcpSocketState> tcb, uint32_t packetsAcked, const Time &rtt)
{
  if (!rtt.IsZero ())
    {
      m_rtt = tcb->m_srtt.Get();
    }
}

void
TcpSwift::SwiftUpdateCwndOnAck (Ptr<TcpSocketState> tcb, uint32_t segmentsAcked)
{
  // On Receiving ACK:
  // retransmit_cnt <-0
  m_retransmitCount = 0;

  // double measuredDelay = (tcb->m_maxDelay);
  // measuredDelay = measuredDelay / 1e9; // Convert to seconds
  // measuredDelay = std::min(measuredDelay, 0.0001); // Clamp at 0
  double measuredDelay = m_rtt.GetSeconds();
  // std::cout << "Measured delay: " << measuredDelay << std::endl;
  double targetDelay = 0.0;
  ComputeTargetDelay(tcb, targetDelay);

  double cwnd = (double) tcb->GetCwndInSegments();
  m_canDecrease = CanDecrease();

  if (measuredDelay < targetDelay)
    {
      // std::cout << "Increased Measured delay: " << measuredDelay << ", target delay: " << targetDelay << " Max Delay: " << tcb->m_maxDelay / 1e9  << std::endl;

      // Additive Increase
      if (cwnd >= 1.0)
        {
          cwnd += (m_ai / cwnd) * segmentsAcked;
        }
      else
        {
          cwnd += (m_ai * segmentsAcked);
        }

    }
  else
    {
        // std::cout << "Decreased Measured delay: " << measuredDelay << ", target delay: " << targetDelay << " Max Delay: " << tcb->m_maxDelay / 1e9  << std::endl;

      // Multiplicative Decrease (only if can_decrease)
      if (m_canDecrease)
        {

          double factor = (1.0 - 0.4 * (measuredDelay - targetDelay) / measuredDelay);
          double lowerBound = (1.0 - m_maxMdf);
          factor = std::max(factor, lowerBound);
          cwnd = cwnd * factor;

          // Perform MD once per RTT
          m_tLastDecrease = Simulator::Now();
        }
    }

  // After computing new cwnd:
  double time = ns3::Simulator::Now().GetSeconds();

  // Log to the corresponding file based on m_flowIndex.
  // Ensure g_cwndFiles is accessible here. If not, you might need a global map or a static pointer.
  if (!g_cwndFiles.is_open())
  {
    std::ostringstream fname;
    fname << "./scratch/cwnd-flow" << m_flowIndex << ".dat";
    g_cwndFiles.open(fname.str().c_str(), std::ios::out);
  }
  if (g_cwndFiles.is_open())
  {
    g_cwndFiles << time << " " << cwnd << " " << measuredDelay << " " << targetDelay << std::endl;
  }

  // std::cout << "Cwnd before AI: " << tcb->m_cWnd << " ";
  // std::cout << "cwnd: " << cwnd << " segmentSize: " << tcb->m_segmentSize << " ";
  uint32_t newCwnd = (uint32_t)(cwnd * (double)tcb->m_segmentSize);
  // std::cout << "Cwnd after AIMD: " << newCwnd << std::endl;

  tcb->m_cWnd = newCwnd;

  tcb->m_maxDelay = 0.0;
}

void
TcpSwift::IncreaseWindow (Ptr<TcpSocketState> tcb, uint32_t segmentsAcked)
{
  SwiftUpdateCwndOnAck(tcb, segmentsAcked);
}

uint32_t
TcpSwift::GetSsThresh (Ptr<const TcpSocketState> tcb, uint32_t bytesInFlight)
{
  // On a congestion event, just halve as a fallback
  return std::max<uint32_t> (2 * tcb->m_segmentSize, tcb->m_cWnd / 2);
}

void
TcpSwift::CwndEvent (Ptr<TcpSocketState> tcb, const TcpSocketState::TcpCAEvent_t event)
{
  // OnRetransmitTimeout (RTO) and possibly Fast Retransmit:
  // According to snippet:
  // On Retransmit Timeout:
  //   retransmit_cnt += 1
  //   if retransmit_cnt >= RETX_RESET_THRESHOLD:
  //       cwnd = min_cwnd
  //     else if can_decrease:
  //       cwnd = (1 - max_mdf)*cwnd

  // On fast recovery:
  //   retransmit_cnt = 0
  //   if can_decrease:
  //     cwnd = (1 - max_mdf)*cwnd

  m_canDecrease = CanDecrease();

  if (event == TcpSocketState::CA_EVENT_LOSS)
    {
      std::cout << "Loss event detected" << std::endl;
      // This typically corresponds to a retransmission timeout or a fast loss detection
      m_retransmitCount += 1;

      if (m_retransmitCount >= m_retxResetThreshold)
        {
          tcb->m_cWnd = MinCwnd(tcb);
        }
      else
        {
          if (m_canDecrease)
            {
              double cwnd = (double) tcb->GetCwndInSegments();
              cwnd *= (1.0 - m_maxMdf);
              tcb->m_cWnd = std::max<uint32_t>(1, (uint32_t) cwnd) * tcb->m_segmentSize;

              m_tLastDecrease = Simulator::Now();
            }
        }

      double time = ns3::Simulator::Now().GetSeconds();
      double cwnd = (double) tcb->GetCwndInSegments();

      double measuredDelay = m_rtt.GetSeconds();
      // std::cout << "Measured delay: " << measuredDelay << std::endl;
      double targetDelay = 0.0;
      ComputeTargetDelay(tcb, targetDelay);

      // Log to the corresponding file based on m_flowIndex.
      // Ensure g_cwndFiles is accessible here. If not, you might need a global map or a static pointer.
      if (!g_cwndFiles.is_open())
      {
        std::ostringstream fname;
        fname << "./scratch/cwnd-flow" << m_flowIndex << ".dat";
        g_cwndFiles.open(fname.str().c_str(), std::ios::out);
      }

      if (g_cwndFiles.is_open())
      {
        g_cwndFiles << time << " " << cwnd << " " << measuredDelay << " " << targetDelay << std::endl;
      }
    }
  // If ECN or other events are defined, handle them similarly.
}

void
TcpSwift::CongestionStateSet (Ptr<TcpSocketState> tcb, const TcpSocketState::TcpCongState_t newState)
{
  // Fast Recovery:
  // When state changes to CA_RECOVERY from CA_OPEN or similar (indicating fast retransmit):
  //   retransmit_cnt =0
  //   if can_decrease:
  //     cwnd=(1 - max_mdf)*cwnd

  if (newState == TcpSocketState::CA_OPEN)
    {
      m_retransmitCount = 0;
    }
  else if (newState == TcpSocketState::CA_RECOVERY)
    {
      m_retransmitCount = 0;
      m_canDecrease = CanDecrease();
      if (m_canDecrease)
        {
          double cwnd = (double) tcb->GetCwndInSegments();
          cwnd *= (1.0 - m_maxMdf);
          tcb->m_cWnd = std::max<uint32_t>(1, (uint32_t) cwnd) * tcb->m_segmentSize;

          m_tLastDecrease = Simulator::Now();

          // After computing new cwnd:
          double time = ns3::Simulator::Now().GetSeconds();
          double measuredDelay = m_rtt.GetSeconds();
          // std::cout << "Measured delay: " << measuredDelay << std::endl;
          double targetDelay = 0.0;
          ComputeTargetDelay(tcb, targetDelay);

          // Log to the corresponding file based on m_flowIndex.
          // Ensure g_cwndFiles is accessible here. If not, you might need a global map or a static pointer.
          if (!g_cwndFiles.is_open())
          {
            std::ostringstream fname;
            fname << "./scratch/cwnd-flow" << m_flowIndex << ".dat";
            g_cwndFiles.open(fname.str().c_str(), std::ios::out);
          }
          if (g_cwndFiles.is_open())
          {
            g_cwndFiles << time << " " << cwnd << " " << measuredDelay << " " << targetDelay << std::endl;
          }
        }
    }
    
}

} // namespace ns3
