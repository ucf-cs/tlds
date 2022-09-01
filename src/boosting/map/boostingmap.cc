#include "boosting/map/boostingmap.h"

__thread BoostingMap::LogType* BoostingMap::m_log;

BoostingMap::BoostingMap(int initalPowerOfTwo, int numThreads)
    : m_list(initalPowerOfTwo, numThreads) {}

BoostingMap::~BoostingMap() {
  printf("Total commit %u, abort (total/fake) %u/%u\n", g_count_commit,
         g_count_abort, g_count_fake_abort);

  ASSERT_CODE(
      printf("Total node count %u, Inserts %u, Deletions %u, Finds %u\n",
             g_count, g_count_ins, g_count_del, g_count_fnd);
      // Print();
  );
}

void BoostingMap::Init() {
  m_log = new LogType;
  m_lock.Init();
}

void BoostingMap::Uninit() {
  delete m_log;
  m_lock.Uninit();
}

BoostingMap::ReturnCode BoostingMap::Insert(uint32_t key, uint32_t val,
                                            uint32_t threadId) {
  if (!m_lock.Lock(key)) {
    return LOCK_FAIL;
  }

  // putIfAbsent_first(KEY k, VALUE v, int T){//T is the executing thread's ID
  if (!m_list.putIfAbsent(key, val, threadId)) {
    return OP_FAIL;
  }

  // Since keys are unique we don't need to worry about saving val
  m_log->push_back(Operation(DELETE, key, threadId));

  return OK;
}

BoostingMap::ReturnCode BoostingMap::Delete(uint32_t key, uint32_t threadId) {
  if (!m_lock.Lock(key)) {
    return LOCK_FAIL;
  }

  // before we remove the node, and while its still locked, we save val in case
  // the operation succeeds, but the transaction fails
  int val = m_list.get(key, threadId);

  if (!m_list.remove(key, threadId)) {
    return OP_FAIL;
  }

  m_log->push_back(Operation(INSERT, key, val, threadId));

  return OK;
}

BoostingMap::ReturnCode BoostingMap::Find(uint32_t key, uint32_t threadId) {
  if (!m_lock.Lock(key)) {
    return LOCK_FAIL;
  }

  // get returns (VALUE)NULL if it can't find the key
  if (!m_list.get(key, threadId)) {
    return OP_FAIL;
  }

  // Nothing to undo if a find commits, but the transaction fails, so add
  // nothing to the log
  return OK;
}

BoostingMap::ReturnCode BoostingMap::Update(uint32_t key, uint32_t expected,
                                            uint32_t val, uint32_t threadId) {
  if (!m_lock.Lock(key)) {
    return LOCK_FAIL;
  }

  if (!m_list.putUpdate(key, expected, val, threadId)) {
    return OP_FAIL;
  }

  // To get here the update must have been successful, which means the expected
  // value was there originally, and val is there now. So, "swap" them to get
  // back to the original state, if the transaction fails.
  m_log->push_back(Operation(UPDATE, key, val, expected, threadId));

  return OK;
}

void BoostingMap::OnAbort(ReturnCode ret) {
  if (ret == LOCK_FAIL) {
    __sync_fetch_and_add(&g_count_fake_abort, 1);
  }

  __sync_fetch_and_add(&g_count_abort, 1);

  for (int i = m_log->size() - 1; i >= 0; --i) {
    bool ret = true;

    const Operation& op = m_log->at(i);

    if (op.type == FIND) {
      ASSERT(false, "Revert operation should be Find");
    } else if (op.type == INSERT) {
      ret = m_list.putIfAbsent(op.key, op.val, op.threadId);
    } else if (op.type == DELETE) {
      ret = m_list.remove(op.key, op.threadId);
    } else {
      ret = m_list.putUpdate(op.key, op.expected, op.val, op.threadId);
    }

    ASSERT(ret, "Revert operations have to succeed");
  }

  m_log->clear();
  m_lock.Unlock();
}

void BoostingMap::OnCommit() {
  __sync_fetch_and_add(&g_count_commit, 1);

  m_log->clear();
  m_lock.Unlock();
}

// void BoostingMap::Print()
// {
//     m_list.Print();
// }
