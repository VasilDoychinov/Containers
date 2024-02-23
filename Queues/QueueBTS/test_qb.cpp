// test_qb.cpp: testing implementation of QueueBTS (Bounded Thread-Safe Fine Grained, synchronization with std::mutex
//

#include <atomic>
#include <thread>
#include <future>

#include <vector>

#include <assert.h>
#include <algorithm>

#include "Logger_decl.hpp"
#include "Logger_helpers.hpp"

#include "src/queue_bts.hpp"

struct sDTy {
   std::thread::id _id ;
   int             _v ;
};


struct Comp {
   size_t  _factor{0} ;
   Comp(size_t f) : _factor{f} {}

   bool operator() (size_t v, size_t r) const& { return v % _factor == r ; }
};


struct TSP_ {
  TSP_() {  Log_to(0, "> TSP_() constructor...") ; }
  ~TSP_() { Log_to(0, "> TSP_() destructor...") ; }
};


std::unique_ptr<int>  test_smptr(std::unique_ptr<TSP_>&& up)
{
   int   value = 101 ;

/*
   std::unique_ptr<char>  pch = std::make_unique<char>('P') ;
   std::unique_ptr<long>  plo = std::make_unique<long>(1L << (sizeof(long) - 1)) ;
   Log_to(0, "> in test_smptr(): size(up<ch>): ", sizeof(pch), ", size(up<long>): ", sizeof(plo)) ;
   Log_to(0, "> in test_smptr():        <ch> : ", *pch, ",        <long> : ", *plo) ;

   std::shared_ptr<char>  spch = std::make_unique<char>('P') ;
   std::shared_ptr<long>  splo = std::make_shared<long>(1L << (sizeof(long) - 1)) ;
   Log_to(0, "> in test_smptr() shared: size(sp<ch>): ", sizeof(spch), ", size(sp<long>): ", sizeof(splo)) ;
   Log_to(0, "> in test_smptr() shared:        <ch> : ", *spch, ",        <long> : ", *splo) ;
**/

   return std::make_unique<int>(value) ;
}

template <class Test_> struct Del_ {
   void operator() (Test_* p) { Log_to(0, "> deleter of UP (Del_))") ; delete p ; }
};


using Test_type = size_t ;

// some testing fucntions
void bq_initial_tests() ;
void bq_initial_t1() ;
void bq_concurrent_ths() ;         // uses


template <typename T> std::ostream&
operator<< (std::ostream& os, const std::vector<T>& v)
{
   os << ">" ;
   std::for_each(v.cbegin(), v.cend(), [&](const auto el){ os << ":" << el ; }) ;
   return os << "<" ;
}


std::atomic<bool>  flWait = true ;                                       // start signal


int main()
{
    Log_to(0, "@ ", LOG_TIME_LAPSE(Log_start())) ;
    Log_to(0, "> active cores: ", std::thread::hardware_concurrency()) ;

/* smart pointers
    std::unique_ptr<TSP_>  sptr ;
    {
       std::unique_ptr<TSP_> up { new TSP_ } ;

       auto  res = test_smptr(std::move(up)) ;
       Log_to(0, "> test_smptr(101) returned: ", *res) ;
       sptr = std::move(up) ;
    }
    // std::vector<std::unique_ptr<int>>  cont ;
    Log_to(0, "> after block") ;
**/

    try {  // ...'s Life block

       if (false)    bq_initial_tests() ;
       if (false)    bq_initial_t1() ;
       if (true)     bq_concurrent_ths() ;                                // multiple Readers and Writers

    } catch (...) { Log_to(0, "\n> exception caught...") ; }

    Log_to(0, "\n> That's it...", LOG_TIME_LAPSE(Log_start()), '\n') ;
    return 0 ;
}

template <typename T> using Q_ = QueueBTS_<T> ;

void writer_task(Q_<Test_type>& q, std::vector<Test_type> buffer) ;      // place buffer's elements into 'bq'
std::vector<Test_type> reader_task(Q_<Test_type>& q, size_t count) ;     // retrieve 'count' elements from 'bq


