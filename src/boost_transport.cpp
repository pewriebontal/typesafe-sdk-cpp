// Copyright 2026 Bontal LLC
//
// Use of this source code is governed by an MIT-style
// license that can be found in the LICENSE file or at
// https://opensource.org/licenses/MIT.

#include "typesafe/boost_transport.h"

#include <boost/asio/connect.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/asio/ssl/error.hpp>
#include <boost/asio/ssl/host_name_verification.hpp>
#include <boost/asio/ssl/stream.hpp>
#include <boost/asio/steady_timer.hpp>
#include <boost/beast/core.hpp>
#include <boost/beast/http.hpp>
#include <boost/beast/ssl.hpp>
#include <boost/beast/version.hpp>
#include <chrono>
#include <optional>
#include <stdexcept>

namespace beast = boost::beast;
namespace http = beast::http;
namespace net = boost::asio;
namespace ssl = net::ssl;
using tcp = net::ip::tcp;

namespace typesafe
{

BoostTransport::BoostTransport() = default;
BoostTransport::~BoostTransport() = default;

HttpResponse BoostTransport::request(const HttpRequest &req)
{
	constexpr std::string_view https_prefix = "https://";
	if (!req.url.starts_with(https_prefix))
		throw std::runtime_error("BoostTransport only supports https:// URLs.");

	std::string host;
	std::string port = "443";
	std::string target = "/";

	const std::string url = req.url.substr(https_prefix.size());
	const size_t      path_pos = url.find_first_of("/?#");
	const std::string authority = url.substr(0, path_pos);
	if (path_pos != std::string::npos)
		target = url.substr(path_pos);
	if (authority.empty() || authority.find('@') != std::string::npos)
		throw std::runtime_error("Invalid HTTPS URL authority.");

	if (authority.front() == '[')
	{
		const size_t close = authority.find(']');
		if (close == std::string::npos)
			throw std::runtime_error("Invalid IPv6 host in HTTPS URL.");
		host = authority.substr(1, close - 1);
		if (close + 1 < authority.size())
		{
			if (authority[close + 1] != ':' || close + 2 == authority.size())
				throw std::runtime_error("Invalid port in HTTPS URL.");
			port = authority.substr(close + 2);
		}
	}
	else
	{
		const size_t colon = authority.rfind(':');
		if (colon == std::string::npos)
			host = authority;
		else
		{
			host = authority.substr(0, colon);
			port = authority.substr(colon + 1);
		}
	}
	if (host.empty() || port.empty())
		throw std::runtime_error("Invalid host or port in HTTPS URL.");
	if (target.front() == '?' || target.front() == '#')
		target.insert(target.begin(), '/');

	net::io_context ioc;
	ssl::context    ctx(ssl::context::tls_client);

	ctx.set_default_verify_paths();
	ctx.set_verify_mode(ssl::verify_peer);

	tcp::resolver                        resolver(ioc);
	beast::ssl_stream<beast::tcp_stream> stream(ioc, ctx);
	stream.set_verify_callback(ssl::host_name_verification(host));

	if (!SSL_set_tlsext_host_name(stream.native_handle(), host.c_str()))
	{
		beast::error_code ec{static_cast<int>(::ERR_get_error()),
		                     net::error::get_ssl_category()};
		throw beast::system_error{ec};
	}

	tcp::resolver::results_type results;
	if (req.timeout_ms > 0)
	{
		beast::error_code resolve_error;
		bool              completed = false;
		bool              timed_out = false;
		net::steady_timer timer(ioc);
		timer.expires_after(std::chrono::milliseconds(req.timeout_ms));
		resolver.async_resolve(host, port,
		                       [&](const beast::error_code    &error,
		                           tcp::resolver::results_type resolved)
		                       {
			                       if (completed)
				                       return;
			                       completed = true;
			                       resolve_error = error;
			                       results = std::move(resolved);
			                       timer.cancel();
		                       });
		timer.async_wait(
		    [&](const beast::error_code &error)
		    {
			    if (!error && !completed)
			    {
				    timed_out = true;
				    resolver.cancel();
			    }
		    });
		ioc.run();
		ioc.restart();
		if (timed_out)
			throw std::runtime_error("DNS resolution timed out.");
		if (resolve_error)
			throw beast::system_error(resolve_error);
	}
	else
	{
		results = resolver.resolve(host, port);
	}

	auto set_deadline = [&]()
	{
		if (req.timeout_ms > 0)
			beast::get_lowest_layer(stream).expires_after(
			    std::chrono::milliseconds(req.timeout_ms));
	};
	set_deadline();
	beast::get_lowest_layer(stream).connect(results);

	set_deadline();
	stream.handshake(ssl::stream_base::client);

	const http::verb                 verb = http::string_to_verb(req.method);
	http::request<http::string_body> beast_req{
	    verb == http::verb::unknown ? http::verb::get : verb, target, 11};
	if (verb == http::verb::unknown)
		beast_req.method_string(req.method);

	beast_req.set(http::field::host, port == "443" ? host : host + ":" + port);
	beast_req.set(http::field::user_agent, "TypeSafe-Boost-Client");

	for (const auto &pair : req.headers)
		beast_req.set(pair.first, pair.second);

	if (!req.body.empty())
	{
		beast_req.body() = req.body;
		beast_req.prepare_payload();
	}

	set_deadline();
	http::write(stream, beast_req);

	beast::flat_buffer                buffer;
	http::response<http::string_body> beast_res;
	set_deadline();
	http::read(stream, buffer, beast_res);

	beast::error_code ec;
	set_deadline();
	stream.shutdown(ec);
	if (ec == net::error::eof || ec == ssl::error::stream_truncated)
		ec = {};
	if (ec)
		throw beast::system_error{ec};

	HttpResponse res;
	res.status_code = beast_res.result_int();
	res.body = beast_res.body();

	for (const auto &field : beast_res)
	{
		std::string k = std::string(field.name_string());
		std::string v = std::string(field.value());
		for (char &c : k)
			c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
		res.headers[k] = v;
	}

	return (res);
}

}  // namespace typesafe
