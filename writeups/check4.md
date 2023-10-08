Checkpoint 4 Writeup
====================

# 回顾
本周的实验将深入了解网络堆栈，并实现一个网络接口：连接环游世界的 Internet 数据报和单跳传输的链路层以太网帧。
这个组件可以“位于”你之前实验中的 TCP/IP 实现之下，但它也将在不同的环境中使用：当你在第六个实验中构建一个路由器时，它将在网络接口之间路由数据报。
图1展示了网络接口在两种环境中的应用方式。
https://cs144.github.io/assignments/check4.pdf


在之前的实验中，你编写了一个能够与任何支持 TCP 的计算机交换 TCP段（TCP segments） 的TCP实现。
这些段实际上是如何传递给对方的TCP实现的呢？
正如我们所讨论的，有几种选择：
1. TCP-in-UDP-in-IP. TCP段 可以在用户数据报的有效载荷中传输。在正常的（用户空间）环境中工作时，这是最容易实现的方式：Linux提供了一个接口（“数据报套接字”，UDPSocket），允许应用程序仅提供用户数据报的有效载荷和目标地址，而内核负责构造UDP头部、IP头部和以太网头部，然后将数据包发送到适当的下一跳。内核确保每个套接字具有独占的本地和远程地址以及端口号的组合。由于是内核将这些写入 UDP 和 IP 头部的实体，它可以保证不同应用程序之间的隔离性。

2. TCP-in-IP. 在常见的用法中，TCP 段 几乎总是直接放置在 Internet 数据报中，IP 头部 和 TCP 头部之间没有 UDP 头部。这就是人们所说的 “TCP/IP”。
这种方式稍微复杂一些。Linux提供了一个称为TUN设备的接口，允许应用程序提供整个Internet数据报，而内核负责处理其余的部分（编写以太网头部，并通过物理以太网卡实际发送等）。但是现在应用程序必须自己构建完整的 IP头部，而不仅仅是有效载荷。

3. TCP-in-IP-in-Ethernet. 在上述方法中，我们仍然依赖于Linux内核的一部分网络堆栈。每当你的代码向TUN设备写入IP数据报时，Linux都必须构建一个适当的链路层（以太网）帧，其中IP数据报作为其有效载荷。这意味着Linux必须根据下一跳的IP地址找出其以太网目标地址。如果Linux还不知道这个映射关系，它会广播一个查询，询问：“谁拥有以下IP地址？你的以太网地址是什么？”，然后等待回应。

这些功能由网络接口执行：它将出站的IP数据报转换为链路层（例如以太网）帧，反之亦然。在真实的系统中，网络接口通常具有类似eth0、eth1、wlan0等的名称。
在本周的实验中，你将实现一个网络接口，并将其放置在TCP/IP堆栈的最底部。你的代码将生成原始的以太网帧，然后通过一个名为TAP设备的接口将其传递给Linux。TAP设备类似于TUN设备，但更低层，它交换的是原始的链路层帧而不是IP数据报。

大部分工作将是查找（和缓存）每个下一跳 IP 地址的以太网地址。用于此目的的协议称为地址解析协议 (Address Resolution Protocol, ARP)。

# 准备开始

## Checkpoint 3 测试
```git
cmake -S . -B build
cd build/
cmake ..
make check3
```

## Checkpoint 4 获取

```git
git fetch --all
git merge origin/check4-startercode
cmake -S . -B build
cmake --build build
```

# CheckPoint 4: 地址解析协议 ARP
在本实验中，你的主要任务是实现 NetworkInterface 的三个主要方法（位于 network_interface.cc 文件中），并维护从 IP 地址到以太网地址的映射关系。这个映射关系是一个缓存或者“软状态”：NetworkInterface 会保留它以提高效率，但如果需要从头开始重新启动，映射关系会自然地重新生成而不会造成问题。

1. ```void NetworkInterface::send_datagram(const InternetDatagram &dgram, const Address &next hop);```
当调用者（例如你的 TCPConnection 或路由器）希望将出站的Internet（IP）数据报发送到下一跳时，将调用此方法。你的网络接口的任务是将这个数据报转换成以太网帧，并最终发送出去。
   - 如果目标以太网地址已知，则立即发送。创建一个以太网帧，类型设置为 EthernetHeader::TYPE_IPv4。将序列化的数据报设置为有效载荷，并设置源地址和目标地址。
   - 如果目标以太网地址未知，则广播一个用于获取下一跳以太网地址的ARP请求，并将 IP 数据报排队，以便在收到 ARP 回复后发送。
   - 但是，我们不希望用 ARP 请求淹没网络。如果网络接口在过去的五秒内已经发送过关于相同IP地址的ARP请求，请不要发送第二个请求，只需等待第一个请求的回复。同样，将数据报排队，直到获取到目标以太网地址为止。

1. ```optional<InternetDatagram> NetworkInterface::recv_frame(const EthernetFrame &frame);```
当从网络接收到一个以太网帧时，将调用此方法。代码应该忽略任何不针对网络接口的帧（这意味着以太网目的地是广播地址或存储在 _ethernet_address 成员变量中的接口自己的以太网地址）。
   - 如果传入的帧是 IPv4，将有效载荷解析为 InternetDatagram，如果解析成功（即 parse() 方法返回 true），将生成的 InternetDatagram 返回给调用者。
   - 如果传入的帧是 ARP，将有效载荷解析为 ARPMessage，如果解析成功，记住发送方的 IP 地址和以太网地址之间的映射关系，保存 30 秒钟（无论是请求还是回复都要学习映射关系）。此外，如果是一个请求我们 IP 地址的 ARP 请求，发送一个适当的 ARP 回复。

1. std::optional<EthernetFrame> maybe_send(); 
如果需要的话，这是 NetworkInterface 实际发送以太网帧的机会。

1. void NetworkInterface::tick(const size_t ms_since_last_tick);
时间推移方法。随着时间的推移，IP到以太网的映射会逐渐过期。

# 测试
```git
cmake --build build --target check4
```