void writer_task(Q_<Test_type>& bq, std::vector<Test_type> buffer)       // place buffer's elements into 'bq'
{
   assert(buffer.size() > 0) ;
   Log_to(0, "> from #", std::this_thread::get_id(),
             " writer_task(): ready to push ", buffer.size(), " elements [",
             *buffer.begin(), ", ", *(buffer.end() - 1), "]") ; // , buffer) ;
   while (flWait) ; // std::this_thread::sleep_for(std::chrono::milliseconds(100)) ;
   Log_to(0, "> from #", std::this_thread::get_id(),
             " writer_task() to commence @", LOG_TIME_LAPSE(Log_start())) ;
                                  // push all of it
   auto   res = bq.push_list(std::move(buffer)) ;
   /* Log_to(0, ": from #", std::this_thread::get_id(),
             " exiting writer_task() @", LOG_TIME_LAPSE(Log_start()),
             ": push_list returns: ", res) ; */
}

std::vector<Test_type> reader_task(Q_<Test_type>& bq, size_t count)      // retrieve 'count' elements from 'bq
{
   Log_to(0, "> from #", std::this_thread::get_id(),
             " reader_task(): ready to extract ", count, " elements") ;
   while (flWait) ; // std::this_thread::sleep_for(std::chrono::seconds(1)) ;
   Log_to(0, "> from #", std::this_thread::get_id(),
             " reader_task() to commence @", LOG_TIME_LAPSE(Log_start())) ;

   std::vector<Test_type>  result ; // Log_to(0, ": reader_task(): exits...") ; return result ;
   try {
      result = bq.pop_list(count) ;
   } catch (...) { throw ; }

   /* Log_to(0, ": from #", std::this_thread::get_id(),
             " exiting reader_task() @", LOG_TIME_LAPSE(Log_start())) ; */
   return result ;
}

std::vector<Test_type>
prepare_test_input(Test_type l, Test_type r)                             // load [l, r) into a vector
{
   std::vector<Test_type>   buffer{} ;
   for ( ; l < r ; ++l)   buffer.push_back(l) ;
   buffer.shrink_to_fit() ;                                              // Log_to(0, "> prepared test buffer of size: ", buffer.size()) ;
   return buffer ;
}

std::vector< std::pair< std::thread, std::future<std::vector<Test_type>> > >
launch_readers(Q_<Test_type>& bq, size_t num_readers, size_t total_test_size)
{
   assert(num_readers > 0 && total_test_size > num_readers) ;

   std::vector<std::pair< std::thread, std::future<std::vector<Test_type>> >> ths ;

   size_t   th_ts = total_test_size / num_readers ;
   size_t   th_ths_rem = total_test_size % num_readers ;

   for (size_t i = 0 ; i < num_readers ; ++i)  {
      std::packaged_task<std::vector<Test_type>(Q_<Test_type>&, size_t)> do_read{std::move(reader_task)} ;
      std::future<std::vector<Test_type>>   fut_result = do_read.get_future() ;
      size_t  ts = th_ts ; if (i == num_readers - 1 )  ts += th_ths_rem ;
      std::thread th(std::move(do_read), std::ref(bq), ts) ;
      ths.emplace_back(std::move(th), std::move(fut_result)) ;
   }
   ths.shrink_to_fit() ;
   return ths ;
}

std::vector< std::thread >
launch_writers(Q_<Test_type>& bq, size_t num_writers, size_t total_test_size)
{
   assert(num_writers > 0 && total_test_size > num_writers) ;

   std::vector< std::thread > ths ;

   size_t   th_ts = total_test_size / num_writers ;
   size_t   th_ths_rem = total_test_size % num_writers ;
   size_t   left = 0 ;

   for (size_t i = 0 ; i < num_writers ; ++i)  {
      size_t  rl = left + th_ts ; if (i == num_writers - 1 )  rl += th_ths_rem ;
      std::thread th(std::move(writer_task), std::ref(bq), prepare_test_input(left, rl));
      ths.emplace_back(std::move(th)) ;
      left = rl ;
   }
   ths.shrink_to_fit() ;
   return ths ;
}

