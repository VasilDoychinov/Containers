>>> folder QueueTS: thread-safe fine-grained Queue as follows

// queue_tsfg.hpp: implementation of QueueTSFG_ : thread-safe fine-grained queue
//                 Tool-chain: C++ 17, STL (IDE: Qt with CMake)
//
//    BASICS:
//    - based on a forward one-linked list of nodes: push to 'tail', pop from 'head'
//    - synchronization through: std:: mutex, lock_guard, condition variables
//                      order: mutex_head -> mutex_tail => deadlock avoidance
//    - smart pointers to handle data, assure RAII, exception safety, ...
//    - dummy node used to separate 'head' & 'tail' to improve concurrent access
//    - INVARIANTS:
//         - head == tail :: empty queue
//         -
//    Operations(PUBLIC):
//    - (default) Constructor: empty Queue: might throw as of dummy node creation (new)
//    - push(): add new value (up to resource availability) => might throw (new)
//    - try_pop(): @return - if NOT empty _ptr to Data, {nullptr} otherwise: noexcept ;
//    - wait_and_pop() family: waiting to pop an element ([no time-out])
//                             reason: complete the functionality
//

>>> 


