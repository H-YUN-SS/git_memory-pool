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
    std::atomic<Slot*> next; //原子指针 串联空闲Slot ，避免竞态 比mutex减少上下文切换开销
};

class MemoryPool
{

public:
    MemoryPool(size_t BlockSize = 4096);
    ~MemoryPool();

    void init(size_t);

    void* allocate();
    void deallocate(void*);

private:
    void allocateNewBlock();
    size_t padPointer(char* p, size_t align);

    bool pushFreeList(Slot* slot);
    Slot* popFreeList();
private:
    int BlockSize_;
    int SlotSize_;
    Slot* firstBlock_;
    Slot* curSlot_;
    std::atomic<Slot*> freeList_;
    Slot* lastSlot_;
    std::mutex mutexForBlock_;
};

class HashBucket
{
public:
    // 初始化 64 个内存池
    void initMemoryPool();

    // 申请、释放内存
    void* useMemory(size_t size);
    void  freeMemory(void* ptr, size_t size);

private:
    // 获取第 index 个内存池
    MemoryPool& getMemoryPool(int index);
};

template <typename T,typename... Args>
T* newElement(Args&&... args)
{
    T* p=nullptr;
    if((p=reinterpret_cast<T*>(HashBucket::useMemory(sizeof(T))))!=nullptr)
    {
        new(p)T(std::forward<Args>(args)...);
    }
    return p;
}


template <typename T>
void deleteElement(T* p)
{
    if(p)
    {
        p->~T();
        HashBucket::freeMemory(reinterpret_cast<void*>(p),sizeof(T));
    }
}


}
