#pragma once

#include <mail/mime/entity.hpp>
#include <mail/mime/string_body.hpp>
#include <sstream>
#include <iomanip>

constexpr auto smtp_server = "smtp.example.com";
constexpr auto test_username = "test_user";
constexpr auto test_password = "PASSWORD";
constexpr auto test_user_mailbox = "test_user@example.com";
const auto test_recipient_mailboxes = { "test_recipient1@example.com", "test_recipient2@example.com" };

inline std::tm localtime(const std::time_t &time_tt)
{
#ifdef _WIN32
	std::tm tm;
	localtime_s(&tm, &time_tt);
#else
	std::tm tm;
	localtime_r(&time_tt, &tm);
#endif
	return tm;
}
inline std::string date_str()
{
	std::ostringstream ostrm;
	ostrm.imbue(std::locale{ "C" });
	auto t = std::time(nullptr);
	std::tm tm = localtime(t);
	ostrm << std::put_time(&tm, "%a, %e %b %Y %T %z");
	return ostrm.str();
}
inline mail::mime::entity<mail::mime::string_body> make_test_mail()
{
	mail::mime::entity<mail::mime::string_body> e;
	e.set(mail::mime::field::mime_version, "1.0");
	e.set(mail::mime::field::from, test_user_mailbox);
	std::string to;
	for (const auto& mb : test_recipient_mailboxes) {
		if (!to.empty()) {
			to += ", ";
		}
		to += mb;
	}
	e.set(mail::mime::field::to, to);
	e.set(mail::mime::field::date, date_str());
	e.set(mail::mime::field::subject, "TEST");
	e.set(mail::mime::field::content_type, "text/plain; charset=utf-8");
	e.set(mail::mime::field::content_transfer_encoding, "quoted-printable");
	e.body() = "TEST";

	return e;
}

