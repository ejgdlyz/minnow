#pragma once

#include <iostream>

class Timer
{
private:
    uint64_t initial_RTO_ms_;
    uint64_t curr_RTO_ms_;  // 记录现在的 RTO
    size_t time_ms_ {0};  // 运行时间
    bool running_ {false};  // 是否启动定时器

public:
    explicit Timer(uint64_t init_RTO); 

    void start();

    void stop();

    bool is_running() const;

    bool is_expired() const;

    void tick(const size_t ms_since_last_tick);

    void double_RTO();

    void reset_RTO();
};
