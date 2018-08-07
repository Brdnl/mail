#pragma once

#include <string_view>

namespace mail::mime {
	enum class media_type {
		unknown = 0,

		application,
		audio,
		example,
		front,
		image,
		message,
		model,
		multipart,
		text,
		video,
	};

	constexpr std::string_view to_string(media_type mt)
	{
		using std::string_view_literals::operator""sv;
		switch (mt) {
			default:
			case media_type::unknown:
				return "<unknown>"sv;
			case media_type::application:
				return "application"sv;
			case media_type::audio:
				return "audio"sv;
			case media_type::example:
				return "example"sv;
			case media_type::front:
				return "front"sv;
			case media_type::image:
				return "image"sv;
			case media_type::message:
				return "message"sv;
			case media_type::model:
				return "model"sv;
			case media_type::multipart:
				return "multipart"sv;
			case media_type::text:
				return "text"sv;
			case media_type::video:
				return "video"sv;
		}
	}
	constexpr media_type string_to_media_type(std::string_view sv)
	{
		using std::string_view_literals::operator""sv;
		if (sv.size() < 2) {
			return media_type::unknown;
		}
		auto c = sv[0];
		sv.remove_prefix(1);
		switch (c){
			case 'a':
				c = sv[0];
				sv.remove_prefix(1);
				switch (c) {
					case 'p':
						if (sv == "plication"sv) {
							return media_type::application;
						}
						break;
					case 'u':
						if (sv == "dio"sv) {
							return media_type::audio;
						}
						break;
					default:
						break;
				}
				break;
			case 'e':
				if (sv == "xample"sv) {
					return media_type::example;
				}
				break;
			case 'f':
				if (sv == "ront"sv) {
					return media_type::front;
				}
				break;
			case 'i':
				if (sv == "mage"sv) {
					return media_type::image;
				}
				break;
			case 'm':
				c = sv[0];
				sv.remove_prefix(1);
				switch (c) {
					case 'e':
						if (sv == "ssage"sv) {
							return media_type::message;
						}
						break;
					case 'o':
						if (sv == "del"sv) {
							return media_type::model;
						}
						break;
					case 'u':
						if (sv == "ltipart"sv) {
							return media_type::multipart;
						}
						break;
					default:
						break;
				}
				break;
			case 't':
				if (sv == "ext"sv) {
					return media_type::text;
				}
				break;
			case 'v':
				if (sv == "ideo"sv) {
					return media_type::video;
				}
				break;
			default:
				break;
		}
		return media_type::unknown;
	}
}
