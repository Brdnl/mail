#pragma once

#include "fields.hpp"
#include <boost/beast/core/detail/empty_base_optimization.hpp>

namespace mail::mime {
	template <typename Fields = fields>
	struct header : Fields {};

	template <typename Body, typename Fields = fields>
	class entity
		: public header<Fields>
		, private boost::beast::detail::empty_base_optimization<typename Body::value_type> {
	public:
		using header_type = header<Fields>;
		using body_type = Body;

		entity() = default;
		entity(entity&&) = default;
		entity(const entity&) = default;
		entity& operator=(entity&&) = default;
		entity& operator=(const entity&) = default;

		template<class... BodyArgs>
		explicit entity(header_type&& h, BodyArgs&&... body_args);
		template<class... BodyArgs>
		explicit entity(header_type const& h, BodyArgs&&... body_args);

		typename body_type::value_type& body() & noexcept
		{
			return boost::beast::detail::empty_base_optimization<typename Body::value_type>::member();
		}

		typename body_type::value_type&& body() && noexcept
		{
			return std::move(boost::beast::detail::empty_base_optimization<typename Body::value_type>::member());
		}

		const typename body_type::value_type& body() const& noexcept
		{
			return boost::beast::detail::empty_base_optimization<typename Body::value_type>::member();
		}
	};
}
