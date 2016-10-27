//------------------------------------------------------------------------------
// 
//     Testing different priority queues
//
//------------------------------------------------------------------------------

#include <cstdio>
#include <cstdlib>
#include <cassert>
#include <vector>
#include <array>
#include <set>
#include <thread>
#include <mutex>
#include <boost/random.hpp>
#include <sched.h>
#include "common/timehelper.h"
#include "common/threadbarrier.h"
#include "bench/setadaptor.h"
#include "bench/mapadaptor.h"

template<typename T>
void WorkThread(uint32_t numThread, int threadId, uint32_t testSize, uint32_t tranSize, uint32_t keyRange, uint32_t insertion, uint32_t deletion, ThreadBarrier& barrier,  T& set)
{
    //set affinity for each thread
    cpu_set_t cpu = {{0}};
    CPU_SET(threadId, &cpu);
    sched_setaffinity(0, sizeof(cpu_set_t), &cpu);

    double startTime = Time::GetWallTime();

    boost::mt19937 randomGenKey;
    boost::mt19937 randomGenOp;
    randomGenKey.seed(startTime + threadId);
    randomGenOp.seed(startTime + threadId + 1000);
    boost::uniform_int<uint32_t> randomDistKey(1, keyRange);
    boost::uniform_int<uint32_t> randomDistOp(1, 100);
    
    set.Init();

    barrier.Wait();
    
    SetOpArray ops(tranSize);

    for(unsigned int i = 0; i < testSize; ++i)
    {
        for(uint32_t t = 0; t < tranSize; ++t)
        {
            uint32_t op_dist = randomDistOp(randomGenOp);
            ops[t].type = op_dist <= insertion ? INSERT : op_dist <= insertion + deletion ? DELETE : FIND;
            ops[t].key  = randomDistKey(randomGenKey);
        }

        set.ExecuteOps(ops);
    }

    set.Uninit();
}


template<typename T>
void Tester(uint32_t numThread, uint32_t testSize, uint32_t tranSize, uint32_t keyRange, uint32_t insertion, uint32_t deletion,  SetAdaptor<T>& set)
{
    std::vector<std::thread> thread(numThread);
    ThreadBarrier barrier(numThread + 1);

    double startTime = Time::GetWallTime();
    boost::mt19937 randomGen;
    randomGen.seed(startTime - 10);
    boost::uniform_int<uint32_t> randomDist(1, keyRange);

    set.Init();

    SetOpArray ops(1);

    for(unsigned int i = 0; i < keyRange; ++i)
    {
        ops[0].type = INSERT;
        ops[0].key  = randomDist(randomGen);
        set.ExecuteOps(ops);
    }

    //Create joinable threads
    for (unsigned i = 0; i < numThread; i++) 
    {
        thread[i] = std::thread(WorkThread<SetAdaptor<T> >, numThread, i + 1, testSize, tranSize, keyRange, insertion, deletion, std::ref(barrier), std::ref(set));
    }

    barrier.Wait();

    {
        ScopedTimer timer(true);

        //Wait for the threads to finish
        for (unsigned i = 0; i < thread.size(); i++) 
        {
            thread[i].join();
        }
    }

    set.Uninit();
}


template<typename T>
void MapWorkThread(uint32_t numThread, int threadId, uint32_t testSize, uint32_t tranSize, uint32_t keyRange, uint32_t insertion, uint32_t deletion, uint32_t update, ThreadBarrier& barrier,  T& map)
{
    //set affinity for each thread
    cpu_set_t cpu = {{0}};
    CPU_SET(threadId, &cpu);
    sched_setaffinity(0, sizeof(cpu_set_t), &cpu);

    double startTime = Time::GetWallTime();

    boost::mt19937 randomGenKey;
    boost::mt19937 randomGenOp;
    randomGenKey.seed(startTime + threadId);
    randomGenOp.seed(startTime + threadId + 1000);
    boost::uniform_int<uint32_t> randomDistKey(1, keyRange);
    boost::uniform_int<uint32_t> randomDistOp(1, 100);
    
    map.Init();

    barrier.Wait();
    
    MapOpArray ops(tranSize);

    for(unsigned int i = 0; i < testSize; ++i)
    {
        for(uint32_t t = 0; t < tranSize; ++t)
        {
            uint32_t op_dist = randomDistOp(randomGenOp);
            //ops[t].type = op_dist <= insertion ? INSERT : op_dist <= insertion + deletion ? DELETE : FIND;
            if(op_dist <= insertion)
            {
                ops[t].type = MAP_INSERT;
                ops[t].value = randomDistKey(randomGenKey);
            }
            else if(op_dist <= insertion + deletion)
            {
                ops[t].type = MAP_DELETE;
                ops[t].value = 0;
            }
            else if(op_dist <= insertion + deletion + update)
            {
                ops[t].type = MAP_UPDATE;
                ops[t].value = randomDistKey(randomGenKey);
            }
            else
            {
                ops[t].type = MAP_FIND;
                ops[t].value = 0;
            }

            ops[t].key  = randomDistKey(randomGenKey);
        }

        map.ExecuteOps(ops, threadId);
    }

    map.Uninit();
}


