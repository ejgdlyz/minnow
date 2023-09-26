#include "reassembler.hh"
#include <algorithm>
#include <iostream>

using namespace std;

Reassembler::Reassembler() 
  : first_unassembled_index_(0), buffer_(), buffer_size_(0), has_last_(false)
{}

// 将 data 转换为可靠的数据流，并写入 output
void Reassembler::insert( uint64_t first_index, string data, bool is_last_substring, Writer& output )
{
  // Your code here.
  if (data.empty())
  {
    if (is_last_substring)
    {
      output.close();
    }
    return;
  }

  if (output.available_capacity() == 0){
    return;
  }
  
  // first_unassembled_index_ = output.bytes_pushed();
  auto const end_index = first_index + data.size();
  auto const first_unacceptable_index = first_unassembled_index_ + output.available_capacity();

  // data is not in [first_unassembled_index_, first_unacceptable_index)
  if (end_index <= first_unassembled_index_ || first_index >= first_unacceptable_index){
    return;
  }

  // data 与 之前的数据 部分重复， 裁剪 data
  if (end_index > first_unacceptable_index)
  {
    data = data.substr(0, first_unacceptable_index - first_index);
    is_last_substring = false;  // 裁剪后，不是最后子串
  }

  // 缓存先到的字符
  if (first_index > first_unassembled_index_)
  {
    insert_into_buffer(first_index, std::move(data), is_last_substring);
    return;
  }

  // data 前缀有重复
  if (first_index < first_unassembled_index_)
  {
    data = data.substr(first_unassembled_index_ - first_index);
  }

  // 有序 first_index = first_unassembled_index_
  first_unassembled_index_ += data.size();

  output.push(std::move(data));

  if (is_last_substring)
  {
    output.close();
  }

  if (!buffer_.empty() && buffer_.begin()->first <= first_unassembled_index_)
  {
    pop_from_buffer(output);
  }
}

// 待处理的字节数
uint64_t Reassembler::bytes_pending() const
{
  return buffer_size_;
}

void Reassembler::insert_into_buffer( const uint64_t first_index, std::string&& data, const bool is_last_substring )
{
  auto begin_index = first_index;
  const auto end_index = first_index + data.size();
  
  //buffer_ 有序双向链表
  for (auto it = buffer_.begin(); it != buffer_.end() && begin_index < end_index;)
  {
    if (it->first <= begin_index)
    {
      begin_index = max(begin_index, it->first + it->second.size());
      ++it;
      continue;
    }

    // 找到插入位置，作为独立节点，在此节点前插入
    if (begin_index == first_index && end_index <= it->first)
    {
      buffer_size_ += data.size();
      buffer_.emplace(it, first_index, std::move(data));
      return;
    }

    // 与之间的数据有重叠, 合并数据
    const auto right_index = min(it->first, end_index);
    const auto len = right_index - begin_index;
    buffer_.emplace(it, begin_index, data.substr(begin_index - first_index, len));
    buffer_size_ += len;
    begin_index = right_index;
    
  }

  // 还有数据未缓存
  if (begin_index < end_index)
  {
    buffer_size_ += end_index - begin_index;
    buffer_.emplace_back(begin_index, data.substr(begin_index - first_index));
  }

  if (is_last_substring)
  {
    has_last_ = true;
  }
}

// 缓冲区数据 移入 字节流
void Reassembler::pop_from_buffer( Writer& output )
{
  for ( auto it = buffer_.begin(); it != buffer_.end(); ) 
  {
    if ( it->first > first_unassembled_index_ ) 
    {
      break;
    }

    // it->first <= first_unassembled_index_
    const auto end = it->first + it->second.size();
    if ( end <= first_unassembled_index_ ) 
    {
      buffer_size_ -= it->second.size();
    } 
    else 
    {
      auto data = std::move( it->second );
      buffer_size_ -= data.size();
      if ( it->first < first_unassembled_index_ ) 
      {
        data = data.substr( first_unassembled_index_ - it->first );
      }

      first_unassembled_index_ += data.size();
      output.push( std::move( data ) );
    }
    it = buffer_.erase( it );
  }

  if ( buffer_.empty() && has_last_ )
  {
    output.close();
  }
}