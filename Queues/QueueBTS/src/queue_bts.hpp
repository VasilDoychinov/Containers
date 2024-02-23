// queue_bts.hpp: implementation of QueueBTS_ : bounded thread-safe fine-grained queue( & ROUND)
//                 Tool-chain: C++ 17, STL (IDE: Qt with CMake)
//
//    BASICS:
//    - no memory allocations within an Instance except for Container(buffer) creation
//    - ::value_type requirements: SWAPPABLE (??? move-only) - a lot is around this
//    - based on std::vector<> for its simplicity , swappability
//    - synchronization through: std:: mutex, lock_guard, condition variables
//                      order: mutex_head -> mutex_tail => deadlock avoidance
//    - 1st step: Bound Queue only; ROUND-queue: push_to (head), pop_from(tail)
//    - smart pointers to handle data, assure RAII, exception safety, ...
//    - dummy node used to separate 'head' & 'tail' in order to improve concurrent access
//    - INVARIANTS (& assertions):
//         - _size == 0 :: empty queue
//         - _size == _capacity: full queue
//         - requested capacity: >= 2
//         - checks for data: through _head, _tail comparison; after that _size will not be protected
//                            => do not change _size (??? for now)
//
//    OPERATIONS(public):
//    - (default) Constructor: empty Queue of the specified Capacity: might throw from std::vector<>.resize(capacity)
//      NOT thread-safe
//    - try_push(): try to push() into (up to resource availability) => noexcept unless move assignment of T throws
//    ?- try_pop(): @return - if NOT empty _ptr to Data, {nullptr} otherwise: noexcept ;
//    ?- wait_and_pop() family: waiting to pop an element ([no time-out])
//                             reason: complete the functionality
//    - wait_to_pop() family: wait on a condition_variable until Queue is !empty
//
//    - NB: no time-out option privided. To support that change wait_...() with a parameter that will be
//          passed onto _wait_...() and further use condition_variable.wait_for() instead of .wait()
//          Further, on return consider the result.
//


#ifndef QUEUE_BTS_HPP
#define QUEUE_BTS_HPP

#include <vector>

#include <atomic>
#include <mutex>
#include <condition_variable>

#include <memory>
#include <utility>
#include <optional>

#include <assert.h>
#include <iostream>

template <typename T = int>
class QueueBTS_ {  static_assert(std::is_swappable_v<T>, "> QueueBTS_<> requires: SWAPPABLE types") ;

  using _Container = std::vector<T> ;                                     // options: array, dequeue,
  using _Handle = typename _Container::iterator ;                         // to an element: at leas std::forward_iterator
  using _TailPair = std::pair<size_t, _Handle> ;                             // size & the Related _tail

  using Lock_guard = std::lock_guard<std::mutex> ;
  using UniqueLock_t = std::unique_lock<std::mutex> ;
  using OptTailLock_ = std::optional<UniqueLock_t> ;                      // Tail lock protects _size as well
  using OptHeadLock_ = std::optional<std::pair<UniqueLock_t, size_t>> ;   // lock & the related _size

  private:

    _Container            _cont{} ;                                      // as a storage Buffer: [_start, _end)
    std::atomic<size_t>   _size{} ;                                      // # of stored elements
    const size_t          _capacity{} ;                                  // const: READING access only

    _Handle         _head{} ;                                            // push() here
    _Handle         _tail{} ;                                            // pop()

    mutable std::mutex _mx_head ;                                        // _head protection: _head & Empty()
    mutable std::mutex _mx_tail ;                                        // _tail ...: _tail & _size
    std::condition_variable _cv_empty ;                                  // stay on if Queue empty
    std::condition_variable _cv_full ;                                   // ...              full

    bool            _isOK{false} ;

  private:
                                  // @return the value of Tail
    _TailPair _get_tail() const& { Lock_guard lu{_mx_tail} ; return {_size, _tail} ; }
                                  // move a Handle: !!! use it appropriately:: protection & all invariants considered
    void _handle_advance(_Handle& h) { ++h ; if (h == _cont.end()) h = _cont.begin() ; }
                                  // the same but for an Index: used with, like, for_each()
    size_t _index_advance(size_t i) const&  { return ++i == _capacity ? 0 : i ; }

