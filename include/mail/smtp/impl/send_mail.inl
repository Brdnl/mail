#pragma once

#include "../session.hpp"
#include <boost/beast/core/handler_ptr.hpp>
#include <boost/beast/core/type_traits.hpp>
#include <boost/asio/associated_allocator.hpp>
#include <boost/asio/associated_executor.hpp>
#include <boost/asio/coroutine.hpp>
#include <boost/asio/handler_continuation_hook.hpp>
#include <boost/asio/write.hpp>
#include <boost/optional/optional.hpp>
#include <vector>

namespace mail::smtp {
	namespace detail {
		inline auto mail_from_buffer(boost::beast::string_view from)
		{
			return boost::beast::buffers_cat(
				boost::asio::const_buffer{ "MAIL FROM:<", 11 },
				boost::asio::buffer(from.data(), from.size()),
				boost::asio::const_buffer{ ">\r\n", 3 }
			);
		}
		inline auto rcpt_to_buffer(boost::beast::string_view to)
		{
			return boost::beast::buffers_cat(
				boost::asio::const_buffer{ "RCPT TO:<", 9 },
				boost::asio::buffer(to.data(), to.size()),
				boost::asio::const_buffer{ ">\r\n", 3 }
			);
		}
		inline auto reset_buffer()
		{
			return boost::asio::const_buffer{ "RSET\r\n", 6 };
		}
		inline auto data_buffer()
		{
			return boost::asio::const_buffer{ "DATA\r\n", 6 };
		}
		inline auto data_end_buffer()
		{
			return boost::asio::const_buffer{ "\r\n.\r\n", 5 };//////// ".\r\n" 3
		}
	}
	template <class Stream>
	template <class Body, class Fields, class Handler>
	class session<Stream>::send_mail_op
		: public boost::asio::coroutine {
	private:
		struct data
		{
			session<Stream>& s;
			std::string from;
			std::vector<std::string> to;
			std::size_t i = 0;
			boost::optional<mime::serializer<Body, Fields>> osr;
			mime::serializer<Body, Fields>* sr;
			boost::beast::error_code ec;

			template <class Iterator>
			data(const Handler&, session<Stream>& s_,
				 boost::beast::string_view from_,
				 Iterator to_first, Iterator to_last,
				 mime::serializer<Body, Fields>& sr_)
				: s(s_)
				, from(from_)
				, to(to_first, to_last)
				, sr(&sr_)
			{
			}
			template <class Iterator>
			data(const Handler&, session<Stream>& s_,
				 boost::beast::string_view from_,
				 Iterator to_first, Iterator to_last,
				 const mime::entity<Body, Fields>& e)
				: s(s_)
				, from(from_)
				, to(to_first, to_last)
			{
				osr.emplace(e);
				sr = &osr.get();
			}
		};
		boost::beast::handler_ptr<data, Handler> d_;
	public:
		send_mail_op(send_mail_op&&) = default;
		send_mail_op(const send_mail_op&) = delete;

		template <class DeducedHandler, class... Args>
		send_mail_op(DeducedHandler&& h,
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

		friend bool asio_handler_is_continuation(send_mail_op* op)
		{
			using boost::asio::asio_handler_is_continuation;
			return asio_handler_is_continuation(std::addressof(op->d_.handler()));
		}
	};
	template <class Stream>
	template <class Body, class Fields, class Handler>
	void session<Stream>::send_mail_op<Body, Fields, Handler>::operator()(boost::beast::error_code ec, std::size_t bytes)
	{
		auto& d = *d_;
		BOOST_ASIO_CORO_REENTER(*this) {
			BOOST_ASIO_CORO_YIELD
				boost::asio::async_write(d.s.s_, detail::mail_from_buffer(d.from), std::move(*this));
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
			for (; d.i != d.to.size(); ++d.i) {
				BOOST_ASIO_CORO_YIELD
					boost::asio::async_write(d.s.s_, detail::rcpt_to_buffer(d.to[d.i]), std::move(*this));
				if (ec) {
					goto send_reset;
				}
				BOOST_ASIO_CORO_YIELD d.s.async_read_resp(std::move(*this));
				if (ec) {
					goto send_reset;
				}
				if (d.s.resp_parser_.get().code() != reply_code::completed) {
					ec = error::failed;
					goto send_reset;
				}
			}
			BOOST_ASIO_CORO_YIELD
				boost::asio::async_write(d.s.s_, detail::data_buffer(), std::move(*this));
			if (ec) {
				goto send_reset;
			}
			BOOST_ASIO_CORO_YIELD d.s.async_read_resp(std::move(*this));
			if (ec) {
				goto send_reset;
			}
			if (d.s.resp_parser_.get().code() != reply_code::start_mail_input) {
				ec = error::failed;
				goto send_reset;
			}

			d.sr->split(false);
			while (!d.sr->is_done()) {
				BOOST_ASIO_CORO_YIELD {
					d.sr->next(ec, [&d, this](boost::beast::error_code& ec, const auto& buffers) {
						ec.assign(0, ec.category());
						d.s.s_.async_write_some(buffers, std::move(*this));
					});
					if (ec) {
						// (lambda not invoked) *this is not moved
						goto send_data_end_and_reset;
					}
				};

				if (ec) {
					goto send_data_end_and_reset;
				}
				if (bytes) {
					d.sr->consume(bytes);
				}
			}

			BOOST_ASIO_CORO_YIELD
				boost::asio::async_write(d.s.s_, detail::data_end_buffer(), std::move(*this));
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
			return;
		send_data_end_and_reset:
			d.ec = ec;
			BOOST_ASIO_CORO_YIELD
				boost::asio::async_write(d.s.s_, detail::data_end_buffer(), std::move(*this));
			if (ec) {
				//d.ec = error::critical_error;
				goto reset_upcall;
			}
			BOOST_ASIO_CORO_YIELD d.s.async_read_resp(std::move(*this));
			if (ec) {
				//d.ec = error::critical_error;
				goto reset_upcall;
			}
			ec = d.ec;
		send_reset:
			d.ec = ec;
			BOOST_ASIO_CORO_YIELD
				boost::asio::async_write(d.s.s_, detail::reset_buffer(), std::move(*this));
			if (ec) {
				//d.ec = error::critical_error;
				goto reset_upcall;
			}
			BOOST_ASIO_CORO_YIELD d.s.async_read_resp(std::move(*this));
			if (ec) {
				//d.ec = error::critical_error;
				goto reset_upcall;
			}
			if (d.s.resp_parser_.get().code() != reply_code::completed) {
				//d.ec = error::critical_error;
				goto reset_upcall;
			}
		reset_upcall:
			d_.invoke(d.ec);
		}
	}

	template <class Stream>
	template <class Body, class Fields>
	void session<Stream>::send_mail(boost::beast::string_view from,
									boost::beast::string_view to,
									mime::serializer<Body, Fields>& serializer)
	{
		static_assert(boost::beast::is_sync_stream<next_layer_type>::value,
					  "SyncStream requirements not met");

		boost::beast::error_code ec;
		send_mail(from, to, serializer, ec);
		if (ec)
			BOOST_THROW_EXCEPTION(boost::beast::system_error{ ec });
	}
	template <class Stream>
	template <class Body, class Fields>
	void session<Stream>::send_mail(boost::beast::string_view from,
									boost::beast::string_view to,
									mime::serializer<Body, Fields>& serializer,
									boost::beast::error_code& ec)
	{
		static_assert(boost::beast::is_sync_stream<next_layer_type>::value,
					  "SyncStream requirements not met");

		send_mail(from, &to, &to + 1, serializer, ec);
	}
	template <class Stream>
	template <class Iterator, class Body, class Fields>
	void session<Stream>::send_mail(boost::beast::string_view from,
									Iterator to_first, Iterator to_last,
									mime::serializer<Body, Fields>& serializer)
	{
		static_assert(boost::beast::is_sync_stream<next_layer_type>::value,
					  "SyncStream requirements not met");

		boost::beast::error_code ec;
		send_mail(from, to_first, to_last, serializer, ec);
		if (ec)
			BOOST_THROW_EXCEPTION(boost::beast::system_error{ ec });
	}
	template <class Stream>
	template <class Iterator, class Body, class Fields>
	void session<Stream>::send_mail(boost::beast::string_view from,
									Iterator to_first, Iterator to_last,
									mime::serializer<Body, Fields>& serializer,
									boost::beast::error_code& ec)
	{
		static_assert(boost::beast::is_sync_stream<next_layer_type>::value,
					  "SyncStream requirements not met");

		boost::asio::write(s_, detail::mail_from_buffer(from), ec);
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
		{
			for (auto iter = to_first; iter != to_last; ++iter) {
				boost::asio::write(s_, detail::rcpt_to_buffer(*iter), ec);
				if (ec) {
					goto send_reset;
				}
				read_resp(ec);
				if (ec) {
					goto send_reset;
				}
				if (resp_parser_.get().code() != reply_code::completed) {
					ec = error::failed;
					goto send_reset;
				}
			}

			boost::asio::write(s_, detail::data_buffer(), ec);
			if (ec) {
				goto send_reset;
			}
			read_resp(ec);
			if (ec) {
				goto send_reset;
			}
			if (resp_parser_.get().code() != reply_code::start_mail_input) {
				ec = error::failed;
				goto send_reset;
			}

			serializer.split(false);
			while (!serializer.is_done()) {
				std::size_t bytes = 0;
				serializer.next(ec, [&s = s_, &bytes](boost::beast::error_code& ec, const auto& buffers) {
					bytes = s.write_some(buffers, ec);
				});
				if (ec) {
					goto send_data_end_and_reset;
				}
				if (bytes) {
					serializer.consume(bytes);
				}
			}

			boost::asio::write(s_, detail::data_end_buffer(), ec);
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
		return;
	send_data_end_and_reset:
		{
			boost::beast::error_code ec_send_end;
			boost::asio::write(s_, detail::data_end_buffer(), ec_send_end);
			if (ec_send_end) {
				//ec = error::critical_error;
				return;
			}
			read_resp(ec_send_end);
			if (ec_send_end) {
				//ec = error::critical_error;
				return;
			}
		}
	send_reset:
		boost::beast::error_code ec_reset;
		boost::asio::write(s_, detail::reset_buffer(), ec_reset);
		if (ec_reset) {
			//ec = error::critical_error;
			return;
		}
		read_resp(ec_reset);
		if (ec_reset) {
			//ec = error::critical_error;
			return;
		}
		if (resp_parser_.get().code() != reply_code::completed) {
			//ec = error::critical_error;
			return;
		}
		return;
	}
	template <class Stream>
	template <class Body, class Fields>
	void session<Stream>::send_mail(boost::beast::string_view from,
									boost::beast::string_view to,
									const mime::entity<Body, Fields>& entity)
	{
		static_assert(boost::beast::is_sync_stream<next_layer_type>::value,
					  "SyncStream requirements not met");

		boost::beast::error_code ec;
		send_mail(from, to, entity, ec);
		if (ec)
			BOOST_THROW_EXCEPTION(boost::beast::system_error{ ec });
	}
	template <class Stream>
	template <class Body, class Fields>
	void session<Stream>::send_mail(boost::beast::string_view from,
									boost::beast::string_view to,
									const mime::entity<Body, Fields>& entity,
									boost::beast::error_code& ec)
	{
		static_assert(boost::beast::is_sync_stream<next_layer_type>::value,
					  "SyncStream requirements not met");

		mime::serializer<Body, Fields> sr{ entity };
		send_mail(from, to, sr, ec);
	}
	template <class Stream>
	template <class Iterator, class Body, class Fields>
	void session<Stream>::send_mail(boost::beast::string_view from,
									Iterator to_first, Iterator to_last,
									const mime::entity<Body, Fields>& entity)
	{
		static_assert(boost::beast::is_sync_stream<next_layer_type>::value,
					  "SyncStream requirements not met");

		boost::beast::error_code ec;
		send_mail(from, to_first, to_last, entity, ec);
		if (ec)
			BOOST_THROW_EXCEPTION(boost::beast::system_error{ ec });
	}
	template <class Stream>
	template <class Iterator, class Body, class Fields>
	void session<Stream>::send_mail(boost::beast::string_view from,
									Iterator to_first, Iterator to_last,
									const mime::entity<Body, Fields>& entity,
									boost::beast::error_code& ec)
	{
		static_assert(boost::beast::is_sync_stream<next_layer_type>::value,
					  "SyncStream requirements not met");

		mime::serializer<Body, Fields> sr{ entity };
		send_mail(from, to_first, to_last, sr, ec);
	}

	template <class Stream>
	template <class Body, class Fields, class SendHandler>
	BOOST_ASIO_INITFN_RESULT_TYPE(
		SendHandler, void(boost::beast::error_code)
	) session<Stream>::async_send_mail(boost::beast::string_view from,
									   boost::beast::string_view to,
									   mime::serializer<Body, Fields>& serializer,
									   SendHandler&& handler)
	{
		static_assert(boost::beast::is_async_stream<next_layer_type>::value,
					  "AsyncStream requirements not met");

		return async_send_mail(from, &to, &to + 1, serializer, std::forward<SendHandler>(handler));
	}
	template <class Stream>
	template <class Iterator, class Body, class Fields, class SendHandler>
	BOOST_ASIO_INITFN_RESULT_TYPE(
		SendHandler, void(boost::beast::error_code)
	) session<Stream>::async_send_mail(boost::beast::string_view from,
									   Iterator to_first, Iterator to_last,
									   mime::serializer<Body, Fields>& serializer,
									   SendHandler&& handler)
	{
		static_assert(boost::beast::is_async_stream<next_layer_type>::value,
					  "AsyncStream requirements not met");

		boost::asio::async_completion<
			SendHandler,
			void(boost::beast::error_code)> init{ handler };

		send_mail_op<
			Body, Fields,
			BOOST_ASIO_HANDLER_TYPE(
				SendHandler,
				void(boost::beast::error_code)
			)
		>{
			std::move(init.completion_handler),
			*this,
			from,
			to_first, to_last,
			serializer
		}();

		return init.result.get();
	}
	template <class Stream>
	template <class Body, class Fields, class SendHandler>
	BOOST_ASIO_INITFN_RESULT_TYPE(
		SendHandler, void(boost::beast::error_code)
	) session<Stream>::async_send_mail(boost::beast::string_view from,
									   boost::beast::string_view to,
									   const mime::entity<Body, Fields>& entity,
									   SendHandler&& handler)
	{
		static_assert(boost::beast::is_async_stream<next_layer_type>::value,
					  "AsyncStream requirements not met");

		return async_send_mail(from, &to, &to + 1, entity, std::forward<SendHandler>(handler));
	}
	template <class Stream>
	template <class Iterator, class Body, class Fields, class SendHandler>
	BOOST_ASIO_INITFN_RESULT_TYPE(
		SendHandler, void(boost::beast::error_code)
	) session<Stream>::async_send_mail(boost::beast::string_view from,
									   Iterator to_first, Iterator to_last,
									   const mime::entity<Body, Fields>& entity,
									   SendHandler&& handler)
	{
		static_assert(boost::beast::is_async_stream<next_layer_type>::value,
					  "AsyncStream requirements not met");
		
		boost::asio::async_completion<
			SendHandler,
			void(boost::beast::error_code)> init{ handler };

		send_mail_op<
			Body, Fields,
			BOOST_ASIO_HANDLER_TYPE(
				SendHandler,
				void(boost::beast::error_code)
			)
		>{
			std::move(init.completion_handler),
			*this,
			from,
			to_first, to_last,
			entity
		}();

		return init.result.get();
	}
}
