#pragma once

#include "response.hpp"
#include "response_parser.hpp"
#include "read_response.hpp"
#include "../mime/entity.hpp"
#include "../mime/serializer.hpp"
#include <boost/beast/core/type_traits.hpp>
#include <boost/beast/core/string.hpp>
#include <boost/beast/core/static_buffer.hpp>
#include <boost/asio/async_result.hpp>

namespace mail::smtp {
	template <class Stream>
	class session {
	public:
		using next_layer_type = std::remove_reference_t<Stream>;
		using executor_type = typename next_layer_type::executor_type;
		using lowest_layer_type = boost::beast::get_lowest_layer<next_layer_type>;

		session(session&& other) = default;
		template <class... Args>
		explicit session(Args&&... args)
			: s_(std::forward<Args>(args)...)
		{
		}
		~session() = default;
		session& operator=(session&&) = default;

		executor_type get_executor() noexcept
		{
			return s_.get_executor();
		}
		next_layer_type& next_layer()
		{
			return s_;
		}
		const next_layer_type& next_layer() const
		{
			return s_;
		}
		lowest_layer_type& lowest_layer()
		{
			return s_.lowest_layer();
		}
		const lowest_layer_type& lowest_layer() const
		{
			return s_.lowest_layer();
		}

		bool is_open() const;

		// <<220
		// >>HELO/EHLO
		// <<250
		void open();
		void open(boost::beast::error_code& ec);
		void open(boost::beast::string_view domain);
		void open(boost::beast::string_view domain, boost::beast::error_code& ec);
		template <class OpenHandler>
		BOOST_ASIO_INITFN_RESULT_TYPE(
			OpenHandler, void(boost::beast::error_code)
		) async_open(OpenHandler&& handler);
		template <class OpenHandler>
		BOOST_ASIO_INITFN_RESULT_TYPE(
			OpenHandler, void(boost::beast::error_code)
		) async_open(boost::beast::string_view domain, OpenHandler&& handler);

		// <<220
		// >>EHLO
		// <<250
		// >>STARTTLS
		// <<220
		// (SSL handshake)
		// >>(SSL)EHLO
		// <<(SSL)250
		void open_starttls();
		void open_starttls(boost::beast::error_code& ec);
		void open_starttls(boost::beast::string_view domain);
		void open_starttls(boost::beast::string_view domain, boost::beast::error_code& ec);
		template <class OpenHandler>
		BOOST_ASIO_INITFN_RESULT_TYPE(
			OpenHandler, void(boost::beast::error_code)
		) async_open_starttls(OpenHandler&& handler);
		template <class OpenHandler>
		BOOST_ASIO_INITFN_RESULT_TYPE(
			OpenHandler, void(boost::beast::error_code)
		) async_open_starttls(boost::beast::string_view domain, OpenHandler&& handler);

		// >>QUIT
		// <<221
		void close();
		void close(boost::beast::error_code& ec);
		template <class CloseHandler>
		BOOST_ASIO_INITFN_RESULT_TYPE(
			CloseHandler, void(boost::beast::error_code)
		) async_close(CloseHandler&& handler);

		// >>NOOP
		// <<250
		void noop();
		void noop(boost::beast::error_code& ec);
		template <class NoopHandler>
		BOOST_ASIO_INITFN_RESULT_TYPE(
			NoopHandler, void(boost::beast::error_code)
		) async_noop(NoopHandler&& handler);

		// >>AUTH LOGIN
		// <<334
		// >>(Base64)username
		// <<334
		// >>(Base64)password
		// <<235
		void auth_login(boost::beast::string_view username,
						boost::beast::string_view password);
		void auth_login(boost::beast::string_view username,
						boost::beast::string_view password,
						boost::beast::error_code& ec);
		template <class AuthHandler>
		BOOST_ASIO_INITFN_RESULT_TYPE(
			AuthHandler, void(boost::beast::error_code)
		) async_auth_login(boost::beast::string_view username,
						   boost::beast::string_view password,
						   AuthHandler&& handler);

