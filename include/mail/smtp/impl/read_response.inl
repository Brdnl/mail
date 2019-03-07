#pragma once

#include "../read_response.hpp"
#include <boost/beast/core/handler_ptr.hpp>
#include <boost/beast/core/type_traits.hpp>
#include <boost/asio/associated_allocator.hpp>
#include <boost/asio/associated_executor.hpp>
#include <boost/asio/handler_continuation_hook.hpp>
#include <boost/beast/core/read_size.hpp>

namespace mail::smtp {
	namespace detail {
		template <class Stream, class DynamicBuffer, class Handler>
		class read_response_op {
			struct data {
				int state = 0;
				Stream& s;
				DynamicBuffer& buf;
				response_parser& parser;

				data(const Handler&, Stream& s_, DynamicBuffer& buf_, response_parser& parser_)
					: s(s_)
					, buf(buf_)
					, parser(parser_)
				{
				}
			};
			boost::beast::handler_ptr<data, Handler> d_;
		public:
			read_response_op(read_response_op&&) = default;
			read_response_op(const read_response_op&) = delete;

			template <class DeducedHandler, class... Args>
			read_response_op(DeducedHandler&& handler,
							 Stream& s,
							 DynamicBuffer& buf,
							 response_parser& parser,
							 Args&&... args)
				: d_(std::forward<DeducedHandler>(handler),
					 s,
					 buf,
					 parser,
					 std::forward<Args>(args)...)
			{
			}

			using allocator_type = boost::asio::associated_allocator_t<Handler>;

			allocator_type get_allocator() const noexcept
			{
				return boost::asio::get_associated_allocator(d_.handler());
			}

			using executor_type = boost::asio::associated_executor_t<
				Handler, decltype(std::declval<Stream&>().get_executor())>;

			executor_type get_executor() const noexcept
			{
				return boost::asio::get_associated_executor(
					d_.handler(), d_->s.get_executor());
			}

			void operator()(boost::beast::error_code ec = {}, std::size_t bytes_transferred = 0);

			friend bool asio_handler_is_continuation(read_response_op* op)
			{
				using boost::asio::asio_handler_is_continuation;
				return op->d_->state >= 2 ||
					asio_handler_is_continuation(
						std::addressof(op->d_.handler()));
			}
		};
		template <class Stream, class DynamicBuffer, class Handler>
		void read_response_op<Stream, DynamicBuffer, Handler>::operator()(
			boost::beast::error_code ec, std::size_t bytes_transferred)
		{
			auto& d = *d_;

			switch (d.state) {
				case 0:
					d.state = 1;
					if (d.buf.size() == 0) {
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
					d.buf.commit(bytes_transferred);
					const auto n = d.parser.put(d.buf.data(), ec);
					d.buf.consume(n);
					if (ec != error::need_more) {
						return d_.invoke(ec);
					}
				}
				do_read:
					try {
						auto b = d.buf.prepare(boost::beast::read_size_or_throw(d.buf, 65536));
						return d.s.async_read_some(b, std::move(*this));
					}
					catch (const std::length_error&) {
						ec = error::buffer_overflow;
						return d_.invoke(ec);
					}
			}
		}
	}

	template <class Stream, class DynamicBuffer>
	void read_response(Stream& s, DynamicBuffer& buffer, response_parser& parser, boost::beast::error_code& ec)
	{
		static_assert(boost::beast::is_sync_read_stream<Stream>::value,
					  "SyncReadStream requirements not met");
		static_assert(boost::asio::is_dynamic_buffer<DynamicBuffer>::value,
					  "DynamicBuffer requirements not met");

		parser.reset();

		if (buffer.size() == 0) {
			goto do_read;
		}
		while (true) {
			{
				const auto n = parser.put(buffer.data(), ec);
				buffer.consume(n);
				if (ec != error::need_more) {
					return;
				}
			}
		do_read:
			try {
				auto b = buffer.prepare(boost::beast::read_size_or_throw(buffer, 65536));
				const auto n = s.read_some(b, ec);
				if (ec) {
					return;
				}
				buffer.commit(n);
			}
			catch (const std::length_error&) {
				ec = error::buffer_overflow;
				return;
			}
		}
	}

	template <class Stream, class DynamicBuffer, class Handler>
	BOOST_ASIO_INITFN_RESULT_TYPE(
		Handler, void(boost::beast::error_code)
	) async_read_response(Stream& s, DynamicBuffer& buffer, response_parser& parser, Handler&& handler)
	{
		static_assert(boost::beast::is_async_read_stream<Stream>::value,
					  "AsyncReadStream requirements not met");
		static_assert(boost::asio::is_dynamic_buffer<DynamicBuffer>::value,
					  "DynamicBuffer requirements not met");

		parser.reset();

		boost::asio::async_completion<
			Handler,
			void(boost::beast::error_code)> init{ handler };

		detail::read_response_op<
			Stream,
			DynamicBuffer,
			BOOST_ASIO_HANDLER_TYPE(
				Handler,
				void(boost::beast::error_code)
			)
		>{
			std::move(init.completion_handler),
			s,
			buffer,
			parser
		}();

		return init.result.get();
	}
}
