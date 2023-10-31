#include <iostream>

#include "Logger_decl.hpp"
#include "Logger_helpers.hpp"

#include "src/priority_queue.hpp"

class Test { int x ; Test(const Test&) = default ; } ;   // NOT Swappable (NOT movable) => PQueue_<Test> is NOT constuctible

int main()
{     // PQueue_<Test>   wt ;   see class Test ;

   PQueue_<int>   wpq(PQueue_<int>{}) ;     Log_to(0, "> #1---\n> ", wpq) ;

   auto           wwpq = std::move(wpq) ;   Log_to(0, "\n> #2---\n> ", wwpq) ;
   Log_to(0, "\n> #1 (moved-from)---\n> ", wpq) ;

   {
      // PQueue_<int>   pq{1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16} ;
      PQueue_<int, std::greater<int>>   pq{17, 18, 19, 20, 21, 22, 23, 24, 25, 16, 15, 14, 13, 12, 11, 10, 9, 8, 7, 6, 5, 4, 3, 2, 1} ;
      // PQueue_<int>   pq{5, 6, 4, 3, 2, 1} ;
      // Log_to(0, "\n> from {1, ..., 16}\n> ", pq) ;

      auto           wpq = std::move(pq) ;   Log_to(0, "\n> {}---\n> ", wpq) ;
      Log_to(0, "\n> {}(moved-from)---\n> ", pq) ;

      Log_to(0, "\n> testing pop() through wpq and showPQ(): \n:") ;  showPQ(std::cout, wpq) ;
      Log_to(0, "> wpq after test\n> ", wpq) ;

      Log_to(0, "\n> testing pop() through std::move(wpq) and showPQ(): \n:") ;  showPQ(std::cout, std::move(wpq)) ;
      Log_to(0, "> wpq after test(moved-from)\n> ", wpq) ;

   }

   if (true) {
      PQueue_<int>   pq{25} ; showPQ(std::cout << "\n\n> showPQ{25}: ", pq) ;
      Log_to(0, "> from {25}\n> ", pq) ;
      auto   res = pq.pop() ;
      Log_to(0, "> pq{25}> pop(): ", res ? *res : 0, "-> pq: ", pq) ;

      std::initializer_list<int>  il = {25, 17, 18, 19, 20, 21, 22, 23, 24, 16, 15, 14, 13, 12, 11, 10, 9, 8, 7, 6, 5, 4, 3, 2, 1} ;
      for (auto& el : il)   pq.push(std::move(el)) ;
      Log_to(0, "> pq after push() from {}\n> ", pq) ;
      showPQ(std::cout << "\n\n> showPQ{}: ", pq) ;
   }

   if (true)   {
      // PQueue_<int>   pq{1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16} ;
      PQueue_<int, std::greater<int>>   pq{17, 18, 19, 20, 21, 22, 23, 24, 25, 16, 15, 14, 13, 12, 11, 10, 9, 8, 7, 6, 5, 4, 3, 2, 1} ;
      showPQ(std::cout << "\n\n> showPQ{()}}:", pq) ;

      auto   res = collection_from_PQ<std::vector<int>>(std::move(pq)) ;
      Log_to(0, "\n> result from collection_from_PQ(): ") ;
      for (auto& el : res) std::cout << " " << el ;

      Log_to(0, "> PQ(moved-from) after collection_() test\n> ", pq) ;
   }


   Log_to(0,"\n> That's it...\n") ;
   return 0;
}
