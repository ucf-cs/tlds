FineGrainedLockingList<int> a;
MRLock mrlock;
ResourceAllocator res(mrlock);
vector<int> resources = {2,7,5,8};
lockable = res.CreateLockable(resrouces);

lockable.Lock();
a.Insert(2);
a.Delete(7);
a.Find(8);
a.Insert(5);
lockable.Unlock();