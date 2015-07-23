#pragma once

#include <boost/asio.hpp>
#include <memory>

namespace AR
{
namespace IO
{


class TCPClient
{
public:
	typedef std::unique_ptr<TCPClient> Ptr;

	~TCPClient();

	void connect(std::string host, unsigned short port, bool is_persistent = false);
	void connect(unsigned long int host, unsigned short port, bool is_persistent = false);
	void disconnect();
	bool write(std::string message);

	static Ptr Create()
	{
		return AR::IO::TCPClient::Ptr(new TCPClient());
	}

private:
	typedef boost::asio::ip::tcp::socket Socket;

	bool persist_connection;
	unsigned long int host;
	unsigned short port;

	long int last_connection;
	unsigned int retry_scale_period;

	TCPClient();
	void tryReconnect();
	boost::asio::io_service io_service;
	std::unique_ptr<Socket> socket;
};

} // namespace IO
} // namespace AR

