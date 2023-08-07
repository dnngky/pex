/*
 * comp2017 - assignment 3
 * dan nguyen
 * kngu7458
 */

#ifndef PE_EXCHANGE_H
#define PE_EXCHANGE_H

#include "pe_common.h"

// Log messages
#define LOG_PREFIX "[PEX]"
#define START_MSG " Starting\n"
#define TRADE_PRODUCT_MSG " Trading %d products:"
#define CREATE_FIFO_MSG " Created FIFO %s\n"
#define CONNECT_FIFO_MSG " Connected to %s\n"
#define LAUNCH_TRADER_MSG " Starting trader %d (%s)\n"
#define PARSE_CMD_MSG " [T%d] Parsing command: <%s>\n"
#define ORDERBOOK_HDR "\t--ORDERBOOK--\n"
#define ORDERBOOK_PROD "\tProduct: %s; Buy levels: %ld; Sell levels: %ld\n"
#define ORDERBOOK_LOG "\t\t%s %ld @ $%ld (%ld %s)\n"
#define POSITIONS_HDR "\t--POSITIONS--\n"
#define POSITIONS_TRADER "\tTrader %d: "
#define POSITIONS_INFO "%s %ld ($%ld)"
#define MATCH_MSG " Match: Order %d [T%d], New Order %d [T%d], value: $%ld, fee: $%ld.\n"
#define DISCONNECT_MSG " Trader %d disconnected\n"
#define END_MSG " Trading completed\n"
#define COLLECTED_FEES_MSG " Exchange fees collected: $%ld\n"

/*
Check for erreneous return from FUNC, and print the error (prefixed by MSG) to stderr.
To be used in the main function.
*/
#define ERRCHECKMAIN(func, msg) ({  \
if (func < 0) {                     \
    perror(msg);                    \
    goto term;                      \
}})
/*
Check for erreneous return from FUNC, and print the error (prefixed by MSG) to stderr.
To be used in non-main functions.
*/
#define ERRCHECKFUNC(func, msg) ({  \
if (func < 0) {                     \
    perror(msg);                    \
    return -1;                      \
}})

// Exchange-specific errors
#define BADTRADER_ERR "ERROR: A trader sent a signal but did not write to its pipe\n"
#define POPQUEUE_ERR "ERROR: Discrepancy between requested (%d [T%d]) and returned (%d [T%d]) order\n"
#define LOGDEL_ERR "ERROR: Order log %s %d @ $%d does not exist\n"

// Trader commands
#define TRADER_BUY "BUY %d %s %d %d%c"
#define TRADER_SELL "SELL %d %s %d %d%c"
#define TRADER_AMEND "AMEND %d %d %d%c"
#define TRADER_CANCEL "CANCEL %d%c"

typedef enum order_t {
    BUY,
    SELL,
} order_t;

typedef enum oqueue_t {
    MAX_PRIORITYQUEUE,
    MIN_PRIORITYQUEUE,
} oqueue_t;

struct trader {
    int tid; // trader id
    char wfp[BUFFER_SIZE]; // exchange fifo filepath
    char rfp[BUFFER_SIZE]; // trader fifo filepath
    int wfd; // exchange fifo file descriptor
    int rfd; // trader fifo file descriptor
    pid_t pid; // process id
    int norders; // number of orders made by trader
    struct position {
        int64_t *qties;
        int64_t *prices;
    } pos;
};

struct order {
    order_t type; // order type (BUY/SELL)
    int tid; // trader id
    int oid; // order id
    int pdid; // product id
    int qty;
    int price;
    uint64_t time; // time of enqueue (in microseconds)
};

struct pidnode {
    pid_t pid;
    struct pidnode *next;
};
/*
A standard queue for managing PIDS.
*/
struct pidqueue {
    struct pidnode *head;
    struct pidnode *tail;
    size_t size;
};

struct orderlog {
    uint64_t qty;
    uint64_t price;
    uint64_t norders;
    struct orderlog *next;
};
/*
A singly linked list for managing order logs.
*/
struct orderbook {
    uint64_t buy_lvls;
    uint64_t sell_lvls;
    struct orderlog *buy_top;
    struct orderlog *sell_top;
};

