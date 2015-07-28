#include <array>
#include <string>
#include <iostream>
#include <chrono>
#include <ctime>
#include <cmath>

#include <boost/regex.hpp>
#include <boost/algorithm/string.hpp>

#include "TCPClient.h"

using namespace boost::asio;
using namespace boost::asio::ip;


#define TIMESTAMP() std::chrono::duration_cast< std::chrono::milliseconds >(\
		std::chrono::high_resolution_clock::now().time_since_epoch()).count()

AR::IO::TCPClient::TCPClient() :
	persist_connection(false),
	host(0),
	port(0),
	last_connection(0),
	retry_scale_period(0),
	socket(new Socket(io_service))
{}

AR::IO::TCPClient::~TCPClient()
{
	disconnect();
}

void AR::IO::TCPClient::connect(std::string host, unsigned short port, bool is_persistent)
{
	unsigned long int host_ip_integer = 0;

	std::vector<std::string> tokens;
	boost::split(tokens, host, boost::is_any_of("."));
	if (tokens.size()==4)
	{
		unsigned int
			b1 = std::stoi(tokens[0]),
			b2 = std::stoi(tokens[1]),
			b3 = std::stoi(tokens[2]),
			b4 = std::stoi(tokens[3]);

		host_ip_integer =
				(b1 & 0xFF) << (8 * 3) |
				(b2 & 0xFF) << (8 * 2) |
				(b3 & 0xFF) << (8 * 1) |
				(b4 & 0xFF);

		AR::IO::TCPClient::connect(host_ip_integer, port, is_persistent);
	}
	else
	{
		std::cerr << "[ERROR] Not a valid IP address" << std::endl;
	}
}

void AR::IO::TCPClient::connect(unsigned long int host, unsigned short port, bool is_persistent)
{
	this->persist_connection = is_persistent;
	this->host = host;
	this->port = port;

	std::cout
		<< "[LOG] Connecting to host at IP: "
		<< ((host >> 8*3) & 0xFF) << "."
		<< ((host >> 8*2) & 0xFF) << "."
		<< ((host >> 8*1) & 0xFF) << "."
		<< ((host       ) & 0xFF) << (is_persistent?" (persistent)":"")
		<< std::endl;

	basic_endpoint<tcp> server_endpoint(address_v4(host), port);

	boost::system::error_code ec;
	//This is a blocking function call!
	socket->connect(server_endpoint, ec);

	if(socket->is_open())
	{
		last_connection = TIMESTAMP();
		retry_scale_period = 0;
	}
}

void AR::IO::TCPClient::disconnect()
{
	if(socket->is_open())
	{
		socket->close();
	}
}

void AR::IO::TCPClient::tryReconnect()
{
	auto now = TIMESTAMP();
	auto elapsed_time = now - last_connection;
	auto wait_period = 2000;//std::min(2000, (int)pow(2, retry_scale_period)*100);
	retry_scale_period++;

	if(elapsed_time > wait_period)
	{
		std::cout
			<< "[LOG] Trying to reconnect..."
			<< std::endl;

		socket = std::unique_ptr<Socket>(new Socket(io_service));
		connect(host, port, persist_connection);
	}
}

bool AR::IO::TCPClient::write(std::string message)
{
	try{
		if (!socket)
			return false;
		if (!socket->is_open())
			return false;

		boost::asio::write(*socket, boost::asio::buffer(message));
	}
	catch(...)
	{
		if(persist_connection)
		{
			tryReconnect();
		}
		return false;
	}
	return true;
}


