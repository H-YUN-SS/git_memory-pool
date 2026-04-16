/*
管理系统级连续内存页（Span），按页数分类存储
向操作系统申请/释放内存页（跨平台兼容）
实现Span的分配、释放、合并（减少内存碎片）
维护页地址到Span的映射，支持反向查找
*/

#include "PageCache.h"


/*

#ifdef = if define 的缩写，预处理阶段（编译代码之前）就会执行判断
_WIN32 = Windows 系统专属的预定义宏
只要在 Windows 平台 用 MSVC/MinGW 等编译器编译代码，这个宏就会自动被定义
但是这里只写了linux的


Windows 用的系统 API：VirtualAlloc / VirtualFree
Linux 用的系统 API：mmap / munmap
这两个平台的函数完全不通用，所以必须用 #ifdef _WIN32 分开写：
*/

#ifdef _WIN32
#include <Windows.h>
#else
#include <sys/mman.h>
#endif

//内存清零 memset
#include<cstring>


namespace Kama_memoryPool
{
    // 函数功能：分配【numPages页】连续内存（Span）
    // 调用者：CentralCache
    // 返回值：Span的起始地址（void*）
    void* PageCache::allocateSpan(size_t numPages)
    {
        // 1. 互斥锁：PageCache是全局单例，多线程必须加锁
        // 2. lock_guard是RAII机制：构造自动加锁，析构自动解锁
        // 3. 绝对不会忘记解锁，避免死锁
        std::lock_guard<std::mutex>lock(mutex_);
        // freeSpans_ 是 map<页数, Span链表>
        // lower_bound(numPages) → 找到第一个【页数 >= numPages】的Span链表
        // 作用：找一个“足够大”的空闲Span来分配
        auto it = freeSpans_.lower_bound(numPages);
        if(it != freeSpans_.end())
        {
            //拿到span（链表头
            Span* span=it->second;

            //从空闲链表中移除Span
            //如果Span后面还有节点 -> 链表后移

            if(span -> next)
            {
                freeSpans_[it->first]=span->next;
            }
            //链表为空
            else
            {
                //从map删掉这个key
                freeSpans_.erase(it);
            }


            //Span分割
            // 如果这个Span比需要的页数大 → 需要拆分！
            // 比如：要1页，拿到4页 → 分1页出去，剩下3页还回去

            if(span->numPages > numPages)
            {
                // 创建新Span，存放“剩下的页数”
                Span* newSpan = new Span;
                // Span 是连续内存  PAGE_SIZE = 4096 字节
                //原Span地址：0x1000 占4页：0x1000 ~ 0x2000（连续）
                newSpan->pageAddr = static_cast<char*>(span->pageAddr)+numPages*PAGE_SIZE;
                
                // 新Span页数 = 原页数 - 分配出去的页数
                newSpan->numPages=span->numPages-numPages;
                newSpan->next=nullptr;

                //头插法 剩余Span还回空闲链表
                auto& list=freeSpans_[newSpan->numPages];
                newSpan->next=list;
                list=newSpan;
                // 原Span只保留需要的页数
                span->numPages = numPages;
            }

            //记录Span映射
            //spanMap[页地址] = Span结构体
            //作用：释放时，通过地址快速找到对应的Span
            spanMap_[span->pageAddr]=span;
            
            //返回Span起始地址（给CentralCache
            return span->pageAddr;


        }
    }
}
