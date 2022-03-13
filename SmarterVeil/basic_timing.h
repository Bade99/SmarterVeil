#pragma once

//TODO(fran): move to OS specific code

LARGE_INTEGER _QueryPerformanceFrequency()
{
    LARGE_INTEGER res{};
    QueryPerformanceFrequency(&res);//NOTE(fran): always works on Windows XP and above
    return res;
}

enum class counter_timescale { seconds = 1, milliseconds = 1000, microseconds = 1000000, };
internal f64 GetPCFrequency(counter_timescale timescale)
{
    local_persistence LARGE_INTEGER li = _QueryPerformanceFrequency(); //NOTE(fran): fixed value, can be cached
    return f64(li.QuadPart) / (f64)timescale;
}
internal i64 StartCounter()
{
    LARGE_INTEGER li;
    QueryPerformanceCounter(&li);
    return li.QuadPart;
}
internal f64 EndCounter(i64 CounterStart, counter_timescale timescale = counter_timescale::milliseconds)
{
    LARGE_INTEGER li;
    QueryPerformanceCounter(&li);
    return double(li.QuadPart - CounterStart) / GetPCFrequency(timescale);
}