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
	auto eps = resolver.resolve(smtp_server, "465");

	ip::tcp::socket socket{ ioc };
	connect(socket, eps);

	ssl::context ctx{ ssl::context_base::tls };
	ssl::stream<ip::tcp::socket&> ssl_stream{ socket, ctx };
	ssl_stream.handshake(ssl_stream.client);

	mail::smtp::session<ssl::stream<ip::tcp::socket&>&> session{ ssl_stream };

	session.open();
	session.auth_login(test_username, test_password);
	session.send_mail(test_user_mailbox, begin(test_recipient_mailboxes), end(test_recipient_mailboxes), make_test_mail());
	session.close();
}
