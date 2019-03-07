#pragma once

#include "response_parser.hpp"
#include <boost/asio/async_result.hpp>
#include <boost/beast/core/error.hpp>

namespace mail::smtp {
	template <class Stream, class DynamicBuffer>
	void read_response(Stream& s, DynamicBuffer& buffer, response_parser& parser, boost::beast::error_code& ec);

	template <class Stream, class DynamicBuffer, class Handler>
	BOOST_ASIO_INITFN_RESULT_TYPE(
		Handler, void(boost::beast::error_code)
	) async_read_response(Stream& s, DynamicBuffer& buffer, response_parser& parser, Handler&& handler);
}

#include "impl/read_response.inl"
