#pragma once

#include <boost/beast/http/fields.hpp>

namespace mail::mime {
	using boost::beast::http::field;

	template<class Allocator>
	using basic_fields = boost::beast::http::basic_fields<Allocator>;

	using fields = basic_fields<std::allocator<char>>;
}
