#include "reassembler.hh"
#include <algorithm>
#include <iostream>

using namespace std;

Reassembler::Reassembler() 
  : first_unassembled_index_(0), first_unacceptable_index_(0), reassemblerBuf_(), flagBuf_(), endIndex_(-1), init_flag_(true)
{}

// 将 data 转换为可靠的数据流，并写入 output
void Reassembler::insert( uint64_t first_index, string data, bool is_last_substring, Writer& output )
{
  // Your code here.
  // 流开始，按照 output 剩余容量初始化 reassemblerBuf_
  if (init_flag_)
  {
    reassemblerBuf_.resize(output.available_capacity(), 0);
    flagBuf_.resize(output.available_capacity(), 0);
    init_flag_ = false;
  }

  // 最后一个字符序列
  if (is_last_substring)
  {
    endIndex_ = first_index + data.size();
  }

  first_unassembled_index_ = output.bytes_pushed();  // 待排序的子串在 output 流中的开始
  first_unacceptable_index_ = first_unassembled_index_ + output.available_capacity();  // output 中的剩余空间

  uint64_t str_begin, str_end;

  if (!data.empty())
  {
    // 当前 data 与之前的接收的数据完全重复，则丢弃。 或 当前的子串第一个字节索引 超出缓冲容量 (不能缓存)，则丢弃
    if (first_index + data.size() < first_unassembled_index_ || first_index >= first_unacceptable_index_)  // 
    {
      data = "";  
    }
    else
    {
      // 标记 data 在 缓冲中的位置
      str_begin = first_index;
      str_end = str_begin + data.size() - 1;

      if (str_begin < first_unassembled_index_)  // 部分重复数据，裁剪掉重复的
      {
        str_begin = first_unassembled_index_;
      }

      if (str_end >= first_unacceptable_index_)  // 去掉尾部超出缓冲区容量的部分
      {
        str_end = first_unacceptable_index_ - 1;
      }

      // data 加入 reassemblerBuf_
      for(uint64_t i = str_begin; i <= str_end; ++i)
      {
        // 将 data 映射到 缓冲区，以保存先到的子串。这里的 data 有可能经过了裁剪，需要减去修改后的开始索引 first_index
        reassemblerBuf_[i - first_unassembled_index_] = data[i - first_index];
        flagBuf_[i - first_unassembled_index_] = true;  // 标记缓存的字符
      }
    }

  }

  string buf;
  // flagBuf_[0] = true，表示当前 reassemblerBuf_ 中的数据已经按序全部接收到
  while( flagBuf_.front())
  {
    buf += reassemblerBuf_.front();
    reassemblerBuf_.pop_front();
    flagBuf_.pop_front();

    // 补上容量
    reassemblerBuf_.push_back(0);
    flagBuf_.push_back(false);
  }

  output.push(buf);

  // 如果 push 进 ByteStream 中的字符数 = 最后一个字符索引，整个流输入完成
  if (output.bytes_pushed() == endIndex_)
  {
    output.close();
  }

  (void)first_index;
  (void)data;
  (void)is_last_substring;
  (void)output;
}

// 待处理的字节数
uint64_t Reassembler::bytes_pending() const
{
  // Your code here.
  return static_cast<uint64_t>(count(flagBuf_.begin(), flagBuf_.end(), true));
}