		// >>MAIL FROM:<xxx@xx.com>
		// <<250
		// >>RCPT TO:<xxx@xx.com>
		// <<250
		// >>DATA
		// >>xxx
		// >>.
		// <<250
		template <class Body, class Fields>
		void send_mail(boost::beast::string_view from,
					   boost::beast::string_view to,
					   mime::serializer<Body, Fields>& serializer);
		template <class Body, class Fields>
		void send_mail(boost::beast::string_view from,
					   boost::beast::string_view to,
					   mime::serializer<Body, Fields>& serializer,
					   boost::beast::error_code& ec);
		template <class Iterator, class Body, class Fields>
		void send_mail(boost::beast::string_view from,
					   Iterator to_first, Iterator to_last,
					   mime::serializer<Body, Fields>& serializer);
		template <class Iterator, class Body, class Fields>
		void send_mail(boost::beast::string_view from,
					   Iterator to_first, Iterator to_last,
					   mime::serializer<Body, Fields>& serializer,
					   boost::beast::error_code& ec);
		template <class Body, class Fields>
		void send_mail(boost::beast::string_view from,
					   boost::beast::string_view to,
					   const mime::entity<Body, Fields>& entity);
		template <class Body, class Fields>
		void send_mail(boost::beast::string_view from,
					   boost::beast::string_view to,
					   const mime::entity<Body, Fields>& entity,
					   boost::beast::error_code& ec);
		template <class Iterator, class Body, class Fields>
		void send_mail(boost::beast::string_view from,
					   Iterator to_first, Iterator to_last,
					   const mime::entity<Body, Fields>& entity);
		template <class Iterator, class Body, class Fields>
		void send_mail(boost::beast::string_view from,
					   Iterator to_first, Iterator to_last,
					   const mime::entity<Body, Fields>& entity,
					   boost::beast::error_code& ec);
		template <class Body, class Fields, class SendHandler>
		BOOST_ASIO_INITFN_RESULT_TYPE(
			SendHandler, void(boost::beast::error_code)
		) async_send_mail(boost::beast::string_view from,
						  boost::beast::string_view to,
						  mime::serializer<Body, Fields>& serializer,
						  SendHandler&& handler);
		template <class Iterator, class Body, class Fields, class SendHandler>
		BOOST_ASIO_INITFN_RESULT_TYPE(
			SendHandler, void(boost::beast::error_code)
		) async_send_mail(boost::beast::string_view from,
						  Iterator to_first, Iterator to_last,
						  mime::serializer<Body, Fields>& serializer,
						  SendHandler&& handler);
		template <class Body, class Fields, class SendHandler>
		BOOST_ASIO_INITFN_RESULT_TYPE(
			SendHandler, void(boost::beast::error_code)
		) async_send_mail(boost::beast::string_view from,
						  boost::beast::string_view to,
						  const mime::entity<Body, Fields>& entity,
						  SendHandler&& handler);
		template <class Iterator, class Body, class Fields, class SendHandler>
		BOOST_ASIO_INITFN_RESULT_TYPE(
			SendHandler, void(boost::beast::error_code)
		) async_send_mail(boost::beast::string_view from,
						  Iterator to_first, Iterator to_last,
						  const mime::entity<Body, Fields>& entity,
						  SendHandler&& handler);
	private:
		void read_resp(boost::beast::error_code& ec)
		{
			read_response(s_, rd_buf_, resp_parser_, ec);
		}
		template <class Handler>
		BOOST_ASIO_INITFN_RESULT_TYPE(
			Handler, void(boost::beast::error_code)
		) async_read_resp(Handler&& handler)
		{
			return async_read_response(s_, rd_buf_, resp_parser_, std::forward<Handler>(handler));
		}

		template <class> class open_op;
		template <class> class open_starttls_op;
		template <class> class close_op;
		template <class> class noop_op;
		template <class> class auth_login_op;
		template <class, class, class> class send_mail_op;


		//enum class status {
		//	open,
		//	closed,
		//	failed,
		//};

		Stream s_;

		static std::size_t constexpr tcp_frame_size = 1536;
		boost::beast::static_buffer<tcp_frame_size> rd_buf_;
		response_parser resp_parser_;
	};
}

#include "impl/open.inl"
#include "impl/close.inl"
#include "impl/noop.inl"
#include "impl/auth_login.inl"
#include "impl/send_mail.inl"
