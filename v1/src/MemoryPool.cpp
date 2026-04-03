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
        curSlot_+=SlotSize_/sizeof(Slot);//curslot_&&slot都是slot*类型
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
            //要是原子变量的写入 后面访问了就需要用memory_order_acquire 无访问就可以memory_order_relaxed
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
    void MemoryPool::init(size_t size)
    {
        assert(size>0);//不可能的错误assert抛出
        SlotSize_=size;

        firstBlock_=nullptr;
        curSlot_=nullptr;
        freeList_=nullptr;
        lastSlot_=nullptr;
    
    }


    void MemoryPool::allocateNewBlock()//开新块
    {
        void* newBlock=operator new(BlockSize_);

        reinterpret_cast<Slot*>(newBlock)->next.store(firstBlock_);
        firstBlock_=reinterpret_cast<Slot*>(newBlock);  
        //因为是开新块 所以需要内存对齐 
        char *body=reinterpret_cast<char*>(newBlock)+sizeof(Slot);

        size_t padding = padPointer(body,SlotSize_);
        curSlot_=reinterpret_cast<Slot*>(body+padding);

        lastSlot_=reinterpret_cast<Slot*>(reinterpret_cast<char*>(newBlock)+BlockSize_-SlotSize_+1);
    }

    size_t MemoryPool::padPointer(char* p,size_t align)
    {
        size_t rem=reinterpret_cast<size_t>(p)%align;
        return rem==0?0:align-rem;
    }

    void HashBucket::initMemoryPool()
    {
        for(int i=0;i<MEMORY_POOL_NUM ;i++)
        {
            getMemoryPool(i).init((i+1)*SLOT_BASE_SIZE);
        }
    }
    MemoryPool& HashBucket::getMemoryPool(int index)
    {
        static MemoryPool memoryPool[MEMORY_POOL_NUM];
        return memoryPool[index];
    }
    void* HashBucket::useMemory(size_t size)
    {
        if(size==0||size>MAX_SLOT_SIZE)
        {
            return nullptr;
        }
        int index=(size + SLOT_BASE_SIZE-1)/SLOT_BASE_SIZE-1;
        return getMemoryPool(index).allocate();
    }

    void HashBucket::freeMemory(void *ptr,size_t size)
    {
        if(!ptr||size==0||size>MAX_SLOT_SIZE)
        {
            return;
        }
        int index=(size+SLOT_BASE_SIZE-1)/SLOT_BASE_SIZE-1;
        getMemoryPool(index).deallocate(ptr);
    }
}