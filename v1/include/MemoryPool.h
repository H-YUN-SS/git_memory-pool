#pragma once

#include <atomic>        //原子操作	Slot中的std::atomic
#include <cassert>       //断言	assert()检查条件
#include <cstdint>      
#include <iostream>      //输入输出	调试时打印信息
#include <memory>        //内存管理	operator new/delete
#include <mutex>         //互斥锁	std::mutex保护共享资源


namespace Kama_memoryPool
{
#define MEMORY_POOL_NUM 64      //内存池数量64
#define SLOT_BASE_SIZE 8        //最小槽8字节
#define MAX_SLOT_SIZE 512       //最大槽512字节
struct Slot
{
    std::atomic<Slot*>next; //原子指针 串联空闲Slot ，避免竞态 比mutex减少上下文切换开销
};


}
