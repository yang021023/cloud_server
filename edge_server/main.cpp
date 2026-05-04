#include "logger.h"
#include "tcp_server.h"

int main() {
    Logger::Info("[EdgeServer] Starting edge server...");

    TcpServer server(8888);
    if (!server.Start()) {
        Logger::Error("[EdgeServer] Startup failed.");
        return 1;
    }

    return 0;
}
