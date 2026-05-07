moodycamel::ConcurrentQueue — vendored single-header MPMC queue
================================================================

The multi-threaded render engine (Source/Engine/VibeThreadPool.cpp) requires
this header to compile.

How to drop it in
-----------------
1. Download the single-header library from:
       https://github.com/cameron314/concurrentqueue
   The only file you need is `concurrentqueue.h` from the repo root.

2. Place it here:
       libs/concurrentqueue/concurrentqueue.h

3. (Optional) For blocking-queue support, also drop `blockingconcurrentqueue.h`
   into this folder. Currently unused by VibeThreadPool but harmless to have.

4. Build via do_build.bat as usual — CMake adds this folder to the include
   path automatically (see top-level CMakeLists.txt).

License
-------
Simplified BSD (or Boost Software License at user's choice). Both compatible
with this project. License text is at the top of concurrentqueue.h.

Why this queue
--------------
- Lock-free MPMC (multi-producer multi-consumer)
- Wait-free for producers
- Header-only, no link-time dependencies
- Battle-tested in production audio engines
- ~5000 lines, single TU
