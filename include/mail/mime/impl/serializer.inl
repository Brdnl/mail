#pragma once

#include "../serializer.hpp"
#include <boost/beast/core/detail/buffers_ref.hpp>
#include <boost/beast/core/detail/config.hpp>
#include <boost/assert.hpp>

namespace mail::mime {
	template <typename Body, typename Fields>
	template<std::size_t I, class Visit>
	void serializer<Body, Fields>::do_visit(boost::beast::error_code& ec, Visit& visit)
	{
		pv_.template emplace<I>(limit_, v_.template get<I>());
		visit(ec, boost::beast::detail::make_buffers_ref(
			pv_.template get<I>()));
	}

	template <typename Body, typename Fields>
	serializer<Body, Fields>::serializer(value_type& e)
		: e_(e)
		, wr_(e, e.body())
	{
	}
	template <typename Body, typename Fields>
	template <class Visit>
	void serializer<Body, Fields>::next(boost::beast::error_code& ec, Visit&& visit)
	{
		switch (s_) {
			case do_construct: {
				fwr_.emplace(e_);
				s_ = do_init;
				BOOST_FALLTHROUGH;
			}
			case do_init: {
				wr_.init(ec);
				if (ec) {
					return;
				}
				if (split_) {
					goto go_header_only;
				}
				auto result = wr_.get(ec);
				if (ec == boost::beast::http::error::need_more) {
					goto go_header_only;
				}
				if (ec) {
					return;
				}
				if (!result) {
					goto go_header_only;
				}
				more_ = result->second;
				v_.template emplace<2>(
					boost::in_place_init,
					fwr_->get(),
					result->first);
				s_ = do_header;
				BOOST_FALLTHROUGH;
			}
			case do_header:
				do_visit<2>(ec, visit);
				break;

			go_header_only:
				v_.template emplace<1>(fwr_->get());
				s_ = do_header_only;
				BOOST_FALLTHROUGH;
			case do_header_only:
				do_visit<1>(ec, visit);
				break;

			case do_body:
				s_ = do_body + 1;
				BOOST_FALLTHROUGH;

			case do_body + 1: {
				auto result = wr_.get(ec);
				if (ec) {
					return;
				}
				if (!result) {
					goto go_complete;
				}
				more_ = result->second;
				v_.template emplace<3>(result->first);
				s_ = do_body + 2;
				BOOST_FALLTHROUGH;
			}

			case do_body + 2:
				do_visit<3>(ec, visit);
				break;
			default:
			case do_complete:
				BOOST_ASSERT(false);
				break;

			go_complete:
				s_ = do_complete;
				break;
		}
	}

	template <typename Body, typename Fields>
	void serializer<Body, Fields>::consume(std::size_t n)
	{
		using boost::asio::buffer_size;
		switch (s_) {
			case do_header:
				BOOST_ASSERT(n <= buffer_size(v_.template get<2>()));
				v_.template get<2>().consume(n);
				if (buffer_size(v_.template get<2>()) > 0) {
					break;
				}
				header_done_ = true;
				v_.reset();
				if (!more_) {
					goto go_complete;
				}
				s_ = do_body + 1;
				break;

			case do_header_only:
				BOOST_ASSERT(n <= buffer_size(v_.template get<1>()));
				v_.template get<1>().consume(n);
				if (buffer_size(v_.template get<1>()) > 0) {
					break;
				}
				fwr_ = boost::none;
				header_done_ = true;
				if (!split_) {
					goto go_complete;
				}
				s_ = do_body;
				break;

			case do_body + 2: {
				BOOST_ASSERT(n <= buffer_size(v_.template get<3>()));
				v_.template get<3>().consume(n);
				if (buffer_size(v_.template get<3>()) > 0) {
					break;
				}
				v_.reset();
				if (!more_) {
					goto go_complete;
				}
				s_ = do_body + 1;
				break;
			}

			default:
				BOOST_ASSERT(false);
			case do_complete:
				break;
			go_complete:
				s_ = do_complete;
				break;
		}
	}
}