struct ordernode {
    struct order *order;
    struct ordernode *parent;
    struct ordernode *left;
    struct ordernode *right;
};
/*
A priority queue for managing orders.
*/
struct orderqueue {
    oqueue_t type; // orderqueue type (max/mininum)
    struct timeval inittime; // time of initialisation
    struct ordernode *root; // root of the orderqueue
    struct ordernode *last; // last inserted ordernode
    size_t size; // number of orders in the orderqueue
};

/*
Initialise a pidnode with PID and add it to PQUEUE.
*/
void penqueue(struct pidqueue *pqueue, pid_t pid);
/*
Remove a pidnode from PQUEUE and return its pid value. -1 is returned
if PQUEUE is empty.
*/
pid_t pdequeue(struct pidqueue *pqueue);
/*
Check whether PQUEUE is empty.
*/
int pqueue_isempty(struct pidqueue *pqueue);
/*
Free all remaining pidnodes in PQUEUE.
*/
void free_pqueue(struct pidqueue *pqueue);

/*
Add ORDER into OBOOK.
*/
void addlog(struct orderbook *obook, struct order order);
/*
Set ORDER's quantity to QTY in OBOOOK. If QTY is 0, then the whole order is
removed. If ORDER does not exist, -1 is returned.
*/
int setlog(struct orderbook *obook, struct order order, int qty);
/*
Remove ORDER completely from OBOOK. If ORDER does not exist, -1 is returned.
*/
int dellog(struct orderbook *obook, struct order order);
/*
Free all outstanding order logs of OBOOK.
*/
int free_obook(struct orderbook *obook);

/*
Initialise a priority queue of given TYPE.
*/
struct orderqueue *init_oqueue(oqueue_t type);
/*
Insert ORDER into OQUEUE.
*/
void oenqueue(struct orderqueue *oqueue, struct order *order);
/*
Retrieve (a pointer to) the max/minimum order from OQUEUE. NULL is returned
if OQUEUE is empty.
*/
struct order *odequeue(struct orderqueue *oqueue);
/*
Retrieve (a pointer to) ORDER in OQUEUE.
*/
struct order *opopqueue(struct orderqueue *oqueue, struct order *order);
/*
Get (a pointer to) the max/minimum order from PQUEUE without removing it.
*/
struct order *opeek(struct orderqueue *oqueue);
/*
Check whether PQUEUE is empty.
*/
int oqueue_isempty(struct orderqueue *oqueue);
/*
Free all outstanding ordernodes in PQUEUE.
*/
void free_oqueue(struct orderqueue *oqueue);

/*
Free all dynamically allocated data.
*/
void teardown(
    struct pidqueue *oqueue,
	struct trader **traders, int ntraders,
	char **products, int nprods,
	struct orderqueue **buy_oqueues, struct orderqueue **sell_oqueues,
	struct order **orders, struct orderbook *obs, struct epoll_event *evs
);
/*
Delete all pipes.
*/
void clean_fifos(struct trader **traders, int ntraders);
/*
Parse product information from FILE into PRODUCTS.
*/
int read_products(char ***products, char *file);
/*
Retrieve the trader from process PID whose signal was delivered and write
to pipe was reported in EVS. Return NULL if no such trader is found.
*/
struct trader *get_trader(pid_t pid, struct epoll_event *evs, int nevs);
/*
Retrieve the pdid (product id) of PRODUCT from an NPRODS-sized list PRODUCTS.
The pdid is determined by the order in which they were listed in the products file.
*/
int get_pdid(char **products, int nprods, char *product);
/*
Verify that O1 matches with O2, that is, their trader IDs and order IDs match.
*/
int order_isequal(struct order o1, struct order o2);
/*
Match orders stored in BUY_OPQUEUES and SELL_OPQUEUES, after ORDER has just been
added or amended, and update OBOOKS accordingly. -1 is returned if an error occurred.
*/
int match_orders(
	struct order order, struct orderqueue **buy_oqueues, struct orderqueue **sell_oqueues,
	struct trader **traders, struct orderbook *obooks, int64_t *collected_fees,
	char *wbuffer, char *msgbuffer
);

#endif
