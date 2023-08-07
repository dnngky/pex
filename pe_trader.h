#ifndef PE_TRADER_H
#define PE_TRADER_H

#include "pe_common.h"

// Limits
#define TRADER_QTY_LIMIT 999
#define MAX_WAITTIME 2

// Error
#define MARKET_NOPEN_ERR "Error: Market has not been opened\n"
#define INVALID_MSG_ERR "[%s] Error: Expected '%s'; Received '%s'\n"
#define INVALID_CMD_ERR "ERROR: Invalid trader command\n"

// Commands
#define BUY "BUY %d %s %d %d;"
#define SELL "SELL %d %s %d %d;"
#define AMEND "AMEND %d %d %d;"
#define CANCEL "CANCEL %d;"

#endif
