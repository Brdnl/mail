#pragma once

#include "../response_parser.hpp"

#include "../error.hpp"
#include <boost/asio/buffers_iterator.hpp>

namespace mail::smtp {
	template<class ConstBufferSequence>
	std::size_t response_parser::put(const ConstBufferSequence & buffers, boost::beast::error_code & ec)
	{
		static_assert(boost::asio::is_const_buffer_sequence<ConstBufferSequence>::value,
					  "ConstBufferSequence requirements not met");

		std::size_t bytes_transferred = 0;

		using boost::asio::buffers_begin;
		using boost::asio::buffers_end;
		auto iter = buffers_begin(buffers);
		const auto end = buffers_end(buffers);

		while (true) {
			switch (state_) {
				case state::nothing_yet:
					if (iter == end) {
						ec = error::need_more;
						return bytes_transferred;
					}
					state_ = state::first_code;
					[[fallthrough]];
				case state::first_code:
				case state::repeat_code: {
					if (end - iter < 3) {
						ec = error::need_more;
						return bytes_transferred;
					}
					const auto n1 = *iter++;
					const auto n2 = *iter++;
					const auto n3 = *iter++;
					if (n1 < '2' || n1 > '5' || n2 < '0' || n2 > '9' || n3 < '0' || n3 > '9') {
						ec = error::syntax_error;
						return bytes_transferred;
					}
					const auto code =
						static_cast<unsigned>(n1 - '0') * 100u +
						static_cast<unsigned>(n2 - '0') * 10u +
						static_cast<unsigned>(n3 - '0');
					if (state_ == state::repeat_code && code != resp_.code_int()) {
						ec = error::syntax_error;
						return bytes_transferred;
					}
					resp_.code(code);
					bytes_transferred += 3;
					state_ = state::sep;
				}
				[[fallthrough]];
				case state::sep: {
					if (iter == end) {
						ec = error::need_more;
						return bytes_transferred;
					}
					const auto ch = *iter++;
					if (ch == '-') {
						++bytes_transferred;
						state_ = state::multiline;
					}
					else if (ch == ' ') {
						++bytes_transferred;
						state_ = state::last_line;
					}
					else if (ch == '\r') {
						if (iter == end) {
							ec = error::need_more;
							return bytes_transferred;
						}
						if (*iter != '\n') {
							ec = error::syntax_error;
							return bytes_transferred;
						}
						if (!resp_.lines().empty()) {
							resp_.push_line("");
						}
						bytes_transferred += 2;
						state_ = state::completed;
					}
					continue;
				}
				case state::multiline:
				case state::last_line: {
					std::size_t n = 0;
					auto iter2 = iter;
					for (; iter2 != end; ++iter2, ++n) {
						if (*iter2 > '\x7E' || *iter2 < '\x20' && *iter2 != '\x09') {
							break;
						}
					}
					if (iter2 == end || std::next(iter2) == end) {
						ec = error::need_more;
						return bytes_transferred;
					}
					if (*iter2 != '\r' || *std::next(iter2) != '\n') {
						ec = error::syntax_error;
						return bytes_transferred;
					}

					resp_.push_line(std::string(iter, iter2));

					iter = std::next(iter2, 2);
					bytes_transferred += n + 2;
					state_ = state_ == state::multiline ? state::repeat_code : state::completed;
					continue;
				}
				case state::completed:
					ec.assign(0, ec.category());
					return bytes_transferred;
			}
		}
	}
}
