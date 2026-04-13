//（最底层：页分配、合并）

#pragma once

#include "Common.h"
#include<map>
#include<mutex>

namespace Kama_memoryPool
{
    class PageCache
    {
        public:
        //操作系统一页固定 = 4096 字节（4KB） 内存池必须按页申请内存
        static const size_t PAGESIZE=4096;
        static PageCache& getInstance()
        {
            static PageCache instance;
            return instance;
        }
        // 分配指定页数的span 申请numPages 页 连续内存（比如 1 页 = 4KB，2 页 = 8KB）
        void * allocateSpan(size_t numPages);

        // 释放span
        void *deallocateSpan(void* ptr, size_t numPages);

        private:


        //单例模式要求：构造函数私有化
        //外部不能 new PageCache()，只能通过 getInstance() 获取实例
        PageCache() = default;

        //真正调用操作系统 API（Linux：mmap / Windows：VirtualAlloc）
        void* systemAlloc(size_t numPages); 
        struct Span
        {
            void*  pageAddr;   // 页起始地址
            size_t numPages;   // 占几页
            Span*  next;       // 链表指针
        };
        void* systemAlloc(size_t numPages);

        // 按页数管理空闲span
        //key：页数（1、2、3、4...） value：对应页数的 Span 链表
        std::map<size_t, Span*> freeSpans_;
        //// 页号到span的映射，用于释放
        
        std::map<void*, Span*> spanMap_;
        std::mutex mutex_;
    };
    

};