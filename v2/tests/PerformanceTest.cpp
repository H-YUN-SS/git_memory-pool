#include "../include/MemoryPool.h"
#include <iostream>
#include <vector>
#include <chrono>
#include <random>
#include <iomanip>  //输出格式控制 设置浮点数精度 （保留三位小数
#include <thread>
#include <array>

using namespace Kama_memoryPool;
using namespace std::chrono;

class Timer
{
    //私有成员变量：存储计时器启动时的时间点
    high_resolution_clock::time_point start;

public:
    //构造函数：创建Timer对象时自动调用，记录当前时间
    //【语法】构造函数初始化列表：比在函数体内赋值更高效

    Timer() : start(high_resolution_clock::now()){}

    //成员函数：计算从Timer创建到现在经过的时间（单位：毫秒）
    double elapsed()
    {
        //获取当前时间点
        auto end = high_resolution_clock::now();

        //计算时间差 ： end - start = 微秒数 ->除以1000转成毫秒
        //duration_cast:强制转换时间单位
        return duration_cast<microseconds>(end-start).count()/1000.0;
    }
};

//性能测试类 所有性能测试逻辑都在这里
class PerformanceTest
{
private:
    //内部结构体：用来存储测试统计数据
}