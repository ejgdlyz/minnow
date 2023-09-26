#include "tcp_receiver.hh"
#include <iostream>
#include <chrono>

using namespace std;

// 接收 TCPSenderMessages 消息，将其有效载荷插入到 Reassembler 中正确的流索引处 
void TCPReceiver::receive( TCPSenderMessage message, Reassembler& reassembler, Writer& inbound_stream )
{
  // Your code here.

  if (!isn_.has_value())
  {
    if (!message.SYN)
    {
      return;
    }
    isn_ = message.seqno;
  }
  
  /*
  为了得到 first_index (stream index), 需要
    1. convert message.seqno to absolute seqno
    2. convert absolute seqno to stream index
  */

  // 1. convert message.seqno to absolute seqno
  auto const checkpoint = inbound_stream.bytes_pushed() + 1;  // stream index -> absolute seqno
  auto const abs_seqno = message.seqno.unwrap(isn_.value(), checkpoint);

  // 2. convert absolute seqno to stream index
  auto const first_index = message.SYN ? 0 : abs_seqno - 1;
  reassembler.insert(first_index,
        message.payload.release(), message.FIN, inbound_stream);
  
  (void)message;
  (void)reassembler;
  (void)inbound_stream;
}

TCPReceiverMessage TCPReceiver::send( const Writer& inbound_stream ) const
{
  // Your code here.
  TCPReceiverMessage tcpRecvMsg;
  auto const win_sz = inbound_stream.available_capacity();
  tcpRecvMsg.window_size = win_sz > UINT16_MAX ? UINT16_MAX : win_sz;

  if (isn_.has_value())
  {
    // convert from stream index to abs seqno
    // + 1 for SYN, + inbound_stream.is_closed() for FIN
    uint64_t const abs_seqno = inbound_stream.bytes_pushed() + 1 + inbound_stream.is_closed();
    tcpRecvMsg.ackno = Wrap32::wrap(abs_seqno, isn_.value());
  }
  
  (void)inbound_stream;
  return tcpRecvMsg;
}
