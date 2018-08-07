#pragma once

namespace mail::smtp {
	enum class error {

		failed = 1,


		need_more,

		buffer_overflow,

		syntax_error,
	};
}

#include "impl/error.inl"
