#include "tcp_sender.hh"
#include "tcp_config.hh"

#include <random>

using namespace std;

/* TCPSender constructor (uses a random ISN if none given) */
TCPSender::TCPSender( uint64_t initial_RTO_ms, optional<Wrap32> fixed_isn )
  : isn_( fixed_isn.value_or( Wrap32 { random_device()() } ) ), initial_RTO_ms_( initial_RTO_ms )
{}

uint64_t TCPSender::sequence_numbers_in_flight() const
{
  // Your code here.
  return outstanding_cnt_;
}

uint64_t TCPSender::consecutive_retransmissions() const
{
  // Your code here.
  return retransmit_cnt_;
}

// 从 queued_segments_ 取出段并发送
optional<TCPSenderMessage> TCPSender::maybe_send()
{
  // Your code here.
  if (queued_segments_.empty())
  {
    return {};
  }

  if (!timer_.is_running())  // 启动定时器
  {
    timer_.start();
  }

  auto msg = queued_segments_.front();
  queued_segments_.pop();
  return msg;
}

// push 从流中提取数据并生成一个个段，直到 outstanding 超出容量或流结束
void TCPSender::push( Reader& outbound_stream )
{
  // Your code here.
  size_t curr_window_size = window_size_ != 0 ? window_size_ : 1;  // 接收方目前的窗口大小
  while(outstanding_cnt_ < curr_window_size)
  {
    TCPSenderMessage msg;  // 生成消息

    if (!syn_)  // syn 标记
    {
      syn_ = msg.SYN = true;  // 新段 开始
      outstanding_cnt_ += 1;
    }

    msg.seqno = Wrap32::wrap(next_seqno_, isn_);  // absolute seqno -> seqno

    auto const payload_size = min(TCPConfig::MAX_PAYLOAD_SIZE, curr_window_size - outstanding_cnt_);
    read(outbound_stream, payload_size, msg.payload);  // byte stream -> string, namely msg.payload， 
    outstanding_cnt_ += msg.payload.size();

    if (!fin_ && outbound_stream.is_finished() && outstanding_cnt_ < curr_window_size)
    {
      fin_ = msg.FIN = true;  // 新段 结束
      outstanding_cnt_ += 1;
    }

    if (msg.sequence_length() == 0)
    {
      break;
    }

    queued_segments_.push(msg);  // 将准备好的 msg 段放入 queued_segments_ 准备发送
    next_seqno_ += msg.sequence_length();  
    outstanding_segments_.push(msg);  //   记录已发送但未完成段的副本

    if (msg.FIN || outbound_stream.bytes_buffered() == 0)
      break;

  }
}

// 向接收者发送一个空消息 以 通知其 现在发送端的 seqno 是什么
TCPSenderMessage TCPSender::send_empty_message() const  // 发送一个长度为零的消息，并正确设置序列号。
{
  // Your code here.
  auto seqno = Wrap32::wrap(next_seqno_, isn_);
  return TCPSenderMessage{seqno, false, {}, false};
}

// 设置窗口大小以控制 push 从流中取数据的长度 即 流量控制
// 根据 ackno 删除 已发送未完成的队列中保存的副本
// 删除后，重置定时器 
void TCPSender::receive( const TCPReceiverMessage& msg )
{
  // Your code here.
  window_size_ = msg.window_size;
  if (msg.ackno.has_value())
  {
    auto ackno = msg.ackno.value().unwrap(isn_, next_seqno_);  // seqno -> absolute seqno
    if (ackno > next_seqno_)  // 接收方发送的 ackno 不能超过已经发出去的最新的 ackno
    {
      return;
    }
    acked_seqno_ = ackno;

    while (!outstanding_segments_.empty())
    {
      auto& front_msg = outstanding_segments_.front();

      // 确保 outstanding_segments_ 中的段的 右边界 <= 收到的确认号
      if (front_msg.seqno.unwrap(isn_, next_seqno_) + front_msg.sequence_length() <= acked_seqno_)
      {
        outstanding_cnt_ -= front_msg.sequence_length();
        outstanding_segments_.pop();
        timer_.reset_RTO();  // 删除后，定时器重置 继续对剩余的段进行计时

        if (!outstanding_segments_.empty())
        {
          timer_.start();  // 仍有段未确认，继续计时
        }

        retransmit_cnt_ = 0;
      }
      else  // 超出右边界
      {
        break;
      }
    }
  }

  if (outstanding_segments_.empty())  // 所有段都确认，关闭定时器
  {
    timer_.stop();
  }

  (void)msg;
}

// tick 模拟时间流逝，调用 tick 时间才变化。
void TCPSender::tick( const size_t ms_since_last_tick )
{
  // Your code here.
  timer_.tick(ms_since_last_tick);
  if (timer_.is_expired())
  {
    queued_segments_.push(outstanding_segments_.front());  // 将未完成的段重新放入 queued_segments_ 以进行重传

    if (window_size_ != 0)
    {
      ++retransmit_cnt_;   // 指数退避
      timer_.double_RTO();
    }

    timer_.start();  // 开始定时器
  }
  (void)ms_since_last_tick;
}
