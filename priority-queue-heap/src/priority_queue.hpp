// priority_queue.hpp: types, function prorotypes, etc for a Priority Queue adaptor
//   REQUIRES: C++17
//
//   - based on: std::vector<>
//   - public interface (all noexcept):
//     . constructors: incl. from std::initializer_list<>
//     . assignments
//     . pop(): @return std::optional<>
//     . pop(DTy& value): @return - void, output in value (if any)
//     . push(): @return - if successful
//     . peek(): the next element to be poped
//     . characteristics:
//       - size(), capacity(), height()
//   - external:
//     . operator<<() - friend
//     . showPQ(): if && pased will be moved-from !
//     . collection_from_PQ(): ... ! @return - the template parameter collection
//   - organized as compareHeap where compare is a template parameter like [less/greater] and access to elements is
//     through indexes (size_t):
//   - when capacity's been exausted - double it to ensure 'amortized O(1)' NOT depending on std::vector<> to resize()
//


#ifndef PRIORITY_QUEUE_HPP
#define PRIORITY_QUEUE_HPP

#include <functional>                                          // for std::less<>
#include <vector>
#include <mutex>
#include <utility>

#include <type_traits>
#include <assert.h>

#include "Logger_decl.hpp"
#include "Logger_helpers.hpp"

inline bool is_pow2(size_t n) { return n > 0 && (!(n & (n - 1))) ; }

template<class T> struct Remove_cvr {                                         // not in TTraits until C++20
    typedef std::remove_cv_t<std::remove_reference_t<T>> type;
};
template <typename T> using Remove_cvr_t = typename Remove_cvr<T>::type ;


template <typename DTy, typename Compare = std::less<DTy>, bool Protected = false,
          typename Allocator = std::allocator<DTy>
          // ??? typename Container = std::vector<DTy, Allocator>
         >
class PQueue_   {                                                             // internal index in [1, ...] = size()
    static_assert(std::is_move_constructible_v<DTy> && std::is_move_assignable_v<DTy>, "> PQueue_ elements NOT swappable") ;

    using Container = std::vector<DTy, Allocator> ;                           // the underlying container
    using value_type = typename Container::value_type ;

    using PQ_ind = size_t ;                                                   // index to access an element

  private:                                                                    // static methodss
    static PQ_ind _ind_p(PQ_ind ind) { return ind / 2 ; }
    static PQ_ind _ind_lc(PQ_ind ind) { return ind * 2 ; }
    static PQ_ind _ind_rc(PQ_ind ind) { return ind * 2 + 1 ; }
    static PQ_ind _ind_next() { return 1 ; }

  private:

    // secure the capacity required is available
    void   _reserve() { auto cont_cap = _cont.capacity() ; if (cont_cap == _cont.size())  _cont.reserve(cont_cap * 2) ; }
    // if an index is valid
    bool   _is_valid(PQ_ind ind) const& noexcept { return ind > 0 && ind <= this->size() ; }
    auto   _level(PQ_ind ind) const& noexcept { size_t l = 0 ; for (size_t nh = 1 ; nh <= ind ; nh <<= 1, ++l) ; return l ; }
    bool   _has_parent(PQ_ind ind) const& noexcept { return _is_valid(ind) && _is_valid(_ind_p(ind)) ; }
    bool   _has_left_child(PQ_ind ind) const& noexcept { return _is_valid(ind) && _is_valid(_ind_lc(ind)) ; }
    bool   _has_right_child(PQ_ind ind) const& noexcept { return _is_valid(ind) && _is_valid(_ind_rc(ind)) ; }


    // the parent & children of an element (at ind)
    value_type&  _parent(PQ_ind ind) & noexcept { assert(_has_parent(ind)) ; return _access(_ind_p(ind)) ; }
    value_type&  _left_child(PQ_ind ind) & noexcept { assert(_has_left_child(ind)) ; return _cont[_ind_lc(ind)] ; }
    value_type&  _right_child(PQ_ind ind) & noexcept { assert(_has_right_child(ind)) ; return _cont[_ind_rc(ind)] ; }
    value_type&  _next_to_go() & noexcept { return _cont[_ind_next()] ; }     // if OK, _cont.size() >= 2
    PQ_ind       _comp_child(PQ_ind ind) & noexcept {                         // might not have a right child
                    assert(_has_left_child(ind)) ;
                    if (!_has_right_child(ind))   return _ind_lc(ind) ;
                    return this->_comp(_left_child(ind), _right_child(ind)) ? _ind_lc(ind) : _ind_rc(ind) ;
                 }

      // the element at ind
    value_type&  _access(PQ_ind ind) & noexcept { assert(_is_valid(ind)) ; return _cont[ind] ; }

    void         _heapifyUP(PQ_ind ind) & ;                                   // restore the Heap starting at ind
    void         _heapifyDOWN(PQ_ind ind) & ;                                 // restore the Heap starting at ind

    DTy          _popPQ() & noexcept ;                                        // extract next-to-go, if any

  public:

