#pragma once
#include "ThreadCache.h"

namespace Kama_memoryPool
{
    /*
      @brief 内存池对外暴露的统一入口类
      上层业务代码只需要调用此类的静态方法即可申请/释放内存
      内部自动转发给线程本地缓存 ThreadCache 处理
     */
    class MemoryPool
    {
    public:
        /*
          @brief 对外内存分配接口
          @param size 申请的内存字节大小
          @return 分配好的内存块指针
         */
        static void* allocate(size_t size)
        {
            // 转发给单例 ThreadCache 执行内存分配
            return ThreadCache::getInstance()->allocate(size);
        }

        /*
          @brief 对外内存释放接口
          @param ptr 要释放的内存指针
          @param size 内存块的原始大小
         */
        static void deallocate(void* ptr, size_t size)
        {
            // 转发给单例 ThreadCache 执行内存释放
            ThreadCache::getInstance()->deallocate(ptr, size);
        }
    };

} // namespace Kama_memoryPool