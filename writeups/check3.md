Checkpoint 3 Writeup
====================
# 回顾
Checkpoint 0 实现了流控制字节流的抽象 (ByteStream). 
Checkpoint 1 and 2 实现了 将不可靠数据报中携带的段转换为传入字节流的工具：Reassembler 和 TCPReceiver.

Checkpoint 3 将实现连接的另一端：TCPSender. 它将出站的字节流转为段，这些段将成为不可靠数据报的有效载荷。

# 准备开始

## Checkpoint 2 测试
```git
cmake -S . -B build
cd build/
cmake ..
make check2
```

## Checkpoint 3 获取

```git
git fetch --all
git merge origin/check3-startercode
cmake -S . -B build
cmake --build build
```

# Checkpoint 3: The TCP Sender
TCP协议 通过不可靠的数据报可靠地传输一对流量控制的字节流。两方均参与 TCP 连接，并且互为对等方。每个对等方同时充当发送者和接收者。

本周将实现 TCP 的发送方。它负责从字节流（由某些发送方应用程序创建写入）中读取数据，并将该流转化为一系列 TCP 段。
在远程端，TCP 接收方将这些段（到达的分段——可能并非全部到达）转换回原始字节流，并将确认号(ackno) 和 窗口大小(window_size) 发送回发送方。

TCP 发送方的目标如下：

1. 跟踪接收方的窗口（接受传入的 TCPReceiverMessages 的确认号(acknos) 和 窗口大小(window sizes)。

2. 通过从 ByteStream 读取数据并尽可能地填满窗口，创建新的 TCP 段（如果需要，包括 SYN 和 FIN 标志）并发送它们。发送方应连续发送段，直到窗口满或者 ByteStream 没有更多数据的需要传送。

3. 跟踪那些已发送但尚未被接收者确认的段——这些段也成为 “未完成段”（outstanding segments）

4. 如果自发送后经过了足够的时间未收到确认号，则重新发送未完成的段。

## TCP 发送方如何知道数据段是否丢失
TCPSender 将发送一堆 TCPSenderMessage. 每个都将包含一个来自 ByteStream 的子串，用序列号进行索引以指示其在流中的位置。并在流的开头设置 SYN 标志和结尾设置 FIN 标志。

除了发送这些段之外，TCPSender 还必须跟踪其未完成的段，直到它们占用的序列号被完全确认为止。
TCPSender 会定期地调用 TCPSender 的 tick 方法，以标识时间。
TCPSender 负责查看其未完成的 TCPSenderMessage 集合，并确定最旧的发送段是否已在未确认的情况下等待太长时间（即，它的所有序列号都没被确认）。
如果是，则需要重传。

以下是等待太长时间的规则：
(These are based on a simplified version of the “real” rules for TCP: RFC 6298, recommendations 5.1 through 5.6.)

1. 每隔几毫秒，TCPSender 的 tick 方法被调用，并带有一个参数。该参数告诉它自上次调用该方法以来已经过去了多少毫秒。使用它来维护 TCPSender 活动的
总毫秒数。不要调用其他操作系统的时钟函数。

2. 当 TCPSender 被构造时，它会被给定一个参数，告诉它重传超时（retransmission timeout, RTO）的初始值。RTO 是重新发送未完成的 TCP 段之前等待的毫秒数。RTO 的值会随着时间的推移而变化，但是 “初始值”保持不变。起始代码将 RTO 的 “初始值” 保存在名为 "_initial_retransmission_timeout" 的成员变量中。

3. 你将实现重传定时器 timer，在特定时间启动的警报，一旦超过重传超时时间（RTO），警报就会响起（或“过期”）。通过调用 tick 来实现

4. 每次发送包含数据的片段（在序列空间中为非零长度）时（无论是第一次还是重新发送），如果计时器没有在运行，则启动计时器，使其在 RTO 毫秒后过期（对于当前RTO值）。所谓“过期”，意味着时间将在将来的某个毫秒数内耗尽。

5. 当所有未完成的数据都被确认后，停止重传定时器。

