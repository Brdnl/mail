#pragma once

#include "../session.hpp"
#include <boost/beast/core/handler_ptr.hpp>
#include <boost/beast/core/type_traits.hpp>
#include <boost/asio/associated_allocator.hpp>
#include <boost/asio/associated_executor.hpp>
#include <boost/asio/coroutine.hpp>
#include <boost/asio/handler_continuation_hook.hpp>
#include <boost/asio/write.hpp>

namespace mail::smtp {
	namespace detail {
		inline auto noop_buffer()
		{
			return boost::asio::const_buffer{ "NOOP\r\n", 6 };
		}
	}

	template <class Stream>
	template <class Handler>
	class session<Stream>::noop_op
		: public boost::asio::coroutine {
	private:
		struct data
		{
			session<Stream>& s;

			data(const Handler&, session<Stream>& s_)
				: s(s_)
			{
			}
		};
		boost::beast::handler_ptr<data, Handler> d_;
	public:
		noop_op(noop_op&&) = default;
		noop_op(const noop_op&) = delete;

		template <class DeducedHandler, class... Args>
		noop_op(DeducedHandler&& h,
				session<Stream>& s, Args&&... args)
			: d_(std::forward<DeducedHandler>(h),
				 s, std::forward<Args>(args)...)
		{
		}

		using allocator_type = boost::asio::associated_allocator_t<Handler>;

		allocator_type get_allocator() const noexcept
		{
			return boost::asio::get_associated_allocator(d_.handler());
		}

		using executor_type = boost::asio::associated_executor_t<
			Handler, decltype(std::declval<session<Stream>&>().get_executor())>;

		executor_type get_executor() const noexcept
		{
			return boost::asio::get_associated_executor(d_.handler(), d_->s.get_executor());
		}

		void operator()(boost::beast::error_code ec = {}, std::size_t bytes = 0);

		friend bool asio_handler_is_continuation(noop_op* op)
		{
			using boost::asio::asio_handler_is_continuation;
			return asio_handler_is_continuation(std::addressof(op->d_.handler()));
		}
	};
	template <class Stream>
	template <class Handler>
	void session<Stream>::noop_op<Handler>::operator()(boost::beast::error_code ec, std::size_t bytes)
	{
		auto& d = *d_;
		BOOST_ASIO_CORO_REENTER(*this) {
			BOOST_ASIO_CORO_YIELD
				boost::asio::async_write(d.s.s_, detail::noop_buffer(), std::move(*this));
			if (ec) {
				goto upcall;
			}
			BOOST_ASIO_CORO_YIELD d.s.async_read_resp(std::move(*this));
			if (ec) {
				goto upcall;
			}
			if (d.s.resp_parser_.get().code() != reply_code::completed) {
				ec = error::failed;
				goto upcall;
			}
		upcall:
			d_.invoke(ec);
		}
	}

	template <class Stream>
	void session<Stream>::noop()
	{
		static_assert(boost::beast::is_sync_stream<next_layer_type>::value,
					  "SyncStream requirements not met");

		boost::beast::error_code ec;
		noop(ec);
		if (ec)
			BOOST_THROW_EXCEPTION(boost::beast::system_error{ ec });
	}
	template <class Stream>
	void session<Stream>::noop(boost::beast::error_code& ec)
	{
		static_assert(boost::beast::is_sync_stream<next_layer_type>::value,
					  "SyncStream requirements not met");

		boost::asio::write(s_, detail::noop_buffer(), ec);
		if (ec) {
			return;
		}
		read_resp(ec);
		if (ec) {
			return;
		}
		if (resp_parser_.get().code() != reply_code::completed) {
			ec = error::failed;
			return;
		}
	}
	template <class Stream>
	template <class NoopHandler>
	BOOST_ASIO_INITFN_RESULT_TYPE(
		NoopHandler, void(boost::beast::error_code)
	) session<Stream>::async_noop(NoopHandler&& handler)
	{
		static_assert(boost::beast::is_async_stream<next_layer_type>::value,
					  "AsyncStream requirements not met");

		boost::asio::async_completion<
			NoopHandler,
			void(boost::beast::error_code)> init{ handler };

		async_noop<
			BOOST_ASIO_HANDLER_TYPE(
				NoopHandler,
				void(boost::beast::error_code)
			)
		>{
			std::move(init.completion_handler),
			*this
		}();

		return init.result.get();
	}
}
