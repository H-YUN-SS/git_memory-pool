#include "../include/ThreadCache.h"
#include "../include/CentralCache.h"


namespace Kama_memoryPool
{
    //线程向ThreadCache申请内存
    void* ThreadCache::allocate(size_t size)
    {
        if(size == 0)
        {
            size=ALIGNMENT;
        }
        //超过256KB大对象 直接调用系统malloc 不进内存池
        if(size > MAX_BYTES)
        {
            return malloc(size);
        }

        //计算这个大小属于第几个自由链表（分桶）
        size_t index = SizeClass::getIndex(size);

        //准备分配 计数-1
        freeListSize_[index]--;

        //如果本地自由链表有可用块直接用第一个
        if(void* ptr = freeList_[index])
        {
            freeList_[index] = *reinterpret_cast<void**>(ptr);

            return ptr;
        }

    }
}