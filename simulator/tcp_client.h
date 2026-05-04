#pragma once

#include <memory>
#include <string>

class TcpClient {
public:
    TcpClient(const std::string& server_ip, int server_port);
    ~TcpClient();

    bool Connect();
    bool SendLine(const std::string& line);
    void Close();
    bool IsConnected() const;

private:
    class Impl;
    std::unique_ptr<Impl> impl_;
};
