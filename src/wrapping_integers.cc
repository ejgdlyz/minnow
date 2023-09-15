#include "wrapping_integers.hh"

using namespace std;

// absolute seqno -> seqno
Wrap32 Wrap32::wrap( uint64_t n, Wrap32 zero_point )
{
  // Your code here.
  // 这里取模与截断等效，因为 模为进制数的倍数， 截断 n 的前 32 位 等价于 % 2^32 
  (void)n;
  (void)zero_point;
  return Wrap32 {zero_point + static_cast<uint32_t>(n)};
}

// seqno -> absolute seqno
uint64_t Wrap32::unwrap( Wrap32 zero_point, uint64_t checkpoint ) const
{
  // Your code here.
  //先得到 mod 后的 absolute_seqno
  uint64_t absolute_seqno = static_cast<uint64_t> (this->raw_value_ - zero_point.raw_value_);
  
  // 然后将 absolute_seqno 还原为 mod 前，并且离 checkpoint 最近
   
  // 找出 checkpoint 前后两个可还原的 absolute_seqno, 取最近即可
  uint64_t mod_cnt = checkpoint >> 32;  // mod 的次数
  uint64_t remainder = checkpoint << 32 >> 32;  // 获得余数

  // 离 checkpoint 最近的边界的 mod 次数， mod_cnt 是左边界
  uint64_t bound;
  if (remainder < 1UL << 31)  // remainder 属于 [0, 2^32 - 1]
  {
    bound = mod_cnt;
  }
  else
  {
    bound = mod_cnt + 1;
  }
  // 以该边界的左右边界作为 基础，还原出 2 个 mod 之前的 absolute_seqno
  uint64_t absolute_seqno_left = absolute_seqno + ((bound == 0 ? 0: bound - 1) << 32);
  uint64_t absolute_seqno_right = absolute_seqno + (bound << 32);

  // 判断 checkpoint 离哪个 absolute_seqno 更近
  if (checkpoint < (absolute_seqno_left + absolute_seqno_right) >> 1)
  {
    absolute_seqno = absolute_seqno_left;
  }
  else
  {
    absolute_seqno = absolute_seqno_right;
  }
  (void)zero_point;
  (void)checkpoint;
  return {absolute_seqno};
}
