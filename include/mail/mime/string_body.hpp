#pragma once

#include <boost/beast/http/string_body.hpp>
#include "http_body_wrapper.hpp"

namespace mail::mime {
	template<class CharT, class Traits = std::char_traits<CharT>, class Allocator = std::allocator<CharT>>
	using basic_string_body = http_body_wrapper<boost::beast::http::basic_string_body<CharT, Traits, Allocator>>;

	using string_body = basic_string_body<char>;
}
