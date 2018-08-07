#pragma once

#include "../session.hpp"
#include <boost/beast/core/detail/base64.hpp>
#include <boost/beast/core/handler_ptr.hpp>
#include <boost/beast/core/type_traits.hpp>
#include <boost/asio/associated_allocator.hpp>
#include <boost/asio/associated_executor.hpp>
#include <boost/asio/coroutine.hpp>
#include <boost/asio/handler_continuation_hook.hpp>
#include <boost/asio/write.hpp>

namespace mail::smtp {
	namespace detail {
		inline auto auth_login_buffer()
		{
			return boost::asio::const_buffer{ "AUTH LOGIN\r\n", 12 };
		}

		inline std::string base64_encode(boost::beast::string_view sv)
		{
			std::string dest;
			dest.resize(boost::beast::detail::base64::encoded_size(sv.size()));
			dest.resize(boost::beast::detail::base64::encode(&dest[0], sv.data(), sv.size()));
			return dest;
		}
	}
	template <class Stream>
	template <class Handler>
	class session<Stream>::auth_login_op
		: public boost::asio::coroutine {
	private:
		struct data
		{
			session<Stream>& s;
			std::string b64_username;
			std::string b64_password;

			data(const Handler&, session<Stream>& s_,
				 boost::beast::string_view username,
				 boost::beast::string_view password)
				: s(s_)
				, b64_username(detail::base64_encode(username) + "\r\n")
				, b64_password(detail::base64_encode(password) + "\r\n")
			{
			}
		};
		boost::beast::handler_ptr<data, Handler> d_;
	public:
		auth_login_op(auth_login_op&&) = default;
		auth_login_op(const auth_login_op&) = delete;

		template <class DeducedHandler, class... Args>
		auth_login_op(DeducedHandler&& h,
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

		friend bool asio_handler_is_continuation(auth_login_op* op)
		{
			using boost::asio::asio_handler_is_continuation;
			return asio_handler_is_continuation(std::addressof(op->d_.handler()));
		}
	};
	template <class Stream>
	template <class Handler>
	void session<Stream>::auth_login_op<Handler>::operator()(boost::beast::error_code ec, std::size_t bytes)
	{
		auto& d = *d_;
		BOOST_ASIO_CORO_REENTER(*this) {
			BOOST_ASIO_CORO_YIELD
				boost::asio::async_write(d.s.s_, detail::auth_login_buffer(), std::move(*this));
			if (ec) {
				goto upcall;
			}
			BOOST_ASIO_CORO_YIELD d.s.async_read_resp(std::move(*this));
			if (ec) {
				goto upcall;
			}
			if (d.s.resp_parser_.get().code() != reply_code::authentication_continue) {
				ec = error::failed;
				goto upcall;
			}
			BOOST_ASIO_CORO_YIELD
				boost::asio::async_write(d.s.s_, boost::asio::buffer(d.b64_username), std::move(*this));
			if (ec) {
				goto upcall;
			}
			BOOST_ASIO_CORO_YIELD d.s.async_read_resp(std::move(*this));
			if (ec) {
				goto upcall;
			}
			if (d.s.resp_parser_.get().code() != reply_code::authentication_continue) {
				ec = error::failed;
				goto upcall;
			}
			BOOST_ASIO_CORO_YIELD
				boost::asio::async_write(d.s.s_, boost::asio::buffer(d.b64_password), std::move(*this));
			if (ec) {
				goto upcall;
			}
			BOOST_ASIO_CORO_YIELD d.s.async_read_resp(std::move(*this));
			if (ec) {
				goto upcall;
			}
			if (d.s.resp_parser_.get().code() != reply_code::authentication_succeeded) {
				ec = error::failed;
				goto upcall;
			}
		upcall:
			d_.invoke(ec);
		}
	}

	template<class Stream>
	void session<Stream>::auth_login(boost::beast::string_view username,
									 boost::beast::string_view password)
	{
		static_assert(boost::beast::is_sync_stream<next_layer_type>::value,
					  "SyncStream requirements not met");

		boost::beast::error_code ec;
		auth_login(username, password, ec);
		if (ec)
			BOOST_THROW_EXCEPTION(boost::beast::system_error{ ec });
	}
	template<class Stream>
	void session<Stream>::auth_login(boost::beast::string_view username,
									 boost::beast::string_view password,
									 boost::beast::error_code& ec)
	{
		static_assert(boost::beast::is_sync_stream<next_layer_type>::value,
					  "SyncStream requirements not met");

		boost::asio::write(s_, detail::auth_login_buffer(), ec);
		if (ec) {
			return;
		}
		read_resp(ec);
		if (ec) {
			return;
		}
		if (resp_parser_.get().code() != reply_code::authentication_continue) {
			ec = error::failed;
			return;
		}
		auto b64_username = detail::base64_encode(username);
		b64_username += "\r\n";
		boost::asio::write(s_, boost::asio::buffer(b64_username), ec);
		if (ec) {
			return;
		}
		read_resp(ec);
		if (ec) {
			return;
		}
		if (resp_parser_.get().code() != reply_code::authentication_continue) {
			ec = error::failed;
			return;
		}
		auto b64_password = detail::base64_encode(password);
		b64_password += "\r\n";
		boost::asio::write(s_, boost::asio::buffer(b64_password), ec);
		if (ec) {
			return;
		}
		read_resp(ec);
		if (ec) {
			return;
		}
		if (resp_parser_.get().code() != reply_code::authentication_succeeded) {
			ec = error::failed;
			return;
		}
	}
	template<class Stream>
	template <class AuthHandler>
	BOOST_ASIO_INITFN_RESULT_TYPE(
		AuthHandler, void(boost::beast::error_code)
	) session<Stream>::async_auth_login(boost::beast::string_view username,
										boost::beast::string_view password,
										AuthHandler&& handler)
	{
		static_assert(boost::beast::is_async_stream<next_layer_type>::value,
					  "AsyncStream requirements not met");

		boost::asio::async_completion<
			AuthHandler,
			void(boost::beast::error_code)> init{ handler };

		auth_login_op<
			BOOST_ASIO_HANDLER_TYPE(
				AuthHandler,
				void(boost::beast::error_code)
			)
		>{
			std::move(init.completion_handler),
			*this,
			username, password
		}();

		return init.result.get();
	}
}