void bq_concurrent_ths()
{
   int   value = 0 ;
   constexpr size_t      capacity = 5 ;
   QueueBTS_<Test_type>  bq{capacity} ;                                  Log_to(0, ": empty ", bq) ;

   constexpr size_t   test_size = 100000 ;                                // Test size (total)
   constexpr size_t   num_readers = 3 ;                                  // # reading threads
   constexpr size_t   num_writers = 5 ;                                  // # writing threads

   flWait = true ;
   Log_to(0, "\n> Concurrentcy: Tests------------------: queue is: ",
             (bq.empty() ? "" : "NOT"), "> test size: ", test_size,
             ", readers: ", num_readers, ", writers: ", num_writers) ;

                                   // prepare for Reading threads
   /* std::packaged_task<std::vector<Test_type>(Q_<Test_type>&, size_t)> r1_do_read{std::move(reader_task)} ;
   std::future<std::vector<Test_type>>   fut1_result = r1_do_read.get_future() ;
   std::packaged_task<std::vector<Test_type>(Q_<Test_type>&, size_t)> r2_do_read{std::move(reader_task)} ;
   std::future<std::vector<Test_type>>   fut2_result = r2_do_read.get_future() ;
   std::packaged_task<std::vector<Test_type>(Q_<Test_type>&, size_t)> r3_do_read{std::move(reader_task)} ;
   std::future<std::vector<Test_type>>   fut3_result = r3_do_read.get_future() ;
                                  // launch Readers
   std::thread th1_reader(std::move(r1_do_read), std::ref(bq), ts_read1) ;
   std::thread th2_reader(std::move(r2_do_read), std::ref(bq), ts_read2) ;
   std::thread th3_reader(std::move(r3_do_read), std::ref(bq), ts_read3) ;
   */
   auto   readers = launch_readers(bq, num_readers, test_size) ;
                                  // launch Writers
   /* std::thread th1_writer(std::move(writer_task), std::ref(bq), prepare_test_input(0, ts_buff1));
   std::thread th2_writer(std::move(writer_task), std::ref(bq),  prepare_test_input(ts_buff1, ts_buff1 + ts_buff2));
   std::thread th3_writer(std::move(writer_task), std::ref(bq),  prepare_test_input(ts_buff1 + ts_buff2, test_size));
   */
   auto   writers = launch_writers(bq, num_writers, test_size) ;

   std::this_thread::sleep_for(std::chrono::milliseconds(100)) ;
   flWait = false ;

   std::vector<Test_type>   result ;                                     // accumulate all from readers in

   Log_to(0, "\n----- wating for results\n") ;
   for (auto& [th, fut_res] : readers)   {
      auto res = fut_res.get() ;
      result.insert(end(result), res.begin(), res.end()) ;
      if (th.joinable())   { th.join() ; Log_to(0, "> joined a reader") ; }
   }
   result.shrink_to_fit() ;                                              // results from all threads are in

   /* std::vector<Test_type>   result = fut1_result.get() ;                 // obtain results
   std::vector<Test_type>   r2 = fut2_result.get() ;
   std::vector<Test_type>   r3 = fut3_result.get() ;
   result.insert(end(result), r2.begin(), r2.end()), result.shrink_to_fit() ;// result holds both outcomess
   result.insert(end(result), r3.begin(), r3.end()), result.shrink_to_fit() ;// result holds both outcomess
   if (th1_reader.joinable())   { th1_reader.join() ; Log_to(0, ": reader_1 joined") ; }
   if (th2_reader.joinable())   { th2_reader.join() ; Log_to(0, ": reader_2 joined") ; }
   if (th3_reader.joinable())   { th3_reader.join() ; Log_to(0, ": reader_3 joined") ; }
   */
   Log_to(0,"> results obtained > reading threads joined @ ", LOG_TIME_LAPSE(Log_start()), '\n') ;

   for (auto& th : writers)   if (th.joinable()) { th.join() ; Log_to(0, "> joined a writer") ; }
   Log_to(0,"> writing threads joined @ ", LOG_TIME_LAPSE(Log_start()), '\n') ;

   /* if (th1_writer.joinable())   { th1_writer.join() ; Log_to(0, ": writer_1 joined") ; }
   if (th2_writer.joinable())   { th2_writer.join() ; Log_to(0, ": writer_2 joined") ; }
   if (th3_writer.joinable())   { th3_writer.join() ; Log_to(0, ": writer_3 joined") ; } */

   Log_to(0, "----------------------------------- @ ", LOG_TIME_LAPSE(Log_start()), "\n") ;

   // Log_to(0, "> the result is: ", result) ;
   Log_to(0, "> queue is(expected empty): ", bq) ;

   Log_to(0, "> checking results...") ;
   std::sort(result.begin(), result.end()) ;    // sort result to compare with [0...test_size)

   if (true)   {
      for (size_t i = 1 ; i < test_size ; ++i)   {
         if (result[i - 1] + 1 != result[i])   Log_to(0, "> Mismatch i - 1: ", result[i - 1], " <> i: ", result[i]) ;
      }
   }
   if (true) {
      size_t   err_count = 0 ;
      bool fl_ok = true ;
      for (size_t i = 0 ; i < test_size ; ++i) {   // as 'buffer' might have changed
         if (i != result.at(i))   { fl_ok = false ;
            std::vector<Test_type>   check_b{} ;
            size_t   left  = i > capacity ? i - capacity : 0 ;
            size_t   right = std::min(i + capacity, result.size()) ;
            for ( ; left < right ; ++left)   check_b.push_back(left) ;
            Log_to(0, "\n> origin: ", i, " >>> ERROR >>> found: ", result.at(i), "> ", check_b) ;
            if (err_count++ == 10)   break ;
         }
      }
      Log_to(0, "\n> test buffer(s) contained: [0 ... ", test_size,
                ") > result contains: ", result.size(), " elements >> [",
                result[0], ", ", result[result.size() - 1], "]: ",
                fl_ok ? "all from origin found & match" : "ERROR") ;
      if (!fl_ok)   {
         for (size_t i = 1 ; i < test_size ; ++i)   {
            if (result[i - 1] + 1 != result[i]) Log_to(0, "> Mismatch i - 1: ", result[i - 1], " <> i: ", result[i]) ;
         }
         Log_to(0, "\n> result >", result) ;
      }
   } // if()

   Log_to(0, "\n> eo Concurrentcy: Initial Tests-------------- @ ", LOG_TIME_LAPSE(Log_start())) ;
}



