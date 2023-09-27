#include "Timer.hh"

Timer::Timer(uint64_t init_RTO) : initial_RTO_ms_(init_RTO), curr_RTO_ms_(init_RTO) 
{}

void Timer::start()
{
    running_ = true;
    time_ms_ = 0;
}

void Timer::stop()
{
    running_ = false;
}

bool Timer::is_running() const
{
    return running_;
}

bool Timer::is_expired() const 
{
    return running_ && (time_ms_ >= curr_RTO_ms_);
}

void Timer::tick(const size_t ms_since_last_tick)
{
    if(running_)
    {
        time_ms_ += ms_since_last_tick;
    }
}

void Timer::double_RTO()
{
    curr_RTO_ms_ *= 2;
}

void Timer::reset_RTO()
{
    curr_RTO_ms_ = initial_RTO_ms_;
}