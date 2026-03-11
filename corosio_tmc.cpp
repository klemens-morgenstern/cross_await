#include <boost/capy.hpp>
#include <boost/corosio.hpp>
#include <boost/system/result.hpp>

#include <tmc/task.hpp>

#include <coroutine>
#include <exception>
#include <iostream>
#include <memory_resource>
#include <stop_token>


namespace capy = boost::capy;
namespace corosio = boost::corosio;

template<typename Result>
struct tmc_task_adaptor
{
  tmc::task<Result> inner;
  std::unique_ptr<tmc::aw_task<tmc::task<Result>, Result>> aw;

  capy::executor_ref er;
  tmc::ex_any ex;

  tmc_task_adaptor(tmc::task<Result> && inner) : inner(std::move(inner)) {}
  tmc_task_adaptor(tmc_task_adaptor && res) noexcept : inner(std::move(res.inner)) {}

  bool await_ready() 
  {
    return false;
  }
  

  std::coroutine_handle<> await_suspend(std::coroutine_handle<> h, const capy::io_env * env)
  {
    er = env->executor;
    ex.executor = &er;
    ex.s_post = +[](void* Erased, tmc::work_item&& Item, size_t Priority, size_t ThreadHint) noexcept
         {
           static_cast<capy::executor_ref*>(Erased)->post(Item);
         };
        
    ex.s_post_bulk = +[](void* Erased, tmc::work_item* Items, size_t Count, size_t Priority, size_t ThreadHint) noexcept
         {
          for (auto i = 0u; i < Count; i ++)
            static_cast<capy::executor_ref*>(Erased)->post(Items[i]);
         };

    tmc::detail::get_awaitable_traits<tmc::task<Result>>::set_continuation_executor(inner, &ex);
    aw.reset(new tmc::aw_task<tmc::task<Result>, Result>(std::move(inner).operator co_await()));
    return aw->await_suspend(h);
  }

  auto await_resume() 
  {
    return aw->await_resume();
  }
};


template<typename T>
auto adapt(tmc::task<T> &&tt)
{
  return tmc_task_adaptor{std::move(tt)};
}

tmc::task<int> inner() 
{
  std::cout << "Tmc task " << std::endl;
  co_return 42; 
}

capy::task<int> outer()
{
  std::cout << "Capy task 1" << std::endl;
  auto res =  co_await adapt(inner());
  std::cout << "Capy task 2: " << res << std::endl;
  co_return res;
}


int main(int argc, char * argv[])
{
  int res;
  corosio::io_context ctx;
  capy::run_async(ctx.get_executor(), [&](int i) {res = i;})(outer());
  ctx.run();
  return res;
}
