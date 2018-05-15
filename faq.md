---
layout: tervel_default
title: FAQ
---

What is TLDS?
-------------

TLDS provides a framework for developing transactional containers from lock-free ones.
Lock-free algorithms guarantee that the system makes progress in a finite number of steps, regardless of the system scheduler or actions of other threads.
It includes five examples of transactional data structures, lock-free and obstruction-free versions of a linked list, and a skip list, and a lock-free hash map. We are currently working on supporting transactional data structures for non-linked containers and also transactions that are executed on multiple containers.


Why use TLDS?
-----------------------

TLDS can be used to take advantage of the plethora of lock-free codes that have been hand-crafted for performance, but are incompatible with transactional execution. The result is an efficient lock-free data structure that can execute multiple data structure operations in a single atomic step without sacrificing correctness, while minimizing negative performance impacts.



Do I need lock-freedom?
------------------------------

If your data structure stopped executing at an arbitrary point and never resumed, what's the worst that would happen?


Are there examples?
-------------------

Yes, five transactional data structures are included.


On what platforms does TLDS run?
---------------------------------

TLDS has been tested on Ubuntu 14.04LTS with gcc-4.8.4. There are no known compatibility issues.




How stable is the TLDS feature set?
--------------------

It has been tested, but not exhaustively.



Can I contribute to the TLDS code base?
----------------------------------------

Please see our [contribution guidelines](contributing.html).



How do I contact the team?
--------------------------

You can send messages directly to <pierrelaborde@knights.ucf.edu>.


Where do I report bugs?
-----------------------

Please use the issue tracker on GitHub at <https://github.com/ucf-cs/tlds/issues>.
