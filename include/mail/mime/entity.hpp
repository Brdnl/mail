#pragma once

#include "fields.hpp"
#include <boost/core/empty_value.hpp>

namespace mail::mime {
	template <typename Fields = fields>
	struct header : Fields {};

	template <typename Body, typename Fields = fields>
	class entity
		: public header<Fields>
		, private boost::empty_value<typename Body::value_type> {
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

		const header_type& base() const
		{
			return *this;
		}
		
		header_type& base()
		{
			return *this;
		}

		typename body_type::value_type& body() & noexcept
		{
			return boost::empty_value<typename Body::value_type>::get();
		}

		typename body_type::value_type&& body() && noexcept
		{
			return std::move(boost::empty_value<typename Body::value_type>::get());
		}

		const typename body_type::value_type& body() const& noexcept
		{
			return boost::empty_value<typename Body::value_type>::get();
		}
	};
}
