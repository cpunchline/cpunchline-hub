#include <iostream>
#include "asio.hpp"

int client_end_point()
{
    uint16_t port = 3333;
    asio::error_code ec{};
    asio::ip::address ip_address = asio::ip::address_v6::any();
    if (ec)
    {
        std::cerr << "error: [" << ec.value() << "](" << ec.message() << ")" << "\n";
        return ec.value();
    }

    asio::ip::tcp::endpoint ep(ip_address, port);
}

int server_end_point()
{
    uint16_t port = 3333;
    asio::error_code ec;
    asio::ip::address ip_address = asio::ip::address_v6::any();
    if (ec)
    {
        std::cerr << "error: [" << ec.value() << "](" << ec.message() << ")" << "\n";
        return ec.value();
    }

    asio::ip::tcp::endpoint ep(ip_address, port);
}

int create_tcp_socket()
{
    asio::io_context ioc;
    asio::ip::tcp protocol = asio::ip::tcp::v4();
    asio::ip::tcp::socket sock(ioc);
    asio::error_code ec;
    sock.open(protocol, ec);
    if (ec)
    {
        std::cerr << "error: [" << ec.value() << "](" << ec.message() << ")" << "\n";
        return ec.value();
    }

    return 0;
}

int create_acceptor_socket()
{
    asio::io_context ioc;
    asio::ip::tcp protocol = asio::ip::tcp::v4();
    asio::ip::tcp::acceptor acceptor(ioc);
    asio::error_code ec;
    acceptor.open(protocol, ec);
    if (ec)
    {
        std::cerr << "error: [" << ec.value() << "](" << ec.message() << ")" << "\n";
        return ec.value();
    }

    return 0;
}

int main()
{
    std::cout << "asio version[" << ASIO_VERSION << "]" << std::endl;
    client_end_point();
    return EXIT_SUCCESS;
}