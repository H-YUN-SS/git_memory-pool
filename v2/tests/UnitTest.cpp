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


int main()
{
    try
    {
        std::cout<<"Starting memory pool tests..."<<std::endl;


    }
    catch(const std::exception& e)
    {
        std::cerr <<"Test failed with exception" << e.what() << '\n';
    }
    
}