    // constructors
       // default
    explicit PQueue_() noexcept : _cont{}, _mut{}, _isOK{false}, _comp{} {    // create a padding element in position 0
          try { _cont.reserve(2), _cont.push_back(DTy{}), _isOK = true ; }    // the capacity is 2 -> 4 -> 8 ... pow(2, n)
          catch (...) { Log_to(0, "> PQueue(): exception caught") ; }
          // Log_to(0, ": PQueue(): initial container size: ", _cont.size()) ;
    }
       // move & copy
    PQueue_(PQueue_&& coll) noexcept
           : _cont{std::move(coll._cont)}, _mut{}, _isOK{coll._isOK}, _comp{std::move(coll._comp)} { coll = PQueue_{} ; }
    PQueue_(const PQueue_& coll) noexcept : _cont{coll._cont}, _mut{}, _isOK{coll._isOK}, _comp{coll._comp} {}
       // from std::initializer_list<>
    PQueue_(std::initializer_list<DTy> ini_list) noexcept ;

    // assignment
    PQueue_& operator= (PQueue_&& coll) {
                       _cont = std::move(coll._cont), _isOK = coll._isOK, _comp = std::move(coll._comp) ;
                       PQueue_ w ; coll = w ;
                       return *this ;
             }
    PQueue_& operator= (const PQueue_& coll) {
                       _cont = coll._cont, _isOK = coll._isOK, _comp = coll._comp ;
                       return *this ;
             }

  public:

    operator bool() const& noexcept { return _isOK ; }                                 // if OK

    // operations & access
    template <typename T> bool push(T&& value) noexcept ;   // ??? might add up some requirements for noexcept on T
    std::optional<DTy>         pop() & noexcept ;
    const value_type&          peek() const& noexcept { return _cont.size() > 1 ? _cont[1] : _cont[0] ; }
    void                       pop(DTy& value) & noexcept ;                   // pop next into 'value'

    // descriptors
    size_t   size() const& noexcept { auto s = _cont.size() ; return s > 0 ? s - 1 : 0 ; }
    size_t   capacity() const& noexcept { auto c = _cont.capacity() ; return c > 1 ? c - 1 : 0 ; }
    size_t   height() const& noexcept { return this->_level(this->size()) ; }

    template <class T, class C, bool P, class A>
    friend std::ostream& operator<< (std::ostream& os, const PQueue_<T, C, P, A>& pq) ;

  private:

    Container    _cont{} ;                                                    // the underlying container
    std::mutex   _mut{} ;                                                     // for protection

    bool         _isOK{false} ;                                               // no-throwing concept: keep the status

    Compare      _comp{} ;                                                    // comparing functor
}; // class PQueue_


// class PQueue_

// constructors

template <typename DTy, typename Compare, bool Protected, typename Allocator>
PQueue_<DTy, Compare, Protected, Allocator>
::PQueue_(std::initializer_list<DTy> ini_list) noexcept
         : _cont{}, _mut{}, _isOK{false}, _comp{}
{
   /* a conservative approach
      *this = PQueue_<DTy, Compare, Protected, Allocator>{} ;
      for (const auto& el : ini_list)   this->push(el) ;
   **/
   *this = PQueue_<DTy, Compare, Protected, Allocator>{} ;
   try {
      _cont = Container(1 + ini_list.size()) ;                               // allocate space
      size_t   ind = 0 ;
      for (auto& el : ini_list)    _cont[++ind] = std::move(el) ;            // data moved into: not a Heap
                                                            // Log_to(0, "\n\n> PQueue{} INITIAL container size: ",
                                                            // _cont.size(), "; capacity: ", _cont.capacity()) ;

      if (ind > 1)   {                                                       // if only 1: all is done
         for (size_t ind = this->_parent(this->size()) ; ind > 0 ; --ind) {  // for all parents
            _heapifyDOWN(ind) ;
         }                                                                   // ??? this->_reserve() ;
                                                            // Log_to(0, "> PQueue{} reserving capacity: ", 1 << this->height()) ;
         _cont.reserve(1 << this->height()) ;                                 // capacity maintained as 2 ^ height
      }

      _isOK = true ;
   } catch (...) { Log_to(0, "> PQueue(): exception caught") ; }
   // Log_to(0, "> PQueue{} FINAL container size: ", _cont.size(), "; capacity: ", _cont.capacity()) ;
}

// operations

template <typename DTy, typename Compare, bool Protected, typename Allocator>
template <typename T> bool PQueue_<DTy, Compare, Protected, Allocator>
::push(T&& value) noexcept(true)
{
   assert((std::is_same_v<Remove_cvr_t<T>, Remove_cvr_t<DTy>>)) ;

   try {
      this->_reserve() ;                                                      // Strong Guarantee expected
      _isOK = false ;                                                         // invariants are Broken
      _cont.push_back(std::forward<T>(value)) ;                               // new element at this->size()
      this->_heapifyUP(this->size()) ;                                        // restore the Heap(upwards)
      _isOK = true ;                                                          // invariants are Restored
   } catch (...) { Log_to(0, "> push(): exception caught") ; return false ; }

   return true ;
}