void bq_initial_t1()              // test the Circularity: no Concurrency
{
   int   value = 0 ;
   constexpr size_t  capacity = 5 ;
   QueueBTS_<int>  bq{capacity} ;                                    Log_to(0, ": empty ", bq) ;

   Log_to(0, "\n> Circularity Tests------------------: queue is: ", (bq.empty() ? "" : "NOT"), " empty") ;

   bq.push_list({0, 1, 2, 3, 4}) ;
   Log_to(0, "\n: after push_list[0, ", capacity, "): ", bq) ;

   std::vector<int>  work = bq.pop_list(3) ;
   Log_to(0, "\n: after pop_list(3)    : ", bq, " >>> result: ", work) ;

   bq.push_list({0, 1, 2, 3, 4}) ;
   Log_to(0, "\n: after push_list[0, ", capacity, "): ", bq) ;
   Log_to(0, ": try_push(10): ", bq.try_push(10) ? "done" : "failed", "> ", bq) ;

   work = bq.pop_list(bq.size()) ;                                       //uses wait_and_pop(): care about !empty() or, NOT
   Log_to(0, "\n: after pop_list(all)  : ", bq, " >>> result: ", work) ;

   Log_to(0, "\n> eo Circularity Tests--------------") ;
}

void bq_initial_tests()           // test try_push/pop() with empty/full/...
{
       int   value = 0 ;
       constexpr size_t  capacity = 5 ;
       QueueBTS_<int>  bq{capacity} ;                                    Log_to(0, ": empty ", bq) ;

       Log_to(0, "> initial Tests------------------\n") ;
       Log_to(0, ": queue is: ", (bq.empty() ? "" : "NOT"), " empty") ;
       for (size_t i = 0 ; i < capacity ; ++i)   {
          Log_to(0, "> try_pop(", i, "): ", bq.try_pop(value) ? "done-" : "failed-", value, "> ", bq) ;
       }


       for (int  v = 0 ; v < capacity ; ++v) {
          Log_to(0, "> try_push(", v, "): ", bq.try_push(v) ? "done" : "failed", "> ", bq) ;
       }
       Log_to(0, "> queue: ", bq, " expected >:0...:", capacity - 1, "<") ;

       value = capacity ;
       Log_to(0, "\n> try_push(", value, ") into full queue: ", bq.try_push(value) ? "done" : "failed") ;
       Log_to(0, ": queue: ", bq, " expected failed & >:0...:", capacity - 1, "<") ;

       for (size_t i = 0 ; i < capacity + 1 ; ++i)   {
          Log_to(0, "> try_pop(", i, "): ", bq.try_pop(value) ? "done-" : "failed-", value, "> ", bq) ;
       }
       Log_to(0, ": queue: ", bq, " expected failed & ><") ;
}

// eof test_qb.cpp
