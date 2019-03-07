#ifdef _MSC_VER
#pragma warning(disable:4996)
#endif

#include "test_data.h"

#include <mail/smtp/session.hpp>
#include <boost/beast.hpp>
#include <boost/asio.hpp>

#include <boost/asio/ssl.hpp>

using namespace boost::asio;

class client {
public:
	client(io_context& ioc, ssl::context& ctx, mail::mime::entity<mail::mime::string_body> m)
		: session_{ ioc, ctx }
		, mail_{ std::move(m) }
	{
	}

	void start(ip::tcp::resolver::results_type eps)
	{
		async_connect(session_.next_layer().next_layer(), eps,
					  [this](auto ec, auto) {
			if (ec) {
				std::cout << "Connect error: " << ec.message() << "\n";
				return;
			}
			open();
		});
	}
private:
	void open()
	{
		session_.async_open_starttls([this](auto ec) {
			if (ec) {
				std::cout << "Open error: " << ec.message() << "\n";
				return;
			}
			login();
		});
	}
	void login()
	{
		session_.async_auth_login(test_username, test_password,
								  [this](auto ec) {
			if (ec) {
				std::cout << "Login error: " << ec.message() << "\n";
				return;
			}
			send();
		});
	}
	void send()
	{
		session_.async_send_mail(test_user_mailbox,
								 begin(test_recipient_mailboxes), end(test_recipient_mailboxes),
								 mail_,
								 [this](auto ec) {
			if (ec) {
				std::cout << "Send error: " << ec.message() << "\n";
				return;
			}
			close();
		});
	}
	void close()
	{
		session_.async_close([this](auto ec) {
			if (ec) {
				std::cout << "Close error: " << ec.message() << "\n";
				return;
			}
		});
	}

	mail::smtp::session<ssl::stream<ip::tcp::socket>> session_;
	mail::mime::entity<mail::mime::string_body> mail_;
};

int main()
{
	io_context ioc;

	ip::tcp::resolver resolver{ ioc };
	auto eps = resolver.resolve(smtp_server, "smtp");

	ssl::context ctx{ ssl::context_base::tls };

	client c{ ioc, ctx, make_test_mail() };
	c.start(eps);

	ioc.run();
}
