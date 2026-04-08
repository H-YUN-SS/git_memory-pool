//（基础常量、大小类、对齐）
#pragma once
#include <cstddef>
#include <algorithm>
// 全局常量：内存对齐大小（8字节，x86/x64通用）
constexpr size_t ALIGNMENT = 8;
// 内存池管理的最大字节数（超过则直接用系统malloc）
constexpr size_t MAX_BYTES = 256*1024;
//内存池只管 ≤256KB 的小内存，超过就直接调用系统 malloc，避免内存池浪费。

//按大小分类的 32768 条自由链表
constexpr size_t FREE_LIST_SIZE=MAX_BYTES/ALIGNMENT;

//内存块头部信息
struct BlockHeader
{
    //内存块大小
    size_t size;
    //使用标志 T使用 F空闲可分配
    bool inUse;
    //指向下一个内存池
    BlockHeader* next;

};


// 大小类管理类：负责将任意大小向上取整到对齐值，并计算索引
class SizeClass
{
    public:
    //功能：将输入字节数向上取整到ALIGNMENT的倍数 
    static size_t roundUp(size_t bytes)
    {
        // (n + align-1) & ~(align-1)        (bytes + 7) & ~7
        return (bytes +ALIGNMENT -1 )&~(ALIGNMENT -1);
    }
    // 功能：计算对齐后的大小对应的索引（用于内存池的桶映射）
    //bytes=8 →0，bytes=16→1，bytes=24→2...
    static size_t getIndex(size_t bytes)
    {
        bytes = std::max(bytes,ALIGNMENT);
        //先向上取整，再除以ALIGNMENT，最后-1得到索引
        return (bytes + ALIGNMENT - 1) / ALIGNMENT - 1;
    }
    
};