template <typename DTy, typename Compare, bool Protected, typename Allocator>
std::optional<DTy> PQueue_<DTy, Compare, Protected, Allocator>
::pop() & noexcept(true)
{
   if (this->size() == 0)   return std::optional<DTy>{} ;                     // if empty
   return std::optional<DTy>{this->_popPQ()} ;
}

template <typename DTy, typename Compare, bool Protected, typename Allocator>
void PQueue_<DTy, Compare, Protected, Allocator>
::pop(DTy& value) & noexcept(true)
{
   if (this->size() != 0)   value = this->_popPQ() ;                          // is empty
}


// private methods

template <typename DTy, typename Compare, bool Protected, typename Allocator>
DTy PQueue_<DTy, Compare, Protected, Allocator>
::_popPQ() & noexcept(true)
{
   assert(this->size() > 0) ;
   /* size_t   pqs =  this->size() ;
   if (pqs == 0)   return {} ;                                                // empty
   */
   size_t  pqs = this->size() ;                                               // considered NOT empty

   _isOK = false ;                                                            // invariants are Broken
   DTy   value = std::move(this->_next_to_go()) ;                             // get the Next

   if (pqs > 1) this->_access(_ind_next()) = std::move(this->_access(pqs)) ;  // = remove the dummy next
   _cont.erase(_cont.end() - 1) ;                                             // size adjusted
   if (pqs > 2)   this->_heapifyDOWN(_ind_next()) ;                           // restore the Heap for size() - 1 elements

   _isOK = true ;                                                             // invariants are Restored
   return value ;
}

template <typename DTy, typename Compare, bool Protected, typename Allocator>
void PQueue_<DTy, Compare, Protected, Allocator>
::_heapifyUP(PQ_ind ind) &
{
   // restore the _compHeap starting at ind
   for ( ; this->_has_parent(ind) ; ind = this->_ind_p(ind)) {
      decltype(auto) curr = this->_access(ind) ;
      decltype(auto) parent = this->_parent(ind) ;
      if (this->_comp(curr, parent))   std::swap(curr, parent) ;
      else                             break ;
   }
   return ;
}

template <typename DTy, typename Compare, bool Protected, typename Allocator>
void PQueue_<DTy, Compare, Protected, Allocator>
::_heapifyDOWN(PQ_ind ind) &
{
   // restore the _compHeap starting at ind
   for (int i = 0 ; this->_has_left_child(ind) ; ++i) {                           // Heap leaves are on the left side only
      decltype(auto) curr = this->_access(ind) ;
      ind = this->_comp_child(ind) ;                                  // one that compares OK to the other
      if (this->_comp(curr, this->_access(ind)))   break  ;           // parent compares OK to _comp_child
      else            std::swap(curr, this->_access(ind)) ;           // 'ind' already set to the chosen leaf
   }
   return ;
}


// external & friends

#include <iostream>

template <class T, class C, bool P, class A> std::ostream&
operator<< (std::ostream& os, const PQueue_<T, C, P, A>& pq)         // friend
{
   os << "PQueue_(" << std::boolalpha << (bool)pq
      << "): capacity(" << pq.capacity() << "), size(" << pq.size() << "), height("
      << pq.height() << "), next(" << pq.peek() << ")" ;
   if (!pq || pq.size() == 0)     return os ;

   decltype(auto) wpq = const_cast<PQueue_<T, C, P, A>&> (pq) ;
   for (size_t i = 1, j = 0 ; i <= pq.size() ; ++i, ++j)   {
      if (is_pow2(i))   j = 0, os << "\n:" ;
      os << (j % 2 == 0 ? (j % 4 == 0 && j > 0 ? "  |  " : "   ") : " - ") ;
      if (pq._has_parent(i))    os << wpq._parent(i) << '>' ;           // show the parent
      os << wpq._access(i) << '<' ;                                     // ...      element
      if (pq._has_left_child(i))    os << wpq._left_child(i) ;
      if (pq._has_right_child(i))   os << ';' << wpq._right_child(i) ;
      os  << '>' ;
   }
   return os ;
}

// collection_from_PQ(): if NOT passed by value PQ will be poped out
template <typename Collection, class T, class C, bool P, class A>
Collection collection_from_PQ(PQueue_<T, C, P, A> pq)
{
   static_assert((std::is_same_v<Remove_cvr_t<T>, Remove_cvr_t<typename Collection::value_type>>)) ;

   Collection   res_coll ;
   while (pq.size() > 0)    res_coll.push_back(*(pq.pop())) ;

   return res_coll ;
}

// showPQ(): if NOT passed by value PQ will be poped out
template <class T, class C, bool P, class A> void
showPQ(std::ostream& os, PQueue_<T, C, P, A> pq)
{
   // for (size_t i = 0 ; ; ++i) {
   while (true) {
      auto  val = pq.pop() ;
      // std::cout << " #" << i << ": " ;
      if (val)    std::cout << " " << *val ;
      else        { std::cout << "---" ; break ; }
   }

   return ;
}

#endif // PRIORITY_QUEUE_HPP
