// test_qts.cpp: testing implementation of QueueTSFG (Thread-Safe Fine Grained
//

#include <atomic>
#include <thread>

#include <vector>

#include <assert.h>
#include <algorithm>

#include "Logger_decl.hpp"
#include "Logger_helpers.hpp"

#include "src/queue_tsfg.hpp"

struct sDTy {
   std::thread::id _id ;
   int             _v ;
};


struct Comp {
   size_t  _factor{0} ;
   Comp(size_t f) : _factor{f} {}

   bool operator() (size_t v, size_t r) const& { return v % _factor == r ; }
};


void place_in_coll(QueueTSFG_<size_t>& coll, size_t count, const Comp& c, size_t res) ;
void extract_from_coll(QueueTSFG_<size_t>& coll, size_t count, std::vector<size_t>& cont) ;
void extract_wait_from_coll(QueueTSFG_<size_t>& coll, size_t count, std::vector<size_t>& cont) ;


std::atomic<bool>  flWait = true ;                                       // start signal

int main()
{
    Log_to(0, "@ ", LOG_TIME_LAPSE(Log_start())) ;
    Log_to(0, "> active cores: ", std::thread::hardware_concurrency()) ;

    {  // ...'s Life block
       QueueTSFG_<size_t>   q ;
       std::vector<size_t>  result ;
       size_t            count = 1000 ;                                       // # of elements to handle
       Log_to(0, "\n> output contains ", result.size(), "(expected ", 0, ") elements") ;

       /* with wait_and_pop()-> */

       std::thread   w_th_1(place_in_coll, std::ref(q), count, Comp{3}, 1) ;      // %3 == 1
       std::thread   w_th_2(place_in_coll, std::ref(q), count, Comp{3}, 2) ;      // %3 == 2
       std::thread   w_th_3(place_in_coll, std::ref(q), count, Comp{3}, 0) ;      // %3 == 0
       std::thread   r_th_1(extract_from_coll, std::ref(q), count, std::ref(result)) ;  // extract all into result
       // std::thread   r_th_1(extract_wait_from_coll, std::ref(q), count, std::ref(result)) ;  // extract all into result
       // some waiting */ std::this_thread::sleep_for(1000ms) ;

       flWait = false ;

       if (w_th_1.joinable())   w_th_1.join() ;
       if (w_th_2.joinable())   w_th_2.join() ;
       if (w_th_3.joinable())   w_th_3.join() ;
       if (r_th_1.joinable())   r_th_1.join() ;

       std::cout << "\n> " ;
       for (auto el : result)   std::cout << ":" << el ;
       bool fl_found = true ;
       for (size_t i = 0 ; i < count ; ++i) {
          fl_found &= (std::find(begin(result), end(result), i) != end(result)) ;
       }
       Log_to(0, "> output contains ", result.size(),
                 "(expected ", count, ") elements: ", fl_found ? "all found" : "ERROR") ;
    }

    Log_to(0, "\n> That's it...", LOG_TIME_LAPSE(Log_start()), '\n') ;
    return 0 ;
}


void                              // push Values into Collection
place_in_coll(QueueTSFG_<size_t>& coll, size_t count,
              const Comp& c, size_t res)      // factored within [0, count) if comp
{
   while (flWait) ;                                                       // for a Start

   for (size_t i = 0 ; i < count ; ++i)    {
      // std::this_thread::sleep_for(std::chrono::microseconds(10000)) ;
      if (c(i, res))   coll.push(i) ;
   }
}


void                              // extract Values into 'cont'
extract_from_coll(QueueTSFG_<size_t>& coll, size_t count, std::vector<size_t>& cont)
{
   while (flWait) ;

   decltype(auto)   handle = coll.try_pop() ;

   for (size_t i = 0 ; i < count ; handle = coll.try_pop())   {
      if (handle)   { cont.push_back(std::move(*handle)) ; ++i ; }
   }
}


void                              // extract Values into 'cont': wait until data present
extract_wait_from_coll(QueueTSFG_<size_t>& coll, size_t count, std::vector<size_t>& cont)
{
/*   std::unique_ptr<size_t> handle{} ;
   for (size_t i = 0 ; i < count ; ++i) {
      handle = coll.wait_and_pop() ;
      cont.push_back(*handle) ;
   } */
   // does not wait to test the _pop() mechanics
   size_t  eel = 0 ;
   for (size_t i = 0 ; i < count ; ++i) coll.wait_and_pop(eel), cont.push_back(eel) ;
}

// eof test_qts.cpp
