#pragma once

#include "byte_stream.hh"
#include "tcp_receiver_message.hh"
#include "tcp_sender_message.hh"
#include "Timer.hh"

class TCPSender
{
  Wrap32 isn_;
  uint64_t initial_RTO_ms_;

  bool syn_ {false};
  bool fin_ {false};
  unsigned retransmit_cnt_ {0};  // 连续重传次数


  uint64_t acked_seqno_ {0};
  uint64_t next_seqno_ {0};
  uint64_t window_size_ {1};  // 窗口大小初始化为 1

  uint64_t outstanding_cnt_ {0};  // 已发送但未完成的字节数
  std::queue<TCPSenderMessage> outstanding_segments_ {};  // 已发送但未完成的段
  std::queue<TCPSenderMessage> queued_segments_ {};  // 记录已经准备好发送的段

  Timer timer_ {initial_RTO_ms_};

public:
  /* Construct TCP sender with given default Retransmission Timeout and possible ISN */
  TCPSender( uint64_t initial_RTO_ms, std::optional<Wrap32> fixed_isn );

  /* Push bytes from the outbound stream */
  void push( Reader& outbound_stream );

  /* Send a TCPSenderMessage if needed (or empty optional otherwise) */
  std::optional<TCPSenderMessage> maybe_send();

  /* Generate an empty TCPSenderMessage */
  TCPSenderMessage send_empty_message() const;

  /* Receive an act on a TCPReceiverMessage from the peer's receiver */
  void receive( const TCPReceiverMessage& msg );

  /* Time has passed by the given # of milliseconds since the last time the tick() method was called. */
  void tick( uint64_t ms_since_last_tick );

  /* Accessors for use in testing */
  uint64_t sequence_numbers_in_flight() const;  // How many sequence numbers are outstanding?
  uint64_t consecutive_retransmissions() const; // How many consecutive *re*transmissions have happened?
};
