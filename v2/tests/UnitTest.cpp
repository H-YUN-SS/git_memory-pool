#include "../include/MemoryPool.h"  
#include <iostream>                 
#include <vector>                   // 容器：存储多线程/压力测试中的内存分配记录
#include <thread>                   
#include <cassert>                  // 断言：验证内存池行为是否符合预期（失败则直接终止程序）
#include <cstring>                  
#include <random>                   
#include <algorithm>                // 算法库：shuffle（随机打乱释放顺序）
#include <atomic>                   // 原子变量：多线程中安全标记错误状态

using namespace Kama_memoryPool;   

// 一.基础分配测试：验证小/中/大内存块的分配与释放基本功能
void testBasicAllocation()
{
    std::cout<<"Running testBasicAllocation ..."<<std::endl;

    //1.测试小内存分配 （8字节 如单个int/指针）
    void* ptr1 = MemoryPool::allocate(8);//调用内存池静态方法allocate分配8字节
    assert(ptr1 != nullptr);             //断言：分配结果非空 失败则程序终止
    MemoryPool::deallocate(ptr1,8);      //释放内存：必须传入分配的大小 内存池按照大小分类管理

    //2.测试中等大小内存分配 （1024字节 1KB 常见中等对象）
    void* ptr2 = MemoryPool::allocate(1024);
    assert(ptr2 != nullptr);
    MemoryPool::deallocate(ptr2,1024);

    //3.测试大内存分配 （超过内存池MAX_BYTES阈值）
    //内存池对“大内存走系统malloc 验证分支逻辑
    void* ptr3 = MemoryPool::allocate(1024*1024);//1MB
    assert(ptr3 != nullptr);
    MemoryPool::deallocate(ptr3,1024*1024);
    
    std::cout<<"testBasicAllocation passed！"<<std::endl;


}

// 二、内存写入测试：分配内存 → 写入数据 → 读取验证 → 释放
// 目的：确保分配的内存可正常读写，没有越界/损坏
void testMemoryWriting()
{
    std::cout << "Running memory writing test..." << std::endl;

    //要分配的内存大小 ： 128字节
    const size_t size = 128;
    //allocate 返回 void* 强转成char* 方便按字节写入
    char* ptr = static_cast<char*>(MemoryPool::allocate(size));
    //断言：分配成功
    assert(ptr != nullptr);

    //--- 写入数据
    //循环0-127 每个位置写入i%256 (char范围)
    for(size_t i = 0;i < size;i++)
    {
        ptr[i]=static_cast<char>( i% 256);
    }

    //---验证数据
    for(size_t i = 0;i < size;i++)
    {
        // 断言：读取的值 == 写入的值
        assert(ptr[i] == static_cast<char>(i % 256));
    }

    //释放内存
    MemoryPool::deallocate(ptr,size);
    std::cout << "Memory writing test passed!" << std::endl;

}

