/*
 * comp2017 - assignment 3
 * dan nguyen
 * kngu7458
 */

#ifndef PE_COMMON_H
#define PE_COMMON_H

#ifndef _POSIX_SOURCE
#define _POSIX_SOURCE
#endif
#ifndef _DEFAULT_SOURCE
#define _DEFAULT_SOURCE
#endif

#include <errno.h>
#include <fcntl.h>
#include <float.h>
#include <limits.h>
#include <math.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/epoll.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

#define INTASCII_OFFSET 48
#define MAX_INTLEN 20
#define INTLEN(n) (n > 9 ? (int)log10(n) + 1 : 1)

#define TIMEOUT 500000
#define BUFFER_SIZE 128
#define FEE_PERCENTAGE 0.01

// function errors
#define EPOLL_CREATE_ERR "EPOLL_CREATE ERROR"
#define EPOLL_WAIT_ERR "EPOLL_WAIT ERROR"
#define EXECL_ERR "EXECL ERROR"
#define FGETS_ERR "FGETS ERROR"
#define FORK_ERR "FORK ERROR"
#define KILL_ERR "KILL ERROR"
#define READ_ERR "READ ERROR"
#define SIGACTION_ERR "SIGACTION ERROR"
#define SNPRINTF_ERR "SNPRINTF ERROR"
#define SSCANF_ERR "SSCANF_ERR"
#define WAITPID_ERR "WAITPID ERROR"
#define WRITE_ERR "WRITE ERROR"

// FIFOs
#define FIFO_EXCHANGE_STRLEN 17
#define FIFO_TRADER_STRLEN 15
#define FIFO_EXCHANGE "/tmp/pe_exchange_%d"
#define FIFO_TRADER "/tmp/pe_trader_%d"

// Limits
#define ORDER_ID_LIMIT 999999
#define PRODUCT_MAXLEN 16
#define QTY_LIMIT 999999
#define PRICE_LIMIT 999999

// Exchange messages
#define MARKET_OPEN "MARKET OPEN;"
#define MARKET_NOTIFY "MARKET %s %s %d %d;"
#define ACCEPTED "ACCEPTED %d;"
#define AMENDED "AMENDED %d;"
#define CANCELLED "CANCELLED %d;"
#define INVALID "INVALID;"
#define FILL "FILL %d %d;"

#endif
