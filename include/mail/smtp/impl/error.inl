#pragma once

#include "../error.hpp"
#include <boost/beast/core/error.hpp>

namespace boost {
	namespace system {
		template<>
		struct is_error_code_enum<mail::smtp::error>
		{
			static bool const value = true;
		};
	}
}

namespace mail::smtp {
	namespace detail {
		class smtp_error_category : public boost::beast::error_category
		{
		public:
			const char* name() const noexcept override
			{
				return "mail.smtp";
			}

			std::string message(int ev) const override
			{
				switch (static_cast<error>(ev)) {
					case error::need_more: return "need more";
					case error::buffer_overflow: return "buffer overflow";
					case error::syntax_error: return "syntax error";

					default:
						return "mail.smtp error";
				}
			}

			boost::beast::error_condition default_error_condition(int ev) const noexcept override
			{
				return boost::beast::error_condition{ ev, *this };
			}

			bool equivalent(int ev, const boost::beast::error_condition& condition) const noexcept override
			{
				return condition.value() == ev && &condition.category() == this;
			}
			bool equivalent(const boost::beast::error_code& error, int ev) const noexcept override
			{
				return error.value() == ev && &error.category() == this;
			}
		};
		inline const boost::beast::error_category& get_smtp_error_category()
		{
			static const smtp_error_category cat{};
			return cat;
		}
	}
	inline boost::beast::error_code make_error_code(error ev)
	{
		return boost::beast::error_code{
			static_cast<std::underlying_type<error>::type>(ev),
			detail::get_smtp_error_category()
		};
	}
}
