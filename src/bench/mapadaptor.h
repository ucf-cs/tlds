#ifndef MAPADAPTOR_H
#define MAPADAPTOR_H

#include "boosting/map/boostingmap.h"
#include "common/allocator.h"
#include "translink/map/transmap.h"
// #include "rstm/map/rstmhash.hpp"

enum MapOpType { MAP_FIND = 0, MAP_INSERT, MAP_DELETE, MAP_UPDATE };

struct MapOperator {
  uint8_t type;
  uint32_t key;
  uint32_t value;
  uint32_t expected;
  uint32_t threadId;
};

enum MapOpStatus { MAP_ACTIVE = 0, MAP_COMMITTED, MAP_ABORTED };

typedef std::vector<MapOperator> MapOpArray;

template <typename T>
class MapAdaptor {};

template <>
class MapAdaptor<TransMap> {
 public:
  MapAdaptor(uint64_t cap, uint64_t threadCount, uint32_t transSize)
      : m_descAllocator(cap * threadCount * TransMap::Desc::SizeOf(transSize),
                        threadCount, TransMap::Desc::SizeOf(transSize))
        //, m_nodeAllocator(cap * threadCount *  sizeof(TransMap::Node) *
        // transSize, threadCount, sizeof(TransMap::Node))
        ,
        m_nodeDescAllocator(
            cap * threadCount * sizeof(TransMap::NodeDesc) * transSize,
            threadCount, sizeof(TransMap::NodeDesc)),
        m_map(/*&m_nodeAllocator,*/ &m_descAllocator, &m_nodeDescAllocator, cap,
              threadCount) {}

  void Init() {
    m_descAllocator.Init();
    // m_nodeAllocator.Init();
    m_nodeDescAllocator.Init();
  }

  void Uninit() {}

  bool ExecuteOps(const MapOpArray& ops,
                  int threadId)  //, std::vector<VALUE> &toR)
  {
// TransMap::Desc* desc = m_map.AllocateDesc(ops.size());
//  TODO: left off here: put a breakpoint here, after just replacing the
//  following with a malloc.
#ifdef USE_MEM_POOL
    TransMap::Desc* desc = m_descAllocator.Alloc();
#else
    TransMap::Desc* desc = (TransMap::Desc*)malloc(
        sizeof(uint8_t) + sizeof(uint8_t) + sizeof(MapOperator) * ops.size());
#endif

    desc->size = ops.size();
    desc->status = TransMap::MAP_ACTIVE;

    for (uint32_t i = 0; i < ops.size(); ++i) {
      desc->ops[i].type = ops[i].type;
      desc->ops[i].key = ops[i].key;
      desc->ops[i].value = ops[i].value;
    }

    return m_map.ExecuteOps(desc, threadId);  //, toR);
  }

 private:
  Allocator<TransMap::Desc> m_descAllocator;
  // Allocator<TransMap::DataNode> m_nodeAllocator;
  Allocator<TransMap::NodeDesc> m_nodeDescAllocator;
  TransMap m_map;
};

template <>
class MapAdaptor<BoostingMap> {
 public:
  MapAdaptor(int initalPowerOfTwo, int numThreads)
      : m_list(initalPowerOfTwo, numThreads) {}

  ~MapAdaptor() {}

  void Init() { m_list.Init(); }

  void Uninit() { m_list.Uninit(); }

  bool ExecuteOps(const MapOpArray& ops /*, int threadId*/) {
    BoostingMap::ReturnCode ret = BoostingMap::OP_FAIL;

    for (uint32_t i = 0; i < ops.size(); ++i) {
      uint32_t key = ops[i].key;
      uint32_t val = ops[i].value;
      uint32_t threadId = ops[i].threadId;
      uint32_t eVal = ops[i].expected;

      if (ops[i].type == FIND) {
        ret = m_list.Find(key, threadId);
      } else if (ops[i].type == INSERT) {
        ret = m_list.Insert(key, val, threadId);
      } else if (ops[i].type == DELETE) {
        ret = m_list.Delete(key, threadId);
      } else {
        ret = m_list.Update(key, eVal, val, threadId);
      }

      if (ret != BoostingMap::OK) {
        m_list.OnAbort(ret);
        break;
      }
    }

    if (ret == BoostingMap::OK) {
      m_list.OnCommit();
    }

    return ret;
  }

 private:
  BoostingMap m_list;
};

// template<>
// class MapAdaptor<RSTMHashTable>
// {
// public:
//     MapAdaptor()
//     {
//         TM_SYS_INIT();
//     }

//     ~MapAdaptor()
//     {
//         TM_SYS_SHUTDOWN();

//         printf("Total commit %u, abort (total/fake) %u/%u\n", g_count_commit,
//         g_count_abort, g_count_stm_abort - g_count_abort);
//     }

//     void Init()
//     {
//         TM_THREAD_INIT();
//     }

//     void Uninit()
//     {
//         TM_THREAD_SHUTDOWN();
//         TM_GET_THREAD();

//         __sync_fetch_and_add(&g_count_stm_abort, tx->num_aborts);
//     }

//     bool ExecuteOps(const SetOpArray& ops) __attribute__ ((optimize (0)))
//     {
//         bool ret = true;

//         TM_BEGIN(atomic)
//         {
//             if(ret == true)
//             {
//                 for(uint32_t i = 0; i < ops.size(); ++i)
//                 {
//                     uint32_t val = ops[i].key;
//                     if(ops[i].type == MAP_FIND)
//                     {
//                         ret = m_hashtable.lookup(val TM_PARAM);
//                     }
//                     else if(ops[i].type == MAP_INSERT)
//                     {
//                         ret = m_hashtable.insert(val TM_PARAM);
//                     }
//                     else
//                     {
//                         ret = m_hashtable.remove(val TM_PARAM);
//                     }

//                     if(ret == false)
//                     {
//                         //stm::restart();
//                         tx->tmabort(tx);
//                         break;
//                     }
//                 }
//             }
//         }
//         TM_END;

//         if(ret)
//         {
//             __sync_fetch_and_add(&g_count_commit, 1);
//         }
//         else
//         {
//             __sync_fetch_and_add(&g_count_abort, 1);
//         }

//         return ret;
//     }

// private:
//     RSTMHashTable m_hashtable;

//     uint32_t g_count_commit = 0;
//     uint32_t g_count_abort = 0;
//     uint32_t g_count_stm_abort = 0;
// };

#endif /* end of include guard: MAPADAPTOR_H */