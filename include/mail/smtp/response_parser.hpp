#pragma once

#include "response.hpp"
#include <boost/beast/core/error.hpp>

namespace mail::smtp {
	class response_parser
	{
	public:
		response_parser() = default;
		explicit response_parser(response&& resp)
			: resp_{ std::move(resp) }
		{
			resp_.clear();
		}

		const response& get() const
		{
			return resp_;
		}
		response& get()
		{
			return resp_;
		}
		response release()
		{
			return std::move(resp_);
		}

		void reset()
		{
			state_ = state::nothing_yet;
			resp_.clear();
		}

		template<class ConstBufferSequence>
		std::size_t put(const ConstBufferSequence& buffers, boost::beast::error_code& ec);
	private:
		enum class state {
			nothing_yet,
			first_code,
			sep,
			multiline,
			repeat_code,
			last_line,
			completed,
		};
		state state_ = state::nothing_yet;
		response resp_;
	};
}

#include "impl/response_parser.inl"
