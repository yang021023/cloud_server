#pragma once

#include "database.h"

class RestServer {
public:
    explicit RestServer(Database& database);
    bool Start(int port);

private:
    Database& database_;
};
