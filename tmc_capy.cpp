
#include <memory_resource>
#include <tmc/all_headers.hpp>
#include <boost/capy.hpp>

#include <iostream>

namespace capy = boost::capy;

boost::capy::execution_context ctx;



tmc::ex_any* cast_executor(const void * p) 
{
  return const_cast<tmc::ex_any*>(static_cast<const tmc::ex_any*>(p));
};

template<>    
inline constexpr capy::detail::executor_vtable capy::detail::vtable_for<tmc::ex_any> = 
{
  .context = +[](void const * ) noexcept -> boost::capy::execution_context& {return ctx;},
  .on_work_started  = [](const void * p) noexcept {},
  .on_work_finished = [](const void * p) noexcept {},
  .post = [](const void * p, std::coroutine_handle<> h) noexcept 
          {
            tmc::post(cast_executor(p), h);
          },
  .dispatch = [](const void * p, std::coroutine_handle<> h) noexcept 
              {
                return tmc::detail::executor_traits<tmc::ex_any*>::dispatch(cast_executor(p), h, 0);
              },
  .equals = [](void const* a, void const* b) noexcept 
            {return a == b;},
  .type_id=&boost::capy::detail::type_id<tmc::ex_any>()
};


template<typename T>
struct capy_task_adaptor
{
  capy::task<T> tt;
  capy::io_env env;
  std::stop_source src;
  
  bool await_ready () const {return false;}

  template<typename Promise>
  auto await_suspend(std::coroutine_handle<Promise> h) 
  {
    // copied from tmc details.
    auto ex = static_cast<tmc::ex_any*>(h.promise().customizer.continuation_executor);
    if (ex)
      env.executor = {*ex};
    // tmc doesn't modify allocations
    env.frame_allocator = std::pmr::get_default_resource();
    return tt.await_suspend(h, &env);
  }

  T await_resume() 
  {
    return tt.await_resume();
  }
};

template<typename T>
auto adapt(capy::task<T> && tt) 
{
  return capy_task_adaptor<T>{std::move(tt)};
}


capy::task<int> inner() 
{
  std::cout << "capy task" << std::endl;
  co_return 42; 
}

tmc::task<int> outer() 
{
  std::cout << "tmc task 1" << std::endl;
  auto res = co_await adapt(inner());
  std::cout << "tmc task 2: " << res << std::endl;
  co_return res;
}


int main() {
  // Manually construct an executor and block on a root task.
  // `tmc::async_main()` could be used instead to simplify this.
  tmc::cpu_executor().init();

  // Synchronous (blocking) APIs return a std::future.
  int result = tmc::post_waitable(tmc::cpu_executor(), outer()).get();
  std::cout << result << std::endl;
  return 0;
}
