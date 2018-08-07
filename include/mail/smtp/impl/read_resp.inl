#pragma once

#include "../session.hpp"
#include <boost/beast/core/handler_ptr.hpp>
#include <boost/beast/core/type_traits.hpp>
#include <boost/asio/associated_allocator.hpp>
#include <boost/asio/associated_executor.hpp>
#include <boost/asio/handler_continuation_hook.hpp>
#include <boost/beast/core/read_size.hpp>

namespace mail::smtp {
	template <class Stream>
	template <class Handler>
	class session<Stream>::read_resp_op {
		struct data {
			int state = 0;
			session<Stream>& s;

			data(const Handler&, session<Stream>& s_)
				: s(s_)
			{
			}
		};
		boost::beast::handler_ptr<data, Handler> d_;
	public:
		read_resp_op(read_resp_op&&) = default;
		read_resp_op(const read_resp_op&) = delete;

		template <class DeducedHandler, class... Args>
		read_resp_op(DeducedHandler&& handler,
					 session<Stream>& stream,
					 Args&&... args)
			: d_(std::forward<DeducedHandler>(handler),
				 stream,
				 std::forward<Args>(args)...)
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
			return boost::asio::get_associated_executor(
				d_.handler(), d_->s.get_executor());
		}

		void operator()(boost::beast::error_code ec = {}, std::size_t bytes_transferred = 0);

		friend bool asio_handler_is_continuation(read_resp_op* op)
		{
			using boost::asio::asio_handler_is_continuation;
			return op->d_->state >= 2 ? true :
				asio_handler_is_continuation(
					std::addressof(op->d_.handler()));
		}
	};
	template <class Stream>
	template <class Handler>
	void session<Stream>::read_resp_op<Handler>::operator()(
		boost::beast::error_code ec, std::size_t bytes_transferred)
	{
		auto& d = *d_;

		switch (d.state) {
			case 0:
				d.state = 1;
				if (d.s.rd_buf_.size() == 0) {
					goto do_read;
				}
				[[fallthrough]];
			case 1:
				d.state = 2;
				[[fallthrough]];
			case 2: {
				if (ec) {
					return d_.invoke(ec);
				}
				d.s.rd_buf_.commit(bytes_transferred);
				const auto n = d.s.resp_parser_.put(d.s.rd_buf_.data(), ec);
				d.s.rd_buf_.consume(n);
				if (ec != error::need_more) {
					return d_.invoke(ec);
				}
			}
			do_read:
				try {
					auto b = d.s.rd_buf_.prepare(boost::beast::read_size_or_throw(d.s.rd_buf_, 65536));
					return d.s.s_.async_read_some(b, std::move(*this));
				}
				catch (const std::length_error&) {
					ec = error::buffer_overflow;
					return d_.invoke(ec);
				}
		}
	}

	template <class Stream>
	void session<Stream>::read_resp(boost::beast::error_code& ec)
	{
		static_assert(boost::beast::is_sync_read_stream<next_layer_type>::value,
					  "SyncReadStream requirements not met");

		resp_parser_.reset();

		if (rd_buf_.size() == 0) {
			goto do_read;
		}
		while (true) {
			{
				const auto n = resp_parser_.put(rd_buf_.data(), ec);
				rd_buf_.consume(n);
				if (ec != error::need_more) {
					return;
				}
			}
		do_read:
			try {
				auto b = rd_buf_.prepare(boost::beast::read_size_or_throw(rd_buf_, 65536));
				const auto n = s_.read_some(b, ec);
				if (ec) {
					return;
				}
				rd_buf_.commit(n);
			}
			catch (const std::length_error&) {
				ec = error::buffer_overflow;
				return;
			}
		}
	}
	
	template <class Stream>
	template <class Handler>
	BOOST_ASIO_INITFN_RESULT_TYPE(
		Handler, void(boost::beast::error_code)
	) session<Stream>::async_read_resp(Handler&& handler)
	{
		static_assert(boost::beast::is_async_read_stream<next_layer_type>::value,
					  "AsyncReadStream requirements not met");

		resp_parser_.reset();
		
		boost::asio::async_completion<
			Handler,
			void(boost::beast::error_code)> init{ handler };

		read_resp_op<
			BOOST_ASIO_HANDLER_TYPE(
				Handler,
				void(boost::beast::error_code)
			)
		>{
			std::move(init.completion_handler),
			*this
		}();

		return init.result.get();
	}
}