template<typename T>
void MapTester(uint32_t numThread, uint32_t testSize, uint32_t tranSize, uint32_t keyRange, uint32_t insertion, uint32_t deletion, uint32_t update, MapAdaptor<T>& map)
{
    std::vector<std::thread> thread(numThread);
    ThreadBarrier barrier(numThread + 1);

    double startTime = Time::GetWallTime();
    boost::mt19937 randomGen;
    randomGen.seed(startTime - 10);
    boost::uniform_int<uint32_t> randomDist(1, keyRange);

    map.Init();

    MapOpArray ops(1);

    for(unsigned int i = 0; i < keyRange; ++i)
    {
        ops[0].type = INSERT;
        ops[0].key  = randomDist(randomGen);
        ops[0].value = randomDist(randomGen);
        // all prefill gets done by thread 0, the main thread; worker threads start numbering at 1
        map.ExecuteOps(ops, 0); 
    }

    //Create joinable threads
    for (unsigned i = 0; i < numThread; i++) 
    {
        thread[i] = std::thread(MapWorkThread<MapAdaptor<T> >, numThread, i + 1, testSize, tranSize, keyRange, insertion, deletion, update, std::ref(barrier), std::ref(map));
    }

    barrier.Wait();

    {
        ScopedTimer timer(true);

        //Wait for the threads to finish
        for (unsigned i = 0; i < thread.size(); i++) 
        {
            thread[i].join();
        }
    }

    map.Uninit();
}

int main(int argc, const char *argv[])
{
    uint32_t setType = 0;
    uint32_t numThread = 1;
    uint32_t testSize = 100;
    uint32_t tranSize = 1;
    uint32_t keyRange = 100;
    uint32_t insertion = 50;
    uint32_t deletion = 50;
    uint32_t update = 0;

    if(argc > 1) setType = atoi(argv[1]);
    if(argc > 2) numThread = atoi(argv[2]);
    if(argc > 3) testSize = atoi(argv[3]);
    if(argc > 4) tranSize = atoi(argv[4]);
    if(argc > 5) keyRange = atoi(argv[5]);
    if(argc > 6) insertion = atoi(argv[6]);
    if(argc > 7) deletion = atoi(argv[7]);
    if(argc > 8) update = atoi(argv[8]);


    assert(setType < 7);
    assert(keyRange < 0xffffffff);

    const char* setName[] = 
    {   "TransList", 
        "RSTMList",
        "BoostingList",
        "TransSkip",
        "BoostingSkip",
        "OSTMSkip",
        "TransMap"
    };

    printf("Start testing %s with %d threads %d iterations %d txnsize %d unique keys %d%% insert %d%% delete %d%% update.\n", setName[setType], numThread, testSize, tranSize, keyRange, insertion, deletion, update);//(insertion + deletion) >= 100 ? 100 - insertion : deletion, update);

    switch(setType)
    {
    case 0:
        { SetAdaptor<TransList> set(testSize, numThread + 1, tranSize); Tester(numThread, testSize, tranSize, keyRange, insertion, deletion, set); }
        break;
    case 1:
        { SetAdaptor<RSTMList> set; Tester(numThread, testSize, tranSize, keyRange, insertion, deletion, set); }
        break;
    case 2:
        { SetAdaptor<BoostingList> set; Tester(numThread, testSize, tranSize, keyRange, insertion, deletion, set); }
        break;
    case 3:
        { SetAdaptor<trans_skip> set(testSize, numThread + 1, tranSize); Tester(numThread, testSize, tranSize, keyRange, insertion, deletion, set); }
        break;
    case 4:
        { SetAdaptor<BoostingSkip> set; Tester(numThread, testSize, tranSize, keyRange, insertion, deletion, set); }
        break;
    case 5:
        { SetAdaptor<stm_skip> set; Tester(numThread, testSize, tranSize, keyRange, insertion, deletion, set); }
        break;
    case 6: //NOTE: the transmap gets constructed with numthread + 1 as the the threadcount
        { MapAdaptor<TransMap> map(testSize, numThread + 1, tranSize); MapTester(numThread, testSize, tranSize, keyRange, insertion, deletion, update, map); }
        break;
    default:
        break;
    }

    return 0;
}
