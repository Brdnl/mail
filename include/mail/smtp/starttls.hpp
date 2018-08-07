#pragma once

#include "session.hpp"
#include <boost/asio/ssl/stream.hpp>

namespace mail::smtp {
	// >>STARTTLS
	// <<220
	// (SSL handshake)
	// >>(SSL)EHLO
	// <<(SSL)250
	template <class Stream>
	void starttls(session<boost::asio::ssl::stream<Stream>>& session);
	template <class Stream>
	void starttls(session<boost::asio::ssl::stream<Stream>>& session, boost::beast::error_code& ec);
	template <class Stream>
	/*BOOST_ASIO_INITFN_RESULT_TYPE*/void async_starttls(session<Stream>&&, hdlr);
}
