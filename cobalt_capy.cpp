#include <boost/cobalt.hpp>
#include <boost/capy.hpp>
#include <coroutine>
#include <iostream>

namespace cobalt = boost::cobalt;
namespace capy = boost::capy;

boost::capy::execution_context ctx;

boost::asio::io_context::executor_type * cast_executor(const void * p) 
{
  auto exec = const_cast<cobalt::executor*>(static_cast<const cobalt::executor*>(p));
  auto io_exec = exec->target<boost::asio::io_context::executor_type>();
  if (io_exec == nullptr)
    throw std::runtime_error("Unknown  executor type");

  return io_exec;        
};


template<>    
inline constexpr capy::detail::executor_vtable capy::detail::vtable_for<cobalt::executor> = 
{
  .context = +[](void const * ) noexcept -> boost::capy::execution_context& {return ctx;},
  .on_work_started  = [](const void * p) noexcept { cast_executor(p)->on_work_started();},
  .on_work_finished = [](const void * p) noexcept {cast_executor(p)->on_work_finished();},
  .post = [](const void * p, std::coroutine_handle<> h) noexcept {post(*cast_executor(p), h);},
  .dispatch = [](const void * p, std::coroutine_handle<> h) noexcept -> std::coroutine_handle<>
              {
                auto ex = cast_executor(p);
                if (ex->running_in_this_thread())
                  return h;
                else
                  post(*ex, h);
                return std::noop_coroutine();
              },
  .equals = [](void const* a, void const* b) noexcept 
            {return *cast_executor(a) == *cast_executor(b);},
  .type_id=&boost::capy::detail::type_id<boost::asio::io_context::executor_type>()
};
    

template<typename T>
struct capy_task_adaptor
{
  capy::task<T> tt;
  capy::io_env env;
  std::stop_source src;
  boost::asio::cancellation_slot slt;
  
  bool await_ready () const {return false;}

  template<typename Promise>
  auto await_suspend(std::coroutine_handle<Promise> h) 
  {
    cobalt::detail::task_promise<int> & p = h.promise();
    env.executor = p.get_executor();

    env.stop_token = src.get_token();
    slt = p.get_cancellation_slot();
    slt.assign(
      [this](auto ct) 
      {
        if ((ct & boost::asio::cancellation_type::terminal)
               != boost::asio::cancellation_type::none)
          src.request_stop();
      });

    // requires PMR to be compatible
    env.frame_allocator = p.get_allocator().resource();
    return tt.await_suspend(h, &env);
  }

  T await_resume() 
  {
    if (slt.is_connected())
      slt.clear();
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

cobalt::task<int> outer()
{
  std::cout << "cobalt task 1" << std::endl;
  auto res = co_await adapt(inner());
  std::cout << "cobalt task 2: " << res << std::endl;
  co_return res;
}


int main(int argc, char * argv[])
{
  return cobalt::run(outer());
}
