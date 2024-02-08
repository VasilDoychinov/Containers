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


#ifndef QUEUE_TSFG_HPP
#define QUEUE_TSFG_HPP

#include <mutex>
#include <condition_variable>

#include <memory>
#include <utility>


template <typename T>
class QueueTSFG_ {

  using OptUniqueLock_t = std::optional<std::unique_lock<std::mutex>> ;
  using UniqueLock_t = std::unique_lock<std::mutex> ;

  private:

    struct sNode_ {
       std::unique_ptr<T>      _data ;                                    // ??? shared_ptr
       std::unique_ptr<sNode_> _next{} ;                                  // invoke RAII for Next
       explicit sNode_() : _data{}, _next{} {}
       // sNode_(T data) : _data{std::make_unique<T>(std::move(data))}, _next{} {}
       // all specials  = defaults
    }; // struct sNode_


  private:

    mutable std::mutex        _mx_head{} ;                                // Head ...
    std::unique_ptr<sNode_>   _head{} ;
    mutable std::mutex        _mx_tail{} ;                                // Tail ...
    sNode_*                   _tail{} ;

    std::condition_variable   _cv_empty ;                                 // stays on for Queue empty

  private:

    sNode_* _get_tail() const&
            { std::lock_guard<std::mutex> lu{_mx_tail} ; return _tail ; }// @return the value of Tail

    // previous ver: std::unique_ptr<sNode_> _pop_head() &  ;            // extract & return the Head
    std::unique_ptr<sNode_> _pop_protected_head() &                      // extract & return a PROTECTED Head
                   {   std::unique_ptr<sNode_> curr_head = std::move(_head) ;
                       _head = std::move(curr_head->_next) ;
                       return curr_head ;
                   }

    std::unique_lock<std::mutex> _wait_for_data() {                      // no time-out
                 std::unique_lock<std::mutex>  lu{_mx_head} ;            // [&] C++17: this incl.
                 _cv_empty.wait(lu, [&]()->bool{ return _head.get() != _get_tail() ; });
                 return lu ;    // do not std::move()
              }

    std::optional<std::unique_lock<std::mutex>> _check_for_data() {      // no time-out
                 std::unique_lock<std::mutex>  lu{_mx_head} ;
                 return _head.get() != _get_tail()
                        ? OptUniqueLock_t{std::move(lu)}
                        : OptUniqueLock_t{} ;
                 // return lu ;    // do not std::move()
              }

  public:

    explicit QueueTSFG_() : _head{new sNode_}, _tail{_head.get()} {}
    // no other Construction allowed
    QueueTSFG_(const QueueTSFG_& q)             = delete ;
    QueueTSFG_& operator= (const QueueTSFG_& q) = delete ;

                                  // public APIs
    bool empty() const& ;
    bool push(T value) & ;                                               // always true, here

    std::unique_ptr<T> try_pop() & ;                                     // attempt a pop (no time-out)

    std::unique_ptr<T> wait_and_pop() & ;                                // untill data appears [no time-out]
    void           wait_and_pop(T& v) & { v = *(this->wait_and_pop()) ; }// ...

    // ??? friend std::ostream& operator<< (std::ostream& os, const void* data) ;
}; // class QueueTSFG_

template <class T> using Node_type = typename QueueTSFG_<T>::sNode_ ;
template <class T> using ptrNode_type = typename std::unique_ptr<Node_type<T>> ;

                                  // QueueTSFG_<T>:: public APIs

template <typename T> inline bool
QueueTSFG_<T>::empty() const &
{
   std::lock_guard<std::mutex>   lu{_mx_head} ;                          // _ptr to 1st Node
   return _head.get() == this->_get_tail() ;                             // :: empty queue
}

template <typename T> bool
QueueTSFG_<T>::push(T value) &                                           // to 'tail'
{
   decltype(auto)   new_data = std::make_unique<T>(std::move(value)) ;   // ??? shared_ptr
   std::unique_ptr<sNode_>   new_dummy_node{new sNode_{}} ;

   {  // all resources acquired -> lock 'tail' and push to it (points to the Dummy node)
      std::lock_guard<std::mutex>  lu{_mx_tail} ;
      _tail->_data = std::move(new_data) ;                               // into the dummy node::_data
      _tail->_next = std::move(new_dummy_node) ;
      _tail = _tail->_next.get() ;                                       // raw ptr to the new Dummy
   }  // eo Lock: it's been released

   this->_cv_empty.notify_one() ;                                        // for not empty anymore
   return true ; // always for this DS, this mechanics
}

/* previous version (_pop_head()):
template<typename T> inline std::unique_ptr<T>                           // pop from 'head'
QueueTSFG_<T>::try_pop() &                                               // @return - the result
{
   // _check_for_data() ?????????? instead of _pop_hed()
   decltype(auto)   h = this->_pop_head() ;                              // extract the Head if not empty
   return h ? std::move(h->_data) : nullptr ;
} */

template<typename T> inline std::unique_ptr<T>                           // pop from 'head'
QueueTSFG_<T>::try_pop() &                                               // @return - the result
{
   std::unique_ptr<Node_type<T>>  curr_head{} ;
   {  // protected (by *result for _head) section
      decltype(auto)   result = this->_check_for_data() ;                              // extract the Head if not empty
      if (!result)     return nullptr ;
      // and, there is data: (_head still protected
      curr_head = _pop_protected_head() ;
   } // eo Lock
   return std::move(curr_head->_data) ;
}

template<typename T> inline std::unique_ptr<T>                           // wait until data appears, then pop
QueueTSFG_<T>::wait_and_pop() &                                          // @return access to data popped
{
   std::unique_ptr<Node_type<T>>  curr_head{} ;
   {  // lock block, wait for data
      std::unique_lock<std::mutex>   lu{_wait_for_data()} ;       // lock's been on and still is
      // and, there is data ready
      // previous version: curr_head = std::move(_head), _head = std::move(curr_head->_next) ;
      curr_head = _pop_protected_head() ;
   } // eo Lock

   return std::move(curr_head->_data) ;
}

                                  // QueueTSFG_<T>:: private operations
/* previous version (_pop_head()):
template<typename T> std::unique_ptr<Node_type<T>>                       // extract & return 'head'
QueueTSFG_<T>::_pop_head() &                                             // @return - if data present
{
   std::unique_ptr<Node_type<T>>   curr_head = nullptr ;
   {  // hierarchical lock needed to check for empty queue
      std::lock_guard<std::mutex>   lu{_mx_head} ;                        // _ptr to 1st Node
      if (_head.get() == this->_get_tail())   return std::move(curr_head);// :: empty queue

      // only head is still locked now
      curr_head = std::move(_head) ;
      _head = std::move(curr_head->_next) ;
   } // eo Lock _head

   return curr_head ;
} */

#endif // QUEUE_TSFG_HPP
