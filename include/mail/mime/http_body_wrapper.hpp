#pragma once

#include "entity.hpp"
#include <boost/beast/http/message.hpp>

namespace mail::mime {
	namespace detail {
		struct hack_http_message {};
	}
}
namespace boost::beast::http {
	template <class Body>
	struct message<true, Body, mail::mime::detail::hack_http_message> {
		explicit message(const typename Body::value_type& body)
			: body_ref_{ body }
		{
		}

		const typename Body::value_type& body() const
		{
			return body_ref_;
		}
	private:
		const typename Body::value_type& body_ref_;
	};
}
namespace mail::mime {
	template <class HttpBody>
	class http_body_wrapper : public HttpBody {
	public:
		using value_type = typename HttpBody::value_type;

		class reader : public HttpBody::reader {
		private:
			template <class Fields>
			static boost::beast::http::header<true, Fields> get_dummy_header()
			{
				static thread_local boost::beast::http::header<true, Fields> r{};
				return r;
			}
		public:
			template <class Fields>
			explicit reader(entity<http_body_wrapper, Fields>& e)
				: HttpBody::reader{ boost::beast::http::message<true, HttpBody, Fields>{
					get_dummy_header<Fields>(), e.body() } }
			{
			}
			template <class Fields>
			reader(header<Fields>&, value_type& b)
				: HttpBody::reader{ get_dummy_header<Fields>(), b }
			{
			}
		};

		class writer : public HttpBody::writer {
		public:
			template <class Fields>
			explicit writer(const entity<http_body_wrapper, Fields>& e)
				: HttpBody::writer{ boost::beast::http::message<true, HttpBody, detail::hack_http_message>(e.body()) }
			{
			}
			template <class Fields>
			writer(const header<Fields>&, const value_type& b)
				: HttpBody::writer{ boost::beast::http::header<true, Fields>{}, b }
			{
			}
		};
	};
}
