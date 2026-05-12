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
    struct TestStats
    {
        double memPoolTime{0.0};    //内存池耗时
        double systemTime{0.0};     //系统new/delete耗时
        size_t totalAlloc{0};       //总分配次数
        size_t totalBytes{0};       //总分配字节数

    };
public:
    //1.系统预热
    //操作系统和内存池首次运行时会有初始化开销（如分配页表、创建缓存）
    //预热可以消除这些一次性开销，让测试结果更准确
    static void warmup() 
    {
        std::cout << "Warming up memory systems...\n";
        
        // 创建vector存储预热时分配的内存指针和大小
        // pair<void*, size_t>：第一个元素是内存指针，第二个是分配的大小
        std::vector<std::pair<void*, size_t>> warmupPtrs;
        
        // 循环1000次，分配各种常见大小的内存
        for (int i = 0; i < 1000; ++i) 
        {
            // 【语法】范围for循环（C++11）：遍历数组中的每个元素
            // 这里遍历8、16、32...1024这些内存池最常用的大小
            for (size_t size : {8, 16, 32, 64, 128, 256, 512, 1024}) {
                // 分配内存
                void* p = MemoryPool::allocate(size);
                // 【语法】emplace_back：直接在vector末尾构造对象，比push_back高效
                warmupPtrs.emplace_back(p, size);
            }
        }
        
        // 释放所有预热分配的内存
        // 【语法】结构化绑定（C++17）：直接从pair中提取ptr和size
        // 等价于：for (const auto& item : warmupPtrs) {
        //           void* ptr = item.first;
        //           size_t size = item.second;
        //         }
        for (const auto& [ptr, size] : warmupPtrs) 
        {
            MemoryPool::deallocate(ptr, size);
        }
        
        std::cout << "Warmup complete.\n\n";
    }

    //小对象分配性能测试
    static void testSmallAllocation()
    {
        constexpr size_t NUM_ALLOCS = 50000; //总共分配50000次

        const size_t SIZES[] = {8,16,32,64,128,256};

        // 计算数组元素个数：总字节数 / 单个元素字节数
        const size_t NUM_SIZES = sizeof(SIZES) / sizeof(SIZES[0]);
        std::cout << "\nTesting small allocations (" << NUM_ALLOCS 
                  << " allocations of fixed sizes):" << std::endl;


        //测试内存池
        {
            Timer t;
            
            std::array<std::vector<std::pair<void*,size_t>>,NUM_SIZES> sizePtrs;
            //预分配vector空间
            for(auto& ptrs : sizePtrs)
            {
                ptrs.reserve(NUM_ALLOCS / NUM_SIZES);
            }

            //循环分配5万次内存
            for(size_t i = 0; i < NUM_ALLOCS; i++)
            {
                //循环使用不同大小的内存，模拟真实场景
                size_t sizeIndex = i % NUM_SIZES;
                size_t size = SIZES[sizeIndex];

                //用内存池分配内存
                void*ptr = MemoryPool::allocate(size);
                //存入对应大小的vector中
                sizePtrs[sizeIndex].push_back({ptr,size});

                //模拟真实使用，每四次分配随机释放一个已分配的内存
                if(i%4==0)
                {
                    //随机选择一个大小类别
                    size_t releaseIndex = rand() % NUM_SIZES;
                    auto& ptrs = sizePtrs[releaseIndex];

                    //如果该类别有内存，释放最后一个
                    if(!ptrs.empty())
                    {
                        MemoryPool::deallocate(ptrs.back().first,ptrs.back().second);
                        ptrs.pop_back();
                    }
                }
            }

            for(auto& ptrs : sizePtrs)
            {
                for(const auto& [ptr,size] : ptrs)
                {
                    MemoryPool::deallocate(ptr,size);
                }
            }

            std::cout<<"Memory Pool: "<<std::fixed <<std::setprecision(3)<<t.elapsed()<<" ms"<<std::endl;

            
        }


        //测试系统new/delete
        {
            Timer t;
            std::array<std::vector<std::pair<void*,size_t>>,NUM_SIZES>sizePtrs;
            for (auto& ptrs : sizePtrs)
            {
                ptrs.reserve(NUM_ALLOCS / NUM_SIZES);
            }

            for(size_t i = 0;i < NUM_ALLOCS; i++)
            {
                size_t sizeIndex = i % NUM_SIZES;
                size_t size = SIZES[sizeIndex];
                // 用系统new分配内存：分配size个char大小的空间
                void* ptr = new char[size];
                sizePtrs[sizeIndex].push_back({ptr,size});

                if(i % 4 == 0)
                {
                    size_t releaseIndex = rand() % NUM_SIZES;
                    auto& ptrs = sizePtrs[releaseIndex];

                    if(!ptrs.empty())
                    {
                        //用系统delete释放数组内存
                        delete[] static_cast<char*>(ptrs.back().first);
                        ptrs.pop_back();
                    }
                }
            }
            for(auto& ptrs : sizePtrs)
            {
                for(const auto&[ptr,size] : ptrs)
                {
                    delete[] static_cast<char*>(ptr);
                }
            }

            std::cout<< "New/Delete: " << std::fixed << std::setprecision(3)  << t.elapsed() << " ms" << std::endl;
        }


    }
};