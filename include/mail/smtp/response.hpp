#pragma once

#include <vector>
#include <string>

namespace mail::smtp {
	enum class reply_code : unsigned
	{
		unknown								=	0,

		system_status_or_help				=	211,
		help_message						=	214,
		service_ready						=	220,
		service_closing						=	221,
		authentication_succeeded			=	235,
		completed							=	250,
		forward_to							=	251,
		cannot_vrfy_but_accept				=	252,

		authentication_continue				=	334,
		start_mail_input					=	354,

		service_not_available				=	421,
		need_password_transition			=	432,
		mailbox_busy_or_blocked				=	450,
		local_error_in_processing			=	451,
		insufficient_system_storage			=	452,
		temp_authentication_failure			=	454,
		unable_to_accommodate_param			=	455,

		command_unrecognized				=	500,
		parameter_syntax_error				=	501,
		command_not_implemented				=	502,
		bad_command_sequence				=	503,
		parameter_not_implemented			=	504,
		authentication_required				=	530,
		authentication_mechanism_too_weak	=	534,
		authentication_credentials_invalid	=	535,
		encryption_required					=	538,
		mailbox_not_found					=	550,
		user_not_local						=	551,
		exceeded_storage_allocation			=	552,
		mailbox_name_not_allowed			=	553,
		transaction_failed					=	554,
		mail_from_or_rcpt_to_param			=	555,
	};

	class response
	{
	public:
		response() = default;
		response(response&&) = default;
		response(const response&) = default;
		response& operator=(response&&) = default;
		response& operator=(const response&) = default;

		reply_code code() const
		{
			return code_;
		}
		void code(reply_code v)
		{
			code_ = v;
		}
		void code(unsigned v)
		{
			code_ = reply_code{ v };
		}
		unsigned code_int() const
		{
			return static_cast<unsigned>(code_);
		}

		const std::vector<std::string>& lines() const
		{
			return lines_;
		}
		void push_line(const std::string& line)
		{
			lines_.push_back(line);
		}
		void push_line(std::string&& line)
		{
			lines_.push_back(std::move(line));
		}
		void clear()
		{
			lines_.clear();
		}
	private:
		reply_code code_ = reply_code::completed;
		std::vector<std::string> lines_;
	};
}
