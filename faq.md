---
layout: tervel_default
title: FAQ
---

What is tlds?
-------------

tlds provides a framework for developing transactional containers from lock-free ones.
Lock-free algorithms guarantee that the system makes progress in a finite number of steps, regardless of the system scheduler or actions of other threads.


Why use tlds?
-----------------------

tlds can be used to take advantage of the plethora of lock-free codes that have been hand-crafted for performance, but are incompatible with transactional execution. The result is an efficient lock-free data structure that can execute multiple data structure operations in a single atomic step without sacrificing correctness, while minimizing negative performance impacts.



Do I need lock-freedom?
------------------------------

If your data structure stopped executing at an arbitrary point and never resumed, what's the worst that would happen?


Are there examples?
-------------------

Yes, two transactional data structures are included, a lock-free linked list and a lock-free skip list.


On what platforms does tlds run?
---------------------------------

tlds has been tested on Ubuntu 14.04LTS with gcc-4.8.4. There are no known compatibility issues.




How stable is the tlds feature set?
--------------------

It has been tested, but not exhaustively.



Can I contribute to the tlds code base?
----------------------------------------

Please see our [contribution guidelines](contributing.html).



How do I contact the team?
--------------------------

You can send messages directly to <pierrelaborde@knights.ucf.edu>.


Where do I report bugs?
-----------------------

Please use the issue tracker on GitHub at <https://github.com/ucf-cs/tlds/issues>.