                                  // check_...() and return the result
    OptHeadLock_ _check_for_data() { UniqueLock_t   lu{_mx_head} ;       // if data: _head + 1 < _tail
                   auto [ws, wh] = _get_tail() ;                              // [_size,_tail] > 0
                   return ws > 0 ? OptHeadLock_{std::pair{std::move(lu), ws}} // [lock,_size @ _head]
                                 : OptHeadLock_{} ;
                 }
    OptTailLock_ _check_for_capa() { UniqueLock_t   lu{_mx_tail} ;       // if capacity: invariants: - _size < _capaciry
                   return _size < _capacity ? OptTailLock_{std::move(lu)}
                                            : OptTailLock_{} ;
                 }
                                  // wait_...()
    OptHeadLock_ _wait_for_data() { UniqueLock_t   lu{_mx_head} ;        // ??? no time-out; [&] C++17: this incl.
                   size_t wsize ;
                   _cv_empty.wait(lu, [&]() { auto [ws,wt]= _get_tail() ; return (wsize = ws) > 0 ; });
                   // _cv_empty.wait(lu, [&]()->bool { return 0 != this->size() ; } );
                   return OptHeadLock_{std::pair{std::move(lu), wsize}} ;
                 }
    OptTailLock_ _wait_for_capa() { UniqueLock_t lu{_mx_tail} ;          // ??? no time-out; [&] C++17: this incl.
                   _cv_full.wait(lu, [&]() { return  _size < _capacity ; });
                   return OptTailLock_{std::move(lu)} ;
                 }

    void _pop_protected_head(size_t rs) & { size_t ns = rs - 1 ;         // 'rs': size related to current _head
             _handle_advance(_head) ;
             while(!_size.compare_exchange_strong(rs,ns)) ns = rs - 1 ;  // as _size is not protected: --_size;
         }
    void _push_protected_tail() & { _handle_advance(_tail), ++_size ; }  //


  public:
    using value_type = typename _Container::value_type ;

  public:
                                  // constructors
    explicit QueueBTS_(size_t cap) ;                                    // no protection
    // no other Construction allowed
    QueueBTS_(const QueueBTS_& q)             = delete ;
    QueueBTS_& operator= (const QueueBTS_& q) = delete ;

                                  // public APIs: descriptive
    size_t size() const& { std::lock_guard<std::mutex> lt{_mx_tail}; return _size ; }
    bool empty() const&  { return this->size() == 0 ; }
    operator bool () const& { return _isOK ; }                           // if valid: ??? protections

                                  // functionality: push/pop
    bool try_push(T value) & ;                                           // @return - if successful
    bool try_pop(T& value) & ;                                           // attempt a pop (no time-out)
    T    try_pop() & { T v{} ; this->try_pop(v) ; return v ; }           // ..., @return - popped value or T{}

    bool push_list(std::vector<T>&& inil) ;                              // pushes until all in: for testing, mostly: might BLOCK
    std::vector<T> pop_list(size_t count) ;                              // pop 'count' elements into Container

    bool wait_and_pop(T& v) & ;                                          // untill data appears: @return - always true or, BLOCK
    T    wait_and_pop() & { T v{}; this->wait_and_pop(v) ; return v ; }  // ...
    bool wait_to_push(T value) & ;                                       // @return - if successful

    template <class DTy>
       friend std::ostream& operator<< (std::ostream& os, const QueueBTS_<DTy>& data) ;
}; // class QueueTSFG_


template <typename T> QueueBTS_<T>
::QueueBTS_(size_t cap) : _cont{}, _size{0}, _capacity{cap}
                        , _head{}, _tail{}
                        , _mx_head{}, _mx_tail{}
                        , _isOK{false}
{
   assert(cap >= 2) ;
   try {
      _cont.resize(cap, T{}) ; // to test: throw std::runtime_error("simple test") ;
      _head = _cont.begin(), _tail = _head ; // + 1 ;                         // empty queue
      _isOK = true ;
   } catch (...) { /* _isOK is false ??? simple for now */ }
}

                                  // QueueBTS_<T>:: public APIs
template <typename T> bool QueueBTS_<T>
::try_push(T value) &                                                    // to 'tail'
{
   {  // T is requested as swappable -> move new element into *_tail
      decltype(auto)   result = this->_check_for_capa() ;                // protection on _tail: active
      if (!result)     return false ;                                    // empty Queue

      *_tail = std::move(value), _push_protected_tail() ;
   }  // eo Lock: it's been released

   _cv_empty.notify_one() ;                                              // for not empty anymore
   return true ;
}

