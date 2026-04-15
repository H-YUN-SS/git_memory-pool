#pragma once
#include "Common.h"

namespace Kama_memoryPool
{



    // 线程局部单例模式
    // static thread_local：每个线程创建自己独立的 instance
    // 作用：保证每个线程只有一个 ThreadCache
    class ThreadCache
    {
        public:
        static ThreadCache* getInstance()
        {
            //线程单例模式
            static thread_local ThreadCache instance;
            return &instance;
        }

        //申请内存
        void* allocate(size_t size);
        //释放内存
        void* deallocate(void * ptr,size_t size);

        private:
        ThreadCache()
        {
            freeList_.fill(nullptr);//自由列表全置空
            freeListSize_.fill(0);//大小计数全为0
        }
        //从中心缓存 CEntralCache 拿内存 （要求自身freeList_没有空闲
        void* fetchFromCentralCache(size_t index);
        //归还内存给中心缓存 
        void returnToCentralCache(void* start,size_t size);
        //判断是否归还
        bool shouldReturnToCentralCache(size_t index);

        private:
        //每个线程的自由链表数组
        std::array<void*,FREE_LIST_SIZE>freeList_;
        //记录有多少块空闲内存
        std::array<size_t,FREE_LIST_SIZE>freeListSize_;
    };
}