// Copyright 2026 Bontal LLC
//
// Use of this source code is governed by an MIT-style
// license that can be found in the LICENSE file or at
// https://opensource.org/licenses/MIT.

#include "typesafe/curl_transport.h"

#include <arpa/inet.h>
#include <gtest/gtest.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>

#include <atomic>
#include <string>
#include <thread>
#include <vector>

namespace
{

using namespace typesafe;

/**
 * @brief A loopback HTTP/1.1 server that answers every request with a small
 * keep-alive 200 and counts the TCP connections it accepts, so a test can
 * tell whether the client reused a connection.
 */
class KeepAliveServer
{
	public:
		KeepAliveServer()
		{
			sockaddr_in address{};
			socklen_t   length = sizeof(address);

			_listener = ::socket(AF_INET, SOCK_STREAM, 0);
			address.sin_family = AF_INET;
			address.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
			address.sin_port = 0;
			::bind(_listener, reinterpret_cast<sockaddr *>(&address),
			       sizeof(address));
			::listen(_listener, 16);
			::getsockname(_listener, reinterpret_cast<sockaddr *>(&address),
			              &length);
			_port = ntohs(address.sin_port);
			_acceptor = std::thread(
			    [this]()
			    {
				    AcceptLoop();
			    });
		}

		~KeepAliveServer()
		{
			_stopping = true;
			::shutdown(_listener, SHUT_RDWR);
			::close(_listener);
			_acceptor.join();
			// Clients keep connections open on purpose, so unblock recv().
			for (const int client : _clients)
				::shutdown(client, SHUT_RDWR);
			for (std::thread &connection : _connections)
				connection.join();
			for (const int client : _clients)
				::close(client);
		}

		std::string Url() const
		{
			return ("http://127.0.0.1:" + std::to_string(_port) + "/ping");
		}

		int Accepted() const
		{
			return (_accepted.load());
		}

	private:
		int                      _listener = -1;
		int                      _port = 0;
		std::atomic<bool>        _stopping{false};
		std::atomic<int>         _accepted{0};
		std::thread              _acceptor;
		std::vector<std::thread> _connections;
		std::vector<int>         _clients;

		void AcceptLoop()
		{
			while (!_stopping)
			{
				const int client = ::accept(_listener, nullptr, nullptr);

				if (client < 0)
					return;
				_accepted++;
				_clients.push_back(client);
				_connections.emplace_back(
				    [client]()
				    {
					    Serve(client);
				    });
			}
		}

		static void Serve(int client)
		{
			const std::string reply
			    = "HTTP/1.1 200 OK\r\n"
			      "Content-Type: text/plain\r\n"
			      "Content-Length: 4\r\n"
			      "Connection: keep-alive\r\n\r\n"
			      "pong";
			std::string pending;
			char        buffer[4096];

			while (true)
			{
				const ssize_t received
				    = ::recv(client, buffer, sizeof(buffer), 0);

				if (received <= 0)
					break;
				pending.append(buffer, static_cast<std::size_t>(received));
				for (std::size_t end = pending.find("\r\n\r\n");
				     end != std::string::npos; end = pending.find("\r\n\r\n"))
				{
					pending.erase(0, end + 4);
					::send(client, reply.data(), reply.size(), 0);
				}
			}
		}
};

HttpRequest Get(const std::string &url)
{
	HttpRequest request;

	request.method = "GET";
	request.url = url;
	request.timeout_ms = 2000;
	return (request);
}

TEST(CurlTransportTest, ReusesTheConnectionForRequestsOnOneThread)
{
	KeepAliveServer server;
	CurlTransport   transport;

	const HttpResponse first = transport.request(Get(server.Url()));
	const HttpResponse second = transport.request(Get(server.Url()));

	EXPECT_EQ(first.status_code, 200);
	EXPECT_EQ(second.body, "pong");
	EXPECT_EQ(server.Accepted(), 1);
}

TEST(CurlTransportTest, ServesConcurrentThreadsCorrectly)
{
	KeepAliveServer          server;
	CurlTransport            transport;
	std::atomic<int>         succeeded{0};
	std::vector<std::thread> workers;

	for (int worker = 0; worker < 8; ++worker)
		workers.emplace_back(
		    [&]()
		    {
			    for (int call = 0; call < 5; ++call)
			    {
				    const HttpResponse response
				        = transport.request(Get(server.Url()));

				    if (response.status_code == 200 && response.body == "pong")
					    succeeded++;
			    }
		    });
	for (std::thread &worker : workers)
		worker.join();
	EXPECT_EQ(succeeded.load(), 40);
	EXPECT_LE(server.Accepted(), 8);
}

}  // namespace
