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
            static thread_local ThreadCache instance;
            return &instance;
        }
    };
}