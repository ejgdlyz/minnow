#include "network_interface.hh"

#include "arp_message.hh"
#include "ethernet_frame.hh"

using namespace std;

// ethernet_address: Ethernet (what ARP calls "hardware") address of the interface
// ip_address: IP (what ARP calls "protocol") address of the interface
NetworkInterface::NetworkInterface( const EthernetAddress& ethernet_address, const Address& ip_address )
  : ethernet_address_( ethernet_address ), ip_address_( ip_address )
{
  cerr << "DEBUG: Network interface has Ethernet address " << to_string( ethernet_address_ ) << " and IP address "
       << ip_address.ip() << "\n";
}

// dgram: the IPv4 datagram to be sent
// next_hop: the IP address of the interface to send it to (typically a router or default gateway, but
// may also be another host if directly connected to the same network as the destination)

// Note: the Address type can be converted to a uint32_t (raw 32-bit IP address) by using the
// Address::ipv4_numeric() method.
void NetworkInterface::send_datagram( const InternetDatagram& dgram, const Address& next_hop )
{
  auto const& target_ip = next_hop.ipv4_numeric();
  if(ip2ether_.contains(target_ip))
  {
    EthernetFrame frame {
      {ip2ether_[target_ip].first, ethernet_address_, EthernetHeader::TYPE_IPv4},  // the header of the ip datagram  
      serialize(dgram)  // payload , 存放序列化 dgram
      };
      out_frames_.push(std::move(frame));
  }
  else
  {
    if (!arp_timer_.contains(target_ip))
    {
      // 生成 ARP 请求
      ARPMessage arpRequestMsg;
      arpRequestMsg.opcode = ARPMessage::OPCODE_REQUEST;
      arpRequestMsg.sender_ethernet_address = ethernet_address_;
      arpRequestMsg.sender_ip_address = ip_address_.ipv4_numeric();
      arpRequestMsg.target_ip_address = next_hop.ipv4_numeric();

      // 将 ARP 请求包装为以太网帧
      EthernetFrame frame {
        {ETHERNET_BROADCAST, ethernet_address_, EthernetHeader::TYPE_ARP},  // the header of the arp message
        serialize(arpRequestMsg)
      };

      out_frames_.push(std::move(frame));  // 加入待发送队列

      arp_timer_.emplace(next_hop.ipv4_numeric(), 0);  // arp 发送计时

      waited_dgrams_[target_ip].push_back(dgram);  // 保存排队的 ip 数据报和它对应的下一跳 ip
    }
    else
    {
      waited_dgrams_[target_ip].push_back(dgram);
    }
  }
}

// frame: the incoming Ethernet frame
optional<InternetDatagram> NetworkInterface::recv_frame( const EthernetFrame& frame )
{
  if (frame.header.dst != ethernet_address_ && frame.header.dst != ETHERNET_BROADCAST)
  {
    return {};
  }

  if (frame.header.type == EthernetHeader::TYPE_IPv4)  // 收到 ip 数据报
  {
    // 解析 ip 数据并返回
    InternetDatagram dgram;
    if (parse(dgram, frame.payload))
    {
      return dgram;
    }
  }
  else if(frame.header.type == EthernetHeader::TYPE_ARP)  // 收到 arp 数据报
  {
    // 解析 ARP 消息：收到请求消息 或者 收到回复消息
    ARPMessage arpMsg;
    if (parse(arpMsg, frame.payload))
    {
      // 建立映射 即 保存 发送方的 ip 地址和它对应的 以太网地址
      ip2ether_.insert({arpMsg.sender_ip_address, {arpMsg.sender_ethernet_address, 0}});

      if (arpMsg.opcode == ARPMessage::OPCODE_REQUEST)  // 收到 apr 请求
      {
        if (arpMsg.target_ip_address == ip_address_.ipv4_numeric())  // 请求的就是本机以太网地址
        {
          // 生成 arp 应答
          ARPMessage replyMsg;
          replyMsg.opcode = ARPMessage::OPCODE_REPLY;
          replyMsg.sender_ethernet_address = ethernet_address_;
          replyMsg.sender_ip_address = ip_address_.ipv4_numeric();
          replyMsg.target_ethernet_address = arpMsg.sender_ethernet_address;
          replyMsg.target_ip_address = arpMsg.sender_ip_address;

          // arp 应答消息包装为 以太网帧
          EthernetFrame replyFrame {
            {arpMsg.sender_ethernet_address, ethernet_address_, EthernetHeader::TYPE_ARP},
            serialize(replyMsg)
          };

          out_frames_.push(replyFrame);
        }
        
      }
      else if (arpMsg.opcode == ARPMessage::OPCODE_REPLY)  // 收到的是 ARP 回复
      {
        ip2ether_.insert({arpMsg.sender_ip_address, {arpMsg.sender_ethernet_address, 0}});
        auto const& dgrams = waited_dgrams_[arpMsg.sender_ip_address];
        // 收到应答后，即得到了 ip 地址对应的 以太网地址，将排队的 ip 数据发出
        for (auto const& dgram : dgrams)
        {
          send_datagram(dgram, Address::from_ipv4_numeric(arpMsg.sender_ip_address));
        }

        waited_dgrams_.erase(arpMsg.sender_ip_address);  // 发出后，删除对应的数据报
      }
    }
    else
    {
      return {};
    }
    
  }
  else
  {
    return {};
  }

  return {};
}

// ms_since_last_tick: the number of milliseconds since the last call to this method
void NetworkInterface::tick( const size_t ms_since_last_tick )
{
  // 遍历映射表，查看哪些超时
  static constexpr size_t IP_MAP_TTL = 30000;  // 30s 
  static constexpr size_t ARP_REQUEST_TTL = 5000;  // 5s

  for (auto it = ip2ether_.begin(); it != ip2ether_.end();)
  {
    it->second.second += ms_since_last_tick;
    if (it->second.second >= IP_MAP_TTL)
    {
      it = ip2ether_.erase(it);
    }
    else
    {
      ++it;
    }
  }

  // 遍历 arp 请求表，查看哪些超时
  for (auto it = arp_timer_.begin(); it != arp_timer_.end();)
  {
    it->second += ms_since_last_tick;
    if (it->second >= ARP_REQUEST_TTL)
    {
      it = arp_timer_.erase(it);
    }
    else
    {
      ++it;
    }
  }
}

// 返回 待发送队列第一个以太网帧即可
optional<EthernetFrame> NetworkInterface::maybe_send()
{
  if (out_frames_.empty())
    return {};
  
  auto frame = out_frames_.front();
  out_frames_.pop();
  return frame;
}
