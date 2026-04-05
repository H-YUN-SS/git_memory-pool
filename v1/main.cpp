#include <iostream>    
#include <thread>      
#include <vector>    

#include "./include/MemoryPool.h" 

using namespace Kama_memoryPool;



class P1
{
    int id_;
};

class P2
{
    int id_[5];
};

class P3
{
    int id_[10];
};

class P4
{
    int id_[20];
};
/*
ntimes = 每轮申请多少次
nworks = 用多少个线程
rounds = 跑多少轮
*/
void BenchmarkMemoryPool(size_t ntimes,size_t nworks,size_t rounds)
{

    std::vector<std::thread>vthread(nworks);
    //总时间变量
    size_t total_costtime=0;
    for (size_t k = 0; k < nworks; k++)
    {
        vthread[k]=std::thread([&](){
            for(size_t j=0;j<rounds;++j)
            {
                size_t begin1=clock();
                for (size_t i = 0; i < ntimes; i++)
                {
                    P1* p1 = newElement<P1>();
                    deleteElement<P1>(p1);

                    P2* p2 = newElement<P2>();
                    deleteElement<P2>(p2);

                    P3* p3 = newElement<P3>();
                    deleteElement<P3>(p3);

                    P4* p4 = newElement<P4>();
                    deleteElement<P4>(p4);
                }
                size_t end1=clock();
                total_costtime+=end1-begin1;
            }
        });
        
    }
    for(auto& t:vthread)
    {
        t.join();
    }
    std::cout << nworks << "个线程并发执行"
          << rounds << "轮次，每轮次newElement&deleteElement "
          << ntimes << "次，总计花费："
          << total_costtime << " ms" << std::endl;


}
void BenchmarkNew(size_t ntimes,size_t nworks,size_t rounds)
{
    std::vector<std::thread>vthread(nworks);
    size_t total_costtime=0;

    for (size_t k = 0; k < nworks; k++)
    {
        vthread[k]=std::thread([&](){
            for(size_t j=0;j<rounds;++j)
            {
                size_t begin1=clock();
                for (size_t i = 0; i < ntimes; i++)
                {
                    P1* p1 = new P1;
                    delete p1;

                    P2* p2 = new P2;
                    delete p2;

                    P3* p3 = new P3;
                    delete p3;

                    P4* p4 = new P4;
                    delete p4;
                }
                size_t end1=clock();
                total_costtime+=end1-begin1;
            }
        });
    }

    for(auto& t:vthread)
    {
        t.join();
    }

    std::cout << nworks << "个线程并发执行"
              << rounds << "轮次，每轮次new/delete "
              << ntimes << "次，总计花费："
              << total_costtime << " ms" << std::endl;
}


int main()
{
    // 初始化内存池
    HashBucket::initMemoryPool();
    std::cout << "=== 内存池初始化成功 ===" << std::endl << std::endl;

    // 测试：内存池
    std::cout << "===== 测试：内存池 =====" << std::endl;
    BenchmarkMemoryPool(100, 1, 10);

    std::cout << std::endl;

    // 测试：new/delete
    std::cout << "===== 测试：new/delete =====" << std::endl;
    BenchmarkNew(100, 1, 10);


/*
    === 内存池初始化成功 ===

    ===== 测试：内存池 =====
    1个线程并发执行10轮次，每轮次newElement&deleteElement 100次，总计花费：144 ms

    ===== 测试：new/delete =====
    1个线程并发执行10轮次，每轮次new/delete 100次，总计花费：66 ms
*/

    return 0;
}