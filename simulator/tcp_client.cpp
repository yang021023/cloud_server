#define ASIO_STANDALONE
#define _WIN32_WINNT 0x0A00

#include "tcp_client.h"

#include <asio.hpp>
#include <iostream>
#include <utility>

class TcpClient::Impl {
public:
    Impl(std::string server_ip, int server_port)
        : server_ip_(std::move(server_ip)),
          server_port_(server_port),
          socket_(io_context_) {
    }

    bool Connect() {
        Close();

        asio::error_code ec;
        const asio::ip::address address = asio::ip::make_address(server_ip_, ec);
        if (ec) {
            std::cerr << "[TcpClient] Invalid IP address: " << server_ip_ << std::endl;
            return false;
        }

        asio::ip::tcp::endpoint endpoint(address, static_cast<unsigned short>(server_port_));
        socket_.connect(endpoint, ec);
        if (ec) {
            std::cerr << "[TcpClient] Connect failed to " << server_ip_ << ':' << server_port_
                      << ", error=" << ec.message() << std::endl;
            return false;
        }

        connected_ = true;
        return true;
    }

    bool SendLine(const std::string& line) {
        if (!connected_) {
            return false;
        }

        const std::string payload = line + "\n";
        asio::error_code ec;
        asio::write(socket_, asio::buffer(payload), ec);
        if (ec) {
            std::cerr << "[TcpClient] Send failed, error=" << ec.message() << std::endl;
            Close();
            return false;
        }

        return true;
    }

    void Close() {
        asio::error_code ec;
        if (socket_.is_open()) {
            socket_.shutdown(asio::ip::tcp::socket::shutdown_both, ec);
            socket_.close(ec);
        }
        connected_ = false;
    }

    bool IsConnected() const {
        return connected_ && socket_.is_open();
    }

private:
    std::string server_ip_;
    int server_port_ = 0;
    asio::io_context io_context_;
    asio::ip::tcp::socket socket_;
    bool connected_ = false;
};

TcpClient::TcpClient(const std::string& server_ip, int server_port)
    : impl_(std::make_unique<Impl>(server_ip, server_port)) {
}

TcpClient::~TcpClient() = default;

bool TcpClient::Connect() {
    return impl_->Connect();
}

bool TcpClient::SendLine(const std::string& line) {
    return impl_->SendLine(line);
}

void TcpClient::Close() {
    impl_->Close();
}

bool TcpClient::IsConnected() const {
    return impl_->IsConnected();
}
