#include "../include/MemoryPool.h"

namespace Kama_memoryPool
{
    MemoryPool::MemoryPool(size_t BlockSize)
    : BlockSize_(BlockSize)
    , SlotSize_(0)
    , firstBlock_(nullptr)
    , curSlot_(nullptr)
    , freeList_(nullptr)
    , lastSlot_(nullptr)
    {

    }
    MemoryPool::~MemoryPool()
    {

    }
    
    void* MemoryPool::allocate()
    {
        return nullptr;
    }

    void MemoryPool::deallocate(void* ptr)
    {

    }
    
    bool MemoryPool::pushFreeList(Slot*slot)
    {
        return false;
    }

    Slot* MemoryPool::popFreeList()
    {
        return nullptr;
    }
}