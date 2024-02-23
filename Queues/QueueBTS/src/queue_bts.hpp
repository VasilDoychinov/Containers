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

    mutable std::mutex _mx_all ;

  private:
                                  // @return the value of Tail
    _TailPair _get_tail() const& { Lock_guard lu{_mx_tail} ; return {_size, _tail} ; }
                                  // move a Handle: !!! use it appropriately:: protection & all invariants considered
    void _handle_advance(_Handle& h) { ++h ; if (h == _cont.end()) h = _cont.begin() ; }
                                  // the same but for an Index: used with, like, for_each()
    size_t _index_advance(size_t i) const&  { return ++i == _capacity ? 0 : i ; }

/* wait_...
    std::unique_lock<std::mutex> _wait_for_data() {                      // no time-out
                 std::unique_lock<std::mutex>  lu{_mx_head} ;            // [&] C++17: this incl.
                 _cv_empty.wait(lu, [&]()->bool { return 0 != this->size() ; } );
                 return lu ;
              }

    std::unique_lock<std::mutex> _wait_for_capacity() {                  // no time-out
                 std::unique_lock<std::mutex>  lu{_mx_tail} ;            // [&] C++17: this incl.
                 _cv_full.wait(lu, [&]()->bool { return  _size < _capacity ; } );
                 return lu ;
              }**/

    OptHeadLock_ _check_for_data() { UniqueLock_t  lu{_mx_head} ;       // if data: _head + 1 < _tail
                   auto [ws, wh] = _get_tail() ;                              // [_size,_tail]
                   return ws > 0 ? OptHeadLock_{std::pair{std::move(lu), ws}} // [lock,_size @ _head]
                                 : OptHeadLock_{} ;
              }
    OptTailLock_ _check_for_capa() { UniqueLock_t  lu{_mx_tail} ;        // if capacity: invariants: - _size < _capaciry
                   return _size < _capacity ? OptTailLock_{std::move(lu)}
                                            : OptTailLock_{} ;
                }

    void _pop_protected_head(size_t rs) & { size_t ns = rs - 1 ;         // 'rs': size related to current _head
             _handle_advance(_head) ;
             while(!_size.compare_exchange_strong(rs,ns)) ns = rs - 1 ;  // as _size is not protected: --_size;
         }
    void _push_protected_tail() & { _handle_advance(_tail), ++_size ; }   //


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
    bool empty() const& { std::lock_guard<std::mutex> lt{_mx_tail} ; return _size == 0 ; }
    operator bool () const& { return _isOK ; }                           // if valid: ??? protections

                                  // functionality: push/pop
    bool try_push(T value) & ;                                           // @return - if successful
    bool try_pop(T& value) & ;                                           // attempt a pop (no time-out)
    T    try_pop() & { T v{} ; this->try_pop(v) ; return v ; }           // ..., @return - popped value or T{}

    bool push_list(std::vector<T>&& inil) ;                              // pushes until all in: for testing, mostly: might BLOCK
    std::vector<T> pop_list(size_t count) ;                              // pop 'count' elements into Container

    bool wait_and_pop(T& v) & ;                                          // untill data appears [no time-out]
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
   // std::lock_guard<std::mutex> lg{_mx_all} ;

   {  // T is requested as swappable -> move new element into *_tail
      decltype(auto)   result = this->_check_for_capa() ;                // protection on _tail: active
      if (!result)     return false ;                                    // empty Queue

      *_tail = (value), _push_protected_tail() ;                // into the dummy node::_data
      // Log_to(0, ": PUSHED: ", value) ;
   }  // eo Lock: it's been released

   _cv_empty.notify_all() ;                                              // for not empty anymore
   return true ;
}

/* wait_to_push()
template<typename T> inline bool QueueBTS_<T>                           // wait until vacancy, then push
::wait_to_push(T value) &                                               // @return - value: memory allocated somewhere else
{
   static size_t  current_value = 0 ;

   {  // lock block, wait for data
      std::unique_lock<std::mutex>   lu{_wait_for_capacity()} ;         // lock's still on
      // and, there is vacancy
      if (value != current_value++)  Log_to(0, "> pushing ", value, " at current ", current_value - 1) ;
      *_tail = std::move(value), _push_protected_tail() ;
   } // eo Lock

   this->_cv_empty.notify_one() ;
   return true ;                                                        // bool is or eventual Time-out
}
*/

#include <thread>

template <typename T> bool QueueBTS_<T>
::push_list(std::vector<T>&& inil)                                       // ??? Time-out
{
   size_t   count_yields = 0 ;

   std::vector<T>  work = std::move(inil) ;
                                                                         // Log_to(0, "\n> push_list() of ", work.size(), " elements") ;
   // for (auto& el : work) {
   size_t   sw = work.size() ; T value{} ;
   for (size_t i = 0 ; i < sw ; )  {
      // this->wait_to_push(std::move(el)) ;                                // might BLOCK
      if (try_push(work[i])) { value = work[i++] ; }
      else { std::this_thread::yield() ; // if (--count == 0)   break ;
         ++count_yields ;
      }
   }

   Log_to(0,"\n> from #", std::this_thread::get_id(), " last value PUSHED: ", value, ", #yields: ", count_yields) ;
   return true ;
}


template<typename T> inline bool QueueBTS_<T>                            // pop from 'head'
::try_pop(T& value) &                                                    // @return - the result
{
   // std::lock_guard<std::mutex> lg{_mx_all} ;

   {  // protected (by *result for _head) section(initiated in _check_for_data()
      decltype(auto)   result = this->_check_for_data() ;                // protection on _head: active
      if (!result)     return false ;                                    // empty Queue

      value = std::move(*_head), _pop_protected_head(result->second) ;   // _head + 1 = _tail
      // Log_to(0, ": POPPED value: ", value) ;
   } // eo Lock

   // this->_cv_full.notify_one() ;
   return true ;
}

/* wait_and_pop()
template<typename T> inline bool QueueBTS_<T>                            // wait until data appears, then pop
::wait_and_pop(T& value) &                                               // @return - value: memory allocated somewhere else
{
   static size_t  current_value = 0 ;

   {  // lock block, wait for data
      std::unique_lock<std::mutex>   lu{_wait_for_data()} ;              // lock's still on
      // and, there is data ready
      value = std::move(*_head) ;
      if (value != current_value++)  {
         Log_to(0, "> poping ", value, " at current ", current_value - 1,
                   " :: head at ", _head - _cont.begin(), " size: ", _size) ;
      }
      _pop_protected_head() ;
   } // eo Lock

   this->_cv_full.notify_one() ;
   return true ;                                                         // bool is or eventual Time-out
}
*/

template <typename T> std::vector<T> QueueBTS_<T>                        // pop 'count' elements into Container
::pop_list(size_t count)
{
   T value{} ;
   size_t   count_yields = 0 ;
   std::vector<T>   work{} ; work.resize(count, T{}) ;                       //
   for (size_t i = 0 ; i < count ; )    {
      // wait_and_pop(value), work.emplace_back(std::move(value)) ;
      if (try_pop(value))   { work[i++] = std::move(value) ; }
      else { std::this_thread::yield() ;
         ++count_yields ;
      }
   }

   Log_to(0,"\n> from #", std::this_thread::get_id(), " last value POPPED: ", value, ", #yields: ", count_yields) ;
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
