Checkpoint 1 Writeup
====================

# 回顾
Check 0 实现了 ByteStream 这是一个内存中的字节流 "管道"。本实验将实现 TCP 接收器 Reassembler。 该模块接收数据，将其转换为可靠的字节流并
写入 ByteStream，以供应用程序读取使用。

# 按序拼装字串
TCP 发送者将要发送的字节流分成小段（每段不超过 1460 字节）以装入一个数据报中 (datagram)。但是网络可能重排这些数据报，或者丢弃他们，或者重复发送。
接收方必须将这些段重新组装为他们开始的连续字节流。

在本实验中，将编写负责此重组的数据结构：重组器。它将接收由字节字符串以及该字符串的第一个字节在较大流中的索引（代表它在整个流中的位置）。
流的每个字节都有自己唯一的索引，从零开始向上计数。
    子字符串可能以任何顺序到达，因此，必须 缓存 接收到的子字符串，直到他们之前的索引都已写入。
    下一个需要重新组装的子串的索引，一旦接收到该子串，立刻写入 ByteStream。
    提前到达的子串，当容量足够时就缓存，等前面的子串到达在写入。容量不够，直接丢弃。

##  Reassembler 应该保存什么？
insert 方法通知 Reassembler 有关 ByteStream 的新片段以及它在整个流中的位置。
Reassembler 必须处理三类内容：
    字节，流中的下一字节。一旦知道他们，Reasembler 就应该将他们压入 Writer。
    提前到达的字节。如果流的容量可用，这些需要存储在存储器中，以等待先前的字节到达。
    超出流容量的字节，应该被丢弃。
以上的操作的目的是限制 Reassembler 和 ByteStream 的内存容量。


My name: [your name here]

My SUNet ID: [your sunetid here]

I collaborated with: [list sunetids here]

I would like to thank/reward these classmates for their help: [list sunetids here]

This lab took me about [n] hours to do. I [did/did not] attend the lab session.

Program Structure and Design of the Reassembler:
[]

Implementation Challenges:
[]

Remaining Bugs:
[]

- Optional: I had unexpected difficulty with: [describe]

- Optional: I think you could make this lab better by: [describe]

- Optional: I was surprised by: [describe]

- Optional: I'm not sure about: [describe]
