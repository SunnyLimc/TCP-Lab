#include "wrapping_integers.hh"

// Dummy implementation of a 32-bit wrapping integer

// For Lab 2, please replace with a real implementation that passes the
// automated checks run by `make check_lab2`.

template <typename... Targs>
void DUMMY_CODE(Targs &&.../* unused */) {}

using namespace std;

WrappingInt32 wrap(uint64_t n, WrappingInt32 isn) {
    // 在 Absolute Seqno 中，isn 是已经被忽略的
    return WrappingInt32(((n & 0xFFFFFFFF) + isn.raw_value()));
}

//! 我把这个函数称之为对无符号整数和移位的最强理解
uint64_t unwrap(WrappingInt32 n, WrappingInt32 isn, uint64_t checkpoint) {
    uint32_t &&offset = n - wrap(checkpoint, isn);
    //! 为什么
    // 如果需要取最近的点，checkpoint必须转换为uint32的形式
    // 此时区分点为 n 与 checkpoint_32 的距离在前后哪部分 1/2 的 UINT32_MAX
    //! 如何做
    // 逻辑为 如果 offset 大于 1u >> 31，则 n 在 checkpoint 前面
    // 否则意味着 n 在 checkpoint 的后方
    // 注意是 31，(<1<0000000, 此时为7)，比中间值大1。
    //! copyright 某位大佬
    //! 特殊情况 - 判断回退
    // 如果 offset 比 checkpoint 目前的距离要大，则 checkpoint 无法回退
    // 可以将目前的无符号 offset 理解为负数，如果 offset + checkpoint < 1u^32
    // 则无法回退，只能前进（可以理解此时为uint32的负数状态）
    // offset 是无符号整数，直接相加就是 n 与 offset 的反向周期距离
    // 此时只需要判断真正需要相减的情况
    uint64_t ret = checkpoint + offset;
    if (offset >= (1u << 31) && ret >= (1ul << 32)) {
        ret -= (1ul << 32);  // 等于取了 offset 的负
    }
    return ret;
    //! 由于 wrap 返回的数据使用的是无符号数的范围，所以不建议使用有符号类型解决此问题
    //! 会引入更多需要判断的情况
}