6. 如果调用了tick并且重传定时器已过期：
    (a) 如果TCP接收方尚未完全确认的片段中最早（最低序列号）的片段需要重新发送。您需要在某个内部数据结构中存储未完成的片段，以便能够执行此操作。
    (b) 如果窗口大小 window size 不为零：
        i. 跟踪连续重传的次数，并递增该次数，因为您刚刚进行了重传。您的 TCP 连接将使用此信息来决定连接是否无望（连续重传次数过多）并需要中止。
        ii. 将 RTO 的值加倍。这被称为“指数退避”—— 它可以减缓在不稳定网络上的重传，以避免进一步阻塞。
    (c) 重置重传计时器，并启动它，使其在RTO毫秒后过期（考虑到您可能刚刚将RTO的值加倍！）。

7. 当接收方 向 发送方发出 ackno 以确认新数据的成功接收（该 ackno 反映了比任何先前 ackno 都大的绝对序列号）时：
    (a) 将RTO设置回其“初始值”。
    (b) 如果发送方有任何未完成的数据，重新启动重传计时器，使其在RTO毫秒后过期（对于当前的RTO值）。
    (c) 将“连续重传次数”的计数重置为零。


## 2.2 实现 TCP sender
TCP 发送方的基本思想：给定一个要传出的字节流，将其分割为段，将它们发送到接收方。如果它们没有很快得到确认，则继续重发。

以下是 TCPSender 提供的具体接口。它需要处理 5 个重要事件:
1. void push( Reader& outbound stream );
TCPSender被要求从传出的字节流中填充窗口：它从流中读取并生成尽可能多的 TCPSenderMessages，只要有新的字节需要读取并且窗口中有可用空间。

您需要确保发送的每个 TCPSenderMessage 完全适应接收方的窗口。将每个单独的消息尽可能大，但不超过 TCPConfig::MAX PAYLOAD SIZE（1452字节）的值。

您可以使用 TCPSenderMessage::sequence_length() 方法来计算一个片段占用的总序列号数量。请记住，SYN 和 FIN 标志也各自占用一个序列号，这意味着它们在窗口中占用空间。

如果窗口大小为零，我应该怎么办？
如果接收方宣告窗口大小为零，push 方法应该假装窗口大小为 1。发送方可能会发送一个被接收方拒绝（并且未被确认）的单个字节，但这也可能促使接收方发送一个新的确认段，其中它揭示了窗口中有更多空间。如果没有这个处理，发送方将永远不会知道它可以开始重新发送。
这是你的实现在窗口大小为零的特殊情况下应该具备的唯一特殊行为。TCPSender 实际上不应该记住一个虚假的窗口大小为 1。这个特殊情况只存在于 push 方法中。此外，请注意，即使窗口大小为 1，窗口仍然可能是满的。

2. std::optional<TCPSenderMessage> maybe_send();
如果 TCPSender 想要的话，这是实际发送 TCPSenderMessage 的机会
注1：当发送包含数据的段时，保留其副本，以备可能的重新传输。TCPSenderMessage 的有效载荷被存储为只读字符串的引用（一个 Buffer 对象），因此并不会复制一份数据，而只是复制一个引用，不会浪费空间。
注2：重传时，不需要合并序号相连的不同段，一个一个发即可。

3. void receive( const TCPReceiverMessage& msg ); 
从接收方收到一条消息，得到左边缘(=ackno) 和 右边缘 (ackno + window_size)。TCPSender 应该查看其未完成的段集合，并删除任何现在已完全确认的段，（ackno 大于 段中所有序列号）
注1：在接收方通知 TCPSender 之前，应该假设接收器的窗口大小为 1.
注2：确认号在某一段的中间时，视为未完成。只有当确认号越过整个未完成段时，才被视为已完成并从集合中删除其副本。

4. void tick( const size t ms since last tick );
自上次调用此方法以来已经过了一定的毫秒数。发送器可能需要重新发送未完成的段。

5. void send_empty_message();
TCPSender 应该生成并发送一个长度为零的消息，并正确设置序列号。如果对等方希望发送一个 TCPReceiverMessage（例如，因为它需要确认 对等的发送方的某些内容），并且需要生成一个与之相配的 TCPSenderMessage，那么这将非常有用。
注意：像这样不占用序列号的段不需要被跟踪为“未完成”，并且永远不会被重新传输。