#pragma once

#include "../session.hpp"
#include <boost/beast/core/string.hpp>
#include <boost/beast/core/handler_ptr.hpp>
#include <boost/beast/core/type_traits.hpp>
#include <boost/asio/associated_allocator.hpp>
#include <boost/asio/associated_executor.hpp>
#include <boost/asio/coroutine.hpp>
#include <boost/asio/handler_continuation_hook.hpp>
#include <boost/asio/ip/host_name.hpp>
#include <boost/beast/core/buffers_cat.hpp>
#include <boost/asio/write.hpp>

namespace mail::smtp {
	namespace detail {
		inline auto ehlo_buffer(boost::beast::string_view domain)
		{
			return boost::beast::buffers_cat(
				boost::asio::const_buffer{ "EHLO ", 5 },
				boost::asio::buffer(domain.data(), domain.size()),
				boost::asio::const_buffer{ "\r\n", 2 }
			);
		}
		inline auto helo_buffer(boost::beast::string_view domain)
		{
			return boost::beast::buffers_cat(
				boost::asio::const_buffer{ "HELO ", 5 },
				boost::asio::buffer(domain.data(), domain.size()),
				boost::asio::const_buffer{ "\r\n", 2 }
			);
		}
		inline auto starttls_buffer()
		{
			return boost::asio::const_buffer{ "STARTTLS\r\n", 10 };
		}
	}
	template <class Stream>
	template <class Handler>
	class session<Stream>::open_op
		: public boost::asio::coroutine {
	private:
		struct data
		{
			session<Stream>& s;
			std::string domain;

			data(const Handler&, session<Stream>& s_,
				 const std::string& domain_)
				: s(s_)
				, domain(domain_)
			{
			}
			data(const Handler&, session<Stream>& s_,
				 std::string&& domain_)
				: s(s_)
				, domain(std::move(domain_))
			{
			}
		};
		boost::beast::handler_ptr<data, Handler> d_;
	public:
		open_op(open_op&&) = default;
		open_op(const open_op&) = delete;

		template <class DeducedHandler, class... Args>
		open_op(DeducedHandler&& h,
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

		friend bool asio_handler_is_continuation(open_op* op)
		{
			using boost::asio::asio_handler_is_continuation;
			return asio_handler_is_continuation(std::addressof(op->d_.handler()));
		}
	};
	template <class Stream>
	template <class Handler>
	void session<Stream>::open_op<Handler>::operator()(boost::beast::error_code ec, std::size_t bytes)
	{
		auto& d = *d_;
		BOOST_ASIO_CORO_REENTER(*this) {
			BOOST_ASIO_CORO_YIELD d.s.async_read_resp(std::move(*this));
			if (ec) {
				goto upcall;
			}
			if (d.s.resp_parser_.get().code() != reply_code::service_ready) {
				ec = error::failed;
				goto upcall;
			}
			BOOST_ASIO_CORO_YIELD
				boost::asio::async_write(d.s.s_, detail::ehlo_buffer(d.domain), std::move(*this));
			if (ec) {
				goto upcall;
			}
			BOOST_ASIO_CORO_YIELD d.s.async_read_resp(std::move(*this));
			if (ec) {
				goto upcall;
			}
			if (d.s.resp_parser_.get().code() != reply_code::completed) {
				if (d.s.resp_parser_.get().code() != reply_code::command_unrecognized &&
					d.s.resp_parser_.get().code() != reply_code::command_not_implemented) {
					ec = error::failed;
					goto upcall;
				}
				BOOST_ASIO_CORO_YIELD
					boost::asio::async_write(d.s.s_, detail::helo_buffer(d.domain), std::move(*this));
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
			}
			//// parse response lines
		upcall:
			d_.invoke(ec);
		}
	}
	template <class Stream>
	template <class Handler>
	class session<Stream>::open_starttls_op
		: public boost::asio::coroutine {
	private:
		struct data
		{
			session<Stream>& s;
			std::string domain;

			data(const Handler&, session<Stream>& s_,
				 const std::string& domain_)
				: s(s_)
				, domain(domain_)
			{
			}
			data(const Handler&, session<Stream>& s_,
				 std::string&& domain_)
				: s(s_)
				, domain(std::move(domain_))
			{
			}
		};
		boost::beast::handler_ptr<data, Handler> d_;
	public:
		open_starttls_op(open_starttls_op&&) = default;
		open_starttls_op(const open_starttls_op&) = delete;

		template <class DeducedHandler, class... Args>
		open_starttls_op(DeducedHandler&& h,
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

		friend bool asio_handler_is_continuation(open_starttls_op* op)
		{
			using boost::asio::asio_handler_is_continuation;
			return asio_handler_is_continuation(std::addressof(op->d_.handler()));
		}
	};
	template <class Stream>
	template <class Handler>
	void session<Stream>::open_starttls_op<Handler>::operator()(boost::beast::error_code ec, std::size_t bytes)
	{
		auto& d = *d_;
		BOOST_ASIO_CORO_REENTER(*this) {
			BOOST_ASIO_CORO_YIELD async_read_response(d.s.s_.next_layer(), d.s.rd_buf_, d.s.resp_parser_, std::move(*this));
			if (ec) {
				goto upcall;
			}
			if (d.s.resp_parser_.get().code() != reply_code::service_ready) {
				ec = error::failed;
				goto upcall;
			}
			BOOST_ASIO_CORO_YIELD
				boost::asio::async_write(d.s.s_.next_layer(), detail::ehlo_buffer(d.domain), std::move(*this));
			if (ec) {
				goto upcall;
			}
			BOOST_ASIO_CORO_YIELD async_read_response(d.s.s_.next_layer(), d.s.rd_buf_, d.s.resp_parser_, std::move(*this));
			if (ec) {
				goto upcall;
			}
			if (d.s.resp_parser_.get().code() != reply_code::completed) {
				ec = error::failed;
				goto upcall;
			}
			//// parse response lines

			BOOST_ASIO_CORO_YIELD
				boost::asio::async_write(d.s.s_.next_layer(), detail::starttls_buffer(), std::move(*this));
			if (ec) {
				goto upcall;
			}
			BOOST_ASIO_CORO_YIELD async_read_response(d.s.s_.next_layer(), d.s.rd_buf_, d.s.resp_parser_, std::move(*this));
			if (ec) {
				goto upcall;
			}
			if (d.s.resp_parser_.get().code() != reply_code::service_ready) {
				ec = error::failed;
				goto upcall;
			}

			BOOST_ASIO_CORO_YIELD d.s.s_.async_handshake(d.s.s_.client, std::move(*this));
			if (ec) {
				goto upcall;
			}

			BOOST_ASIO_CORO_YIELD
				boost::asio::async_write(d.s.s_, detail::ehlo_buffer(d.domain), std::move(*this));
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
			//// parse response lines
		upcall:
			d_.invoke(ec);
		}
	}

	template<class Stream>
	void session<Stream>::open()
	{
		static_assert(boost::beast::is_sync_stream<next_layer_type>::value,
					  "SyncStream requirements not met");

		open(boost::asio::ip::host_name());
	}
	template<class Stream>
	void session<Stream>::open(boost::beast::error_code & ec)
	{
		static_assert(boost::beast::is_sync_stream<next_layer_type>::value,
					  "SyncStream requirements not met");

		open(boost::asio::ip::host_name(), ec);
	}
	template<class Stream>
	void session<Stream>::open(boost::beast::string_view domain)
	{
		static_assert(boost::beast::is_sync_stream<next_layer_type>::value,
					  "SyncStream requirements not met");

		boost::beast::error_code ec;
		open(domain, ec);
		if (ec)
			BOOST_THROW_EXCEPTION(boost::beast::system_error{ ec });
	}
	template<class Stream>
	void session<Stream>::open(boost::beast::string_view domain, boost::beast::error_code & ec)
	{
		static_assert(boost::beast::is_sync_stream<next_layer_type>::value,
					  "SyncStream requirements not met");

		read_resp(ec);
		if (ec) {
			return;
		}
		if (resp_parser_.get().code() != reply_code::service_ready) {
			ec = error::failed;
			return;
		}
		boost::asio::write(s_, detail::ehlo_buffer(domain), ec);
		if (ec) {
			return;
		}
		read_resp(ec);
		if (ec) {
			return;
		}
		if (resp_parser_.get().code() != reply_code::completed) {
			if (resp_parser_.get().code() != reply_code::command_unrecognized &&
				resp_parser_.get().code() != reply_code::command_not_implemented) {
				ec = error::failed;
				return;
			}
			boost::asio::write(s_, detail::helo_buffer(domain), ec);
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
		//// parse response lines
	}
	template<class Stream>
	template <class OpenHandler>
	BOOST_ASIO_INITFN_RESULT_TYPE(
		OpenHandler, void(boost::beast::error_code)
	) session<Stream>::async_open(OpenHandler && handler)
	{
		static_assert(boost::beast::is_async_stream<next_layer_type>::value,
					  "AsyncStream requirements not met");

		return async_open(boost::asio::ip::host_name(), handler);
	}
	template<class Stream>
	template <class OpenHandler>
	BOOST_ASIO_INITFN_RESULT_TYPE(
		OpenHandler, void(boost::beast::error_code)
	) session<Stream>::async_open(boost::beast::string_view domain, OpenHandler && handler)
	{
		static_assert(boost::beast::is_async_stream<next_layer_type>::value,
					  "AsyncStream requirements not met");

		boost::asio::async_completion<
			OpenHandler,
			void(boost::beast::error_code)> init{ handler };

		open_op<
			BOOST_ASIO_HANDLER_TYPE(
				OpenHandler,
				void(boost::beast::error_code)
			)
		>{
			std::move(init.completion_handler),
			*this,
			domain.to_string()
		}();

		return init.result.get();
	}

	template<class Stream>
	void session<Stream>::open_starttls()
	{
		static_assert(boost::beast::is_sync_stream<next_layer_type>::value,
					  "SyncStream requirements not met");

		open_starttls(boost::asio::ip::host_name());
	}
	template<class Stream>
	void session<Stream>::open_starttls(boost::beast::error_code & ec)
	{
		static_assert(boost::beast::is_sync_stream<next_layer_type>::value,
					  "SyncStream requirements not met");

		open_starttls(boost::asio::ip::host_name(), ec);
	}
	template<class Stream>
	void session<Stream>::open_starttls(boost::beast::string_view domain)
	{
		static_assert(boost::beast::is_sync_stream<next_layer_type>::value,
					  "SyncStream requirements not met");

		boost::beast::error_code ec;
		open_starttls(domain, ec);
		if (ec)
			BOOST_THROW_EXCEPTION(boost::beast::system_error{ ec });
	}
	template<class Stream>
	void session<Stream>::open_starttls(boost::beast::string_view domain, boost::beast::error_code & ec)
	{
		static_assert(boost::beast::is_sync_stream<next_layer_type>::value,
					  "SyncStream requirements not met");

		read_response(s_.next_layer(), rd_buf_, resp_parser_, ec);
		if (ec) {
			return;
		}
		if (resp_parser_.get().code() != reply_code::service_ready) {
			ec = error::failed;
			return;
		}
		boost::asio::write(s_.next_layer(), detail::ehlo_buffer(domain), ec);
		if (ec) {
			return;
		}
		read_response(s_.next_layer(), rd_buf_, resp_parser_, ec);
		if (ec) {
			return;
		}
		if (resp_parser_.get().code() != reply_code::completed) {
			ec = error::failed;
			return;
		}
		//// parse response lines

		boost::asio::write(s_.next_layer(), detail::starttls_buffer(), ec);
		if (ec) {
			return;
		}
		read_response(s_.next_layer(), rd_buf_, resp_parser_, ec);
		if (ec) {
			return;
		}
		if (resp_parser_.get().code() != reply_code::service_ready) {
			ec = error::failed;
			return;
		}

		s_.handshake(s_.client, ec);
		if (ec) {
			return;
		}

		boost::asio::write(s_, detail::ehlo_buffer(domain), ec);
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
		//// parse response lines
	}

	template<class Stream>
	template <class OpenHandler>
	BOOST_ASIO_INITFN_RESULT_TYPE(
		OpenHandler, void(boost::beast::error_code)
	) session<Stream>::async_open_starttls(OpenHandler && handler)
	{
		static_assert(boost::beast::is_async_stream<next_layer_type>::value,
					  "AsyncStream requirements not met");

		return async_open_starttls(boost::asio::ip::host_name(), handler);
	}
	template<class Stream>
	template <class OpenHandler>
	BOOST_ASIO_INITFN_RESULT_TYPE(
		OpenHandler, void(boost::beast::error_code)
	) session<Stream>::async_open_starttls(boost::beast::string_view domain, OpenHandler && handler)
	{
		static_assert(boost::beast::is_async_stream<next_layer_type>::value,
					  "AsyncStream requirements not met");

		boost::asio::async_completion<
			OpenHandler,
			void(boost::beast::error_code)> init{ handler };

		open_starttls_op<
			BOOST_ASIO_HANDLER_TYPE(
				OpenHandler,
				void(boost::beast::error_code)
			)
		>{
			std::move(init.completion_handler),
			*this,
			domain.to_string()
		}();

		return init.result.get();
	}
}
