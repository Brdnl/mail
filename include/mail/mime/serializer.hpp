#pragma once

#include "entity.hpp"
#include <boost/beast/core/detail/config.hpp>
#include <boost/beast/core/buffers_cat.hpp>
#include <boost/beast/core/buffers_prefix.hpp>
#include <boost/beast/core/buffers_suffix.hpp>
#include <boost/beast/core/string.hpp>
#include <boost/beast/core/type_traits.hpp>
#include <boost/beast/core/detail/variant.hpp>
#include <boost/asio/buffer.hpp>

namespace mail::mime {
	using boost::beast::http::is_body;
	//template<class T, class = void>
	//struct is_body_writer : std::false_type {};

	template <typename Body, typename Fields = fields>
	class serializer {
	public:
		static_assert(is_body<Body>::value,
					  "Body requirements not met");

		//static_assert(is_body_writer<Body>::value,
		//			  "BodyWriter requirements not met");

		using value_type = std::conditional_t<
			std::is_constructible_v<typename Body::writer, entity<Body, Fields>&> &&
			!std::is_constructible_v<typename Body::writer, const entity<Body, Fields>&>,
			entity<Body, Fields>,
			const entity<Body, Fields>>;
	private:
		enum
		{
			do_construct        =   0,

			do_init             =  10,
			do_header_only      =  20,
			do_header           =  30,
			do_body             =  40,

			do_complete         = 120
		};

		template<std::size_t, class Visit>
		void do_visit(boost::beast::error_code& ec, Visit& visit);

		using writer = typename Body::writer;

		using cb1_t = boost::beast::buffers_suffix<typename
			Fields::writer::const_buffers_type>;        // header
		using pcb1_t = boost::beast::buffers_prefix_view<cb1_t const&>;

		using cb2_t = boost::beast::buffers_suffix<boost::beast::buffers_cat_view<
			typename Fields::writer::const_buffers_type,// header
			typename writer::const_buffers_type>>;      // body
		using pcb2_t = boost::beast::buffers_prefix_view<cb2_t const&>;

		using cb3_t = boost::beast::buffers_suffix<
			typename writer::const_buffers_type>;       // body
		using pcb3_t = boost::beast::buffers_prefix_view<cb3_t const&>;

		value_type& e_;
		writer wr_;
		boost::optional<typename Fields::writer> fwr_;
		boost::beast::detail::variant<
			cb1_t, cb2_t, cb3_t> v_;
		boost::beast::detail::variant<
			pcb1_t, pcb2_t, pcb3_t> pv_;
		std::size_t limit_ = std::numeric_limits<std::size_t>::max();
		int s_ = do_construct;
		bool split_ = false;
		bool header_done_ = false;
		bool more_;
	public:
		serializer(serializer&&) = default;
		serializer(const serializer&) = default;
		serializer& operator=(const serializer&) = delete;

		explicit serializer(value_type& ent);

		value_type& get()
		{
			return e_;
		}
		std::size_t limit()
		{
			return limit_;
		}
		void limit(std::size_t limit)
		{
			limit_ = limit > 0 ? limit : std::numeric_limits<std::size_t>::max();
		}

		bool split()
		{
			return split_;
		}
		void split(bool v)
		{
			split_ = v;
		}
		bool is_header_done()
		{
			return header_done_;
		}
		bool is_done()
		{
			return s_ == do_complete;
		}

		template<class Visit>
		void next(boost::beast::error_code& ec, Visit&& visit);

		void consume(std::size_t n);

		writer& writer_impl()
		{
			return wr_;
		}
	};
}

#include "impl/serializer.inl"
