#include "../include/ThreadCache.h"
#include "../include/CentralCache.h"


namespace Kama_memoryPool
{
    //① 线程向ThreadCache申请内存
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
        //本地链表空了 -> 去中心缓存 CentralCache 批量取
        return fetchFromCentralCache(index);
    }

    //② 线程向 ThreadCache 释放内存
    void ThreadCache::deallocate(void* ptr,size_t size)
    {
        //大对象直接调用系统 free
        if(size > MAX_BYTES)
        {
            free(ptr);
            return;
        }
        
        //找到对应大小桶
        size_t index = SizeClass::getIndex(size);

        //头插法归还
        *reinterpret_cast<void**>(ptr)=freeList_[index];
        freeList_[index]=ptr;

        freeListSize_[index]++;

        //如果本地块太多，归还一部分给CentralCache
        if(shouldReturnToCentralCache(index))
        {
            returnToCentralCache(freeList_[index],size);
        }
    }

    //③ 判断本地空闲块是否太多
    bool ThreadCache::shouldReturnToCentralCache(size_t index)
    {
        size_t threshold = 256;
        //超过256块就归还 避免一个线程占太多内存
        return (freeListSize_[index]>threshold);
    }

    //④ 从CentralCache 批量获取一批内存块
    void* ThreadCache::fetchFromCentralCache(size_t index)
    {
        //向CentralCache申请一整串链表
        void* start = CentralCache::getInstance().fetchRange(index);
        if(!start)return nullptr;

        //拿第一个块给用户
        void* result = start;

        //剩下的块 -> 挂到ThreadCache本地链表
        freeList_[index] = *reinterpret_cast<void**>(start);

        //遍历统计这批有多少个块
        size_t batchNum = 0;
        void* current = start;

        while(current!= nullptr)
        {
            batchNum++;
            current=*reinterpret_cast<void**>(current);//走到下一个块
        }

        //更新计数
        freeListSize_[index] += batchNum;

        //返回第一个块给用户
        return result;

    }

    //⑤将本地多余的内存 批量归还给CentralCache
    void ThreadCache::returnToCentralCache(void* start,size_t size)
    {
        //找桶
        size_t index = SizeClass::getIndex(size);

        //对齐后真实块大小
        size_t alignedSize = SizeClass::roundUp(size);

        //这个桶的块总数
        size_t batchNum = freeListSize_[index];
        if(batchNum <= 1)
        {
            return;
        }

        //策略 保留四分之一
        size_t keepNum = std::max(batchNum / 4,size_t(1));
        size_t returnNum = batchNum - keepNum;

        //初始化遍历指针
        char* current = static_cast<char*>(start);
        char* spliNode = current;

        //遍历 找到保留部分的最后一块
        for(size_t i = 0;i < keepNum - 1;i++)
        {
            //下一个节点
            spliNode = reinterpret_cast<char*>(*reinterpret_cast<void**>(spliNode));
            if(spliNode == nullptr)
            {
                //链表提前结束 修改归还数量
                returnNum = batchNum - (i+1);
                break;
            }
        }

        if(spliNode != nullptr)
        {
            //1.断开链表
            //要归还的链表头
            void* nextNode = *reinterpret_cast<void**>(spliNode);
            
            //保留部分的最后一个节点 next置空（断开）
            *reinterpret_cast<void**>(spliNode) = nullptr;

            //2.更新线程缓存的自由链表（只保留keepNum个）
            freeList_[index] = start;

            //更新计数
            freeListSize_[index]=keepNum;

            //3.多余部分还给中心缓存
            if(returnNum >0 && nextNode !=nullptr)
            {
                //returnRange(归还链表头 总字节数 下标)
                CentralCache::getInstance().returnRange(nextNode,returnNum*alignedSize,index);
            }
        }


    }

    



}