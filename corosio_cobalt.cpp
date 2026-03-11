#include <boost/cobalt.hpp>
#include <boost/capy.hpp>
#include <boost/corosio.hpp>
#include <coroutine>
#include <exception>
#include <iostream>
#include <libs/asio/include/boost/asio/this_coro.hpp>
#include <memory_resource>
#include <stop_token>


namespace cobalt = boost::cobalt;
namespace capy = boost::capy;
namespace corosio = boost::corosio;

boost::asio::execution_context asio_context;

struct wrapped_executor
{
    boost::capy::executor_ref ex;

    wrapped_executor(boost::capy::executor_ref ex1) : ex(ex1) {}

    template<class F>
    void execute(F f) const 
    {
      capy::run_async(ex, std::move(f))([]() -> capy::task<void> {co_return;}());
    }

    boost::asio::execution_context &
    query(boost::asio::execution::context_t) const noexcept
    {
        return asio_context; 
    }

    static constexpr boost::asio::execution::blocking_t
    query(boost::asio::execution::blocking_t) noexcept
    {
      return boost::asio::execution::blocking.never;
    }

    bool
    operator==(wrapped_executor const &other) const noexcept
    {
      return ex == other.ex;
    }

    bool
    operator!=(wrapped_executor const &other) const noexcept
    {
      return !(*this == other);
    }
};

template<typename Aw>
struct cobalt_task_adaptor
{
  Aw inner;
  using type = decltype(inner.await_resume());
  boost::system::result<type, std::exception_ptr> res{std::exception_ptr()}; 
  

  struct promise_type : cobalt::promise_memory_resource_base
  {
    struct helper
    {
      std::coroutine_handle<void> handle;
      using promise_type = cobalt_task_adaptor<Aw>::promise_type;
    };

    helper get_return_object() 
    {
      return helper{std::coroutine_handle<promise_type>::from_promise(*this)};
    }

    std::suspend_never initial_suspend() const noexcept   { return {};}
    auto final_suspend() noexcept   
    {
      struct aw
      {
        std::coroutine_handle<> h;
        bool await_ready() const noexcept {return false;}

        std::coroutine_handle<> await_suspend(std::coroutine_handle<> hh) noexcept
        {
          hh.destroy();
          return h;
        }
        void await_resume() const noexcept {}
      };
      
      return aw{std::exchange(h, std::noop_coroutine())};
    }
    
    void unhandled_exception() 
    {
      a.res = {boost::system::in_place_error, std::current_exception()};
    }

    template<typename U>
    std::suspend_always yield_value(U && u)
    {
      a.res = {boost::system::in_place_value, std::forward<U>(u)};
      return {};
    }

    void return_void()
    {
      if (a.res.has_error())
        a.res = {boost::system::in_place_value};
    }


    promise_type(cobalt_task_adaptor& a, std::coroutine_handle<> h, const capy::io_env * env) : a(a), h(h), env(env)
    {
    }

    ~promise_type() {h.destroy(); }

    cobalt_task_adaptor& a;
    std::coroutine_handle<> h;
    const capy::io_env * env;

    boost::asio::cancellation_signal sig;
    struct stopper 
    {
      boost::asio::cancellation_signal &sig; 
      void operator()() {sig.emit(boost::asio::cancellation_type::terminal);}
    };
    std::stop_callback<stopper> sp{env->stop_token, stopper{sig}};


    boost::asio::cancellation_slot get_cancellation_slot()
    {
      return sig.slot();
    }
    std::pmr::polymorphic_allocator<void> get_allocator()  const { return env->frame_allocator; }
    cobalt::executor get_executor() const
    {
      return cobalt::executor(wrapped_executor(env->executor));
    }
  };
  

  promise_type::helper do_suspend(std::coroutine_handle<> h, const capy::io_env * env)
  {
    if constexpr (std::is_void_v<type>)
      co_await inner;
    else
      co_yield co_await inner;
  }

  bool await_ready() {return inner.await_ready();}
  //template<typename Promise>
  std::coroutine_handle<> await_suspend(std::coroutine_handle<> h, const capy::io_env * env)
  {
      cobalt::this_thread::set_default_resource(env->frame_allocator);
      return do_suspend(h, env).handle;
  }

  auto await_resume() 
  {
    return std::move(res.value());
  }
};



template<typename T>
auto adapt(cobalt::task<T> &&tt)
{
  return cobalt_task_adaptor{std::move(tt).operator co_await()};
}

cobalt::task<int> inner() 
{
  std::cout << "Cobalt task " << std::endl;
  co_return 42; 
}

capy::task<int> outer()
{
  auto alloc = co_await capy::this_coro::frame_allocator;
  cobalt::this_thread::set_default_resource(alloc);
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
