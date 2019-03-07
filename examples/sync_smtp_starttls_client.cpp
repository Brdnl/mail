#ifdef _MSC_VER
#pragma warning(disable:4996)
#endif

#include "test_data.h"

#include <mail/smtp/session.hpp>
#include <boost/beast.hpp>
#include <boost/asio.hpp>

#include <boost/asio/ssl.hpp>

using namespace boost::asio;

int main()
{
	io_context ioc;

	ip::tcp::resolver resolver{ ioc };
	auto eps = resolver.resolve(smtp_server, "smtp");

	ssl::context ctx{ ssl::context_base::tls };
	mail::smtp::session<ssl::stream<ip::tcp::socket>> session{ ioc, ctx };

	connect(session.next_layer().next_layer(), eps);

	session.open_starttls();
	session.auth_login(test_username, test_password);
	session.send_mail(test_user_mailbox, begin(test_recipient_mailboxes), end(test_recipient_mailboxes), make_test_mail());
	session.close();
}
