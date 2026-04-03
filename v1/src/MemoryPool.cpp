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
        Slot* cur=firstBlock_;
        while(cur!=nullptr)
        {
            Slot* next=cur->next;
            operator delete(reinterpret_cast<void*>(cur));
            cur=next;
        }
    }
    
    void* MemoryPool::allocate()
    {
        Slot* slot=popFreeList();
        if(slot!=nullptr)
        {
            return slot;
        }
        Slot* temp;
        {
            std::lock_guard<std::mutex>lock(mutexForBlock_);
            if(curSlot_>=lastSlot_)
            {
                allocateNewBlock();
            }
        
        temp=curSlot_;
        curSlot_+=SlotSize_/sizeof(slot);//curslot_&&slot都是slot*类型
        }
        return temp;
    }

    void MemoryPool::deallocate(void* ptr)
    {
        if(!ptr)//空指针
        {
            return;
        }
        Slot* slot=reinterpret_cast<Slot*>(ptr);
        pushFreeList(slot);

    }
    
    bool MemoryPool::pushFreeList(Slot*slot)
    {
        while(true)
        {
            Slot* oldHead=freeList_.load(std::memory_order_relaxed);
            slot->next.store(oldHead, std::memory_order_relaxed);
            if(freeList_.compare_exchange_weak(oldHead,slot))
            {
                return true;
            }
        }
    }

    Slot* MemoryPool::popFreeList()
    {
        while(true)
        {
            Slot* oldHead=freeList_.load(std::memory_order_acquire);
            if(oldHead==nullptr)
            {
                return nullptr;
            }
            Slot* newHead=oldHead->next.load(std::memory_order_relaxed);
            
            if(freeList_.compare_exchange_weak(oldHead,newHead))
            {
                return oldHead;
            }
        }
    }
}