// 三、多线程测试
// 函数功能 ：多线程同时申请/释放内存，测试内存池线程安全
void testMultiThreading()
{
    std::cout<< "Running multi-threading test..." <<std::endl;

    // 线程数量 4个
    const int NUM_THREADS = 4;
    // 每个线程分配次数 1000次
    const int ALLOCS_PER_THREAD = 1000;

    std::atomic<bool> has_error{false};

    auto threadFunc = [ &has_error]()
    {
        try
        {
            //vector 存 内存指针，分配大小
            //把所有分配的空间地址存起来
            std::vector<std::pair<void*,size_t>>allocations;
            //预先分配空间 提升效率
            allocations.reserve(ALLOCS_PER_THREAD);

            //循环分配内存
            for(int i=0;i<ALLOCS_PER_THREAD && !has_error;i++)
            {
                //随机大小 ：1-256*8字节 （内存池按对齐块分配
                size_t size=(rand()%256+1)*8;

                //分配内存
                void* ptr = MemoryPool::allocate(size);

                //分配失败 标记错误
                if(!ptr)
                {
                    std::cerr<<"Allocation failed for size: "<<size<<std::endl;
                    has_error=true;
                    break;
                }

                //把指针和大小存入vector
                allocations.push_back({ptr,size});

                //随机释放: 50%概率释放一个已分配的内存
                //rand()%2
                if(rand()%2 && !allocations.empty())
                {
                    //随机选一个索引
                    size_t index = rand() % allocations.size();
                    //释放该内存
                    MemoryPool::deallocate(allocations[index].first,allocations[index].second);

                    //从vector删除该记录
                    allocations.erase(allocations.begin()+index);
                }


            }
            //最后把剩余没释放的内存全部释放
            for(const auto& alloc:allocations)
            {
                MemoryPool::deallocate(alloc.first,alloc.second);
            }
        }
        catch(const std::exception& e)
        {
            std::cerr <<"Thread exception"<< e.what() << '\n';
            has_error = true;
        }
        
    };
    
    //创建并启动4个线程
    std::vector<std::thread>threads;
    for(int i=0;i<NUM_THREADS;i++)
    {
        //emplace_back: 直接在vector内构造线程对象
        threads.emplace_back(threadFunc);
    }

    //等待所有线程执行完毕
    for(auto& thread:threads)
    {
        thread.join();
    }
    std::cout<<"Multi-threading test passed!"<<std::endl;
}

// 四、边界测试
// 函数功能：测试极端/异常情况，内存池是否能正确处理
void testEdgeCases()
{
    std::cout<<"Running edge cases test..."<<std::endl;

    //测试1 分配0字节
    void* ptr1 = MemoryPool::allocate(0);
    assert(ptr1 != nullptr);
    MemoryPool::deallocate(ptr1,0);

    //测试2 分配1字节
    void* ptr2 = MemoryPool::allocate(1);
    assert(ptr2 != nullptr);
    // 断言：内存地址必须按 ALIGNMENT 对齐
    // 内存池分配的地址一定是对齐的，否则访问会崩溃/变慢
    assert((reinterpret_cast<uintptr_t>(ptr2) & (ALIGNMENT -1 ))==0);
    MemoryPool::deallocate(ptr2,1);

    //测试3 分配内存池最大支持大小
    void* ptr3 = MemoryPool::allocate(MAX_BYTES);
    assert(ptr3 != nullptr);
    MemoryPool::deallocate(ptr3,MAX_BYTES);

    //测试4 分配超过内存池最大大小
    //超过大小 ->直接调用系统malloc
    void* ptr4 = MemoryPool::allocate(MAX_BYTES +1 );
    assert(ptr4 != nullptr);
    MemoryPool::deallocate(ptr4,MAX_BYTES +1 );
    std::cout<<"Edge cases test passed!"<<std::endl;
    



}

//压力测试
void testStress()
{
    std::cout<<"Running stress test..."<<std::endl;

    //分配总次数：10000次
    const int NUM_ITERATIONS = 10000;
    std::vector<std::pair<void*,size_t>> allocations;
    allocations.reserve(NUM_ITERATIONS);
    
    //连续分配 10000 块 随机大小内存
    for(int i = 0; i < NUM_ITERATIONS;i++)
    {
        size_t size = (rand() % 1024 + 1)*8;
        void* ptr = MemoryPool::allocate(size);
        assert(ptr!= nullptr);
        allocations.push_back({ptr,size});
        
    }

    //随机打乱顺序（模拟真实场景乱序释放
    std::random_device rd;
    std::mt19937 g(rd());
    std::shuffle(allocations.begin(),allocations.end(),g);

    //释放所有内存
    for(const auto& alloc:allocations)
    {
        MemoryPool::deallocate(alloc.first,alloc.second);
    }


}


int main() 
{
    try 
    {
        std::cout << "Starting memory pool tests..." << std::endl;

        testBasicAllocation();
        testMemoryWriting();
        testMultiThreading();
        testEdgeCases();
        testStress();

        std::cout << "All tests passed successfully!" << std::endl;
        return 0;
    }
    catch (const std::exception& e) 
    {
        std::cerr << "Test failed with exception: " << e.what() << std::endl;
        return 1;
    }
}