/* wait_to_push() */
template<typename T> inline bool QueueBTS_<T>                            // wait until vacancy, then push
::wait_to_push(T value) &                                                // @return - alway true or, BLOCK
{
   {  // lock block, wait for vacancy
      decltype(auto)   result = this->_wait_for_capa() ;                 // lock's still on
      // if (!result)     return false ;                                 here: waited for vacancy (& NO time-out)
      *_tail = std::move(value), _push_protected_tail() ;
   } // eo Lock

   this->_cv_empty.notify_one() ;
   return true ;
}


#include <thread>
static inline std::thread::id get_tid() { return std::this_thread::get_id() ; }

template <typename T> bool QueueBTS_<T>
::push_list(std::vector<T>&& inil)                                       // ??? Time-out
{
   // used with try_push(): size_t   count_yields = 0 ;
   std::vector<T>  work = std::move(inil) ;
                                                                         Log_to(0, "\n> push_list() of ", work.size(), " elements") ;
   // for (auto& el : work) {
   size_t   sw = work.size() ;                                           // for Testing with try_push(): T value{} ;
   for (size_t i = 0 ; i < sw ; )  {
      this->wait_to_push(std::move(work[i++])) ;                         // replace the block below: might BLOCK
      /* if (try_push(work[i])) { value = work[i++] ; }
      else { std::this_thread::yield() ;
         ++count_yields ;
      } uses try_push() instead of wait_to_push() */
   }

   // Log_to(0,"> from #", std::this_thread::get_id(), " last value PUSHED: ", value, ", #yields: ", count_yields) ;
   Log_to(0,"> from #", get_tid(), ": end of push_list() for: ", work[0]) ;
   return true ;
}


template<typename T> inline bool QueueBTS_<T>                            // pop from 'head'
::try_pop(T& value) &                                                    // @return - the result
{
   {  // protected (by *result for _head) section(initiated in _check_for_data()
      decltype(auto)   result = this->_check_for_data() ;                // auto[lock,_size]
      if (!result)     return false ;                                    // empty Queue

      value = std::move(*_head), _pop_protected_head(result->second) ;   // _head + 1 = _tail
   } // eo Lock

   this->_cv_full.notify_one() ;
   return true ;
}

// wait_and_pop(): NO time-out
template<typename T> inline bool QueueBTS_<T>                            // wait until data appears, then pop
::wait_and_pop(T& value) &                                               // @return - always true or, BLOCK
{
   {  // lock block, wait for data
      decltype(auto)   result = this->_wait_for_data() ;                 // auto [lock,_size]
      // if (!result)     return false ;                                 here: waited for vacancy (& NO time-out)
      value = std::move(*_head), _pop_protected_head(result->second) ;
   } // eo Lock

   this->_cv_full.notify_one() ;
   return true ;                                                         // bool is or eventual Time-out
}

template <typename T> std::vector<T> QueueBTS_<T>                        // pop 'count' elements into Container
::pop_list(size_t count)
{
   /* used with try_pop(): size_t   count_yields = 0 ;
   ** T value{} ; */
   std::vector<T>   work{} ; work.resize(count, T{}) ;                   //

   for (size_t i = 0 ; i < count ; )    {
      wait_and_pop(work[i++]) ;   // , work.emplace_back(std::move(value)) ;
      /* if (try_pop(value))   { work[i++] = std::move(value) ; }
      else { std::this_thread::yield() ;
         ++count_yields ;
      } */
   }

   // Log_to(0,"> from #", std::this_thread::get_id(), " last value POPPED: ", value, ", #yields: ", count_yields) ;
   Log_to(0,"> from #", get_tid(), ": end of pop_list(): ", count > 0 ? work[count - 1] : T{}) ;
   return work ;
}

                                  // QueueTSFG_<T>:: private operations

                                  // external functions
template <typename T>
std::ostream& operator<< (std::ostream& os, const QueueBTS_<T>& q)
{
   os << "qBTS{capacity:" << q._capacity << ", size:" << q._size
      << "}: " << ((bool)q ? "OK" : "ERROR") ;
   os << " >> head at: " << q._head - q._cont.begin()
      << "; tail at: " << q._tail - q._cont.begin() ;
   if (q._size > 0) {
      os << " >>" ; // "\n>" ;
      auto ind = q._index_advance(q._head - q._cont.begin()) ;

      for (size_t i = 0 ; i < q._size ; ++i, ind = q._index_advance(ind))  {
         os << ":" << *(q._cont.cbegin() + ind) ;
      }
      os << '<' ;
   } else   os << " ><" ;
   return os ;
}

#endif // QUEUE_BTS_HPP
