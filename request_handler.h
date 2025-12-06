#pragma once

#include "protocol.h"
#include "minidbms.h"

Response processRequest(const Request& req, MiniDBMS& db);