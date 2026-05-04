#include "database.h"
#include "auth.h"
#include "logger.h"
#include "rest_server.h"
#include "grpc_service_impl.h"
#include "sqlite3.h"

#include <cstdlib>
#include <grpcpp/grpcpp.h>
#include <thread>

namespace {
constexpr int kRestPort = 8080;
constexpr int kGrpcPort = 50051;
}

int main() {
    std::setenv("GRPC_VERBOSITY", "ERROR", 1);
    std::setenv("GRPC_GOOGLE_CREDENTIALS_PATH", "", 1);

    Logger::Info("[CloudServer] Starting cloud server...");

    Database database("greenhouse.db");
    if (!database.Open()) {
        Logger::Error("[CloudServer] Database open failed.");
        return 1;
    }

    if (!database.Initialize()) {
        Logger::Error("[CloudServer] Database init failed.");
        return 1;
    }

    if (!database.SeedDefaultData()) {
        Logger::Error("[CloudServer] Database seed failed.");
        return 1;
    }

    Logger::Info("[CloudServer] SQLite database opened: greenhouse.db");

    GrpcServiceImpl grpc_service(database);
    grpc::ServerBuilder builder;
    builder.AddListeningPort("0.0.0.0:" + std::to_string(kGrpcPort), grpc::InsecureServerCredentials());
    builder.RegisterService(&grpc_service);
    std::unique_ptr<grpc::Server> grpc_server = builder.BuildAndStart();
    Logger::Info("[CloudServer] gRPC server listening on 0.0.0.0:" + std::to_string(kGrpcPort));

    RestServer rest_server(database);
    std::thread rest_thread([&]() {
        if (!rest_server.Start(kRestPort)) {
            Logger::Error("[CloudServer] REST server start failed.");
        }
    });

    Logger::Info("[CloudServer] All services started. Press Ctrl+C to stop.");

    rest_thread.join();
    if (grpc_server) {
        grpc_server->Shutdown();
    }

    return 0;
}
