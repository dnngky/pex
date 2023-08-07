/*
 * comp2017 - assignment 3
 * dan nguyen
 * kngu7458
 */

#include "pe_exchange.h"

struct pidqueue pqueue; // pending pids of signal-sending traders

int ntraders; // number of traders
struct trader **traders; // trader containers

/*
Extract and record the pid of the trader whose signal was delivered.
This is a signal handler, to be used by sigaction.
*/
void collect_pid(int signum, siginfo_t *info, void *ucontext)
{
	penqueue(&pqueue, info->si_pid);
}

int main(int argc, char **argv)
{	
	if (argc < 3) {
		puts("ERROR: Not enough arguments");
		return EXIT_FAILURE;
	}
	ntraders = argc-2; // number of traders
	char msgbuffer[BUFFER_SIZE]; // for logging messages to stdout

	// read in products file
	char **products;
	int nprods;
	if ((nprods = read_products(&products, argv[1])) < 0)
		return EXIT_FAILURE;
	
	// set up traders
	traders = malloc(ntraders*sizeof(struct trader *));
	for (int i = 0; i < ntraders; i++) {
		traders[i] = malloc(sizeof(struct trader));
		traders[i]->pos.qties = calloc(nprods, sizeof(int64_t));
		traders[i]->pos.prices = calloc(nprods, sizeof(int64_t));
	}
	// set up pidqueue
	pqueue = (struct pidqueue){
		.head = NULL,
		.tail = NULL,
		.size = 0
	};
	// set up an order list for each trader
	struct order **orders = malloc(ntraders*sizeof(struct order *));
	for (int i = 0; i < ntraders; i++)
		orders[i] = malloc(ORDER_ID_LIMIT*sizeof(struct order));

	// set up an orderbook for each product
	struct orderbook *obooks = malloc(nprods*sizeof(struct orderbook));
	for (int i = 0; i < nprods; i++) {
		obooks[i] = (struct orderbook) {
			.buy_lvls = 0,
			.sell_lvls = 0,
			.buy_top = NULL,
			.sell_top = NULL
		};
	}
	// set up an orderqueue for each product
	struct orderqueue **buy_oqueues = malloc(nprods*sizeof(struct orderqueue *));
	struct orderqueue **sell_oqueues = malloc(nprods*sizeof(struct orderqueue *));
	for (int i = 0; i < nprods; i++) {
		buy_oqueues[i] = init_oqueue(MAX_PRIORITYQUEUE); // match the HIGHEST-priced buy...
		sell_oqueues[i] = init_oqueue(MIN_PRIORITYQUEUE); // ...with the LOWEST-priced sell
	}
	// set up signal handler
	struct sigaction sa = {
		.sa_flags = SA_RESTART | SA_SIGINFO,
		.sa_sigaction = collect_pid
	};
	ERRCHECKMAIN(sigaction(SIGUSR1, &sa, NULL), SIGACTION_ERR);

	// set up epoll
	struct epoll_event *evs = malloc(ntraders*sizeof(struct epoll_event)); // ready list
	int epfd;
	ERRCHECKMAIN((epfd = epoll_create(ntraders)), EPOLL_CREATE_ERR);

	/* [PEX] Starting */ 
	ERRCHECKMAIN(write(STDOUT_FILENO, LOG_PREFIX, strlen(LOG_PREFIX)), WRITE_ERR);
	ERRCHECKMAIN(write(STDOUT_FILENO, START_MSG, strlen(START_MSG)), WRITE_ERR);

	/* [PEX] Trading ... products: ... */ 
	ERRCHECKMAIN(snprintf(msgbuffer, BUFFER_SIZE, TRADE_PRODUCT_MSG, nprods), SNPRINTF_ERR);
	ERRCHECKMAIN(write(STDOUT_FILENO, LOG_PREFIX, strlen(LOG_PREFIX)), WRITE_ERR);
	ERRCHECKMAIN(write(STDOUT_FILENO, msgbuffer, strlen(msgbuffer)), WRITE_ERR);
	for (int i = 0; i < nprods; i++) {
		ERRCHECKMAIN(snprintf(msgbuffer, BUFFER_SIZE, " %s", products[i]), SNPRINTF_ERR);
		ERRCHECKMAIN(write(STDOUT_FILENO, msgbuffer, strlen(msgbuffer)), WRITE_ERR);
	}
	ERRCHECKMAIN(write(STDOUT_FILENO, "\n", strlen("\n")), WRITE_ERR);

	for (int i = 0; i < ntraders; i++)
	{
		traders[i]->tid = i;
		traders[i]->norders = 0;
		
		// make exchange fifo
		ERRCHECKMAIN(snprintf(traders[i]->wfp, BUFFER_SIZE, FIFO_EXCHANGE, i), SNPRINTF_ERR);
		unlink(traders[i]->wfp); // in case there is a fifo lying around
		ERRCHECKMAIN(mkfifo(traders[i]->wfp, 0666), "(EXCHANGE) MKFIFO ERROR");

		// make trader fifo
		ERRCHECKMAIN(snprintf(traders[i]->rfp, BUFFER_SIZE, FIFO_TRADER, i), SNPRINTF_ERR);
		unlink(traders[i]->rfp); // in case there is a fifo lying around
		ERRCHECKMAIN(mkfifo(traders[i]->rfp, 0666), "(TRADER) MKFIFO ERROR");

		/* [PEX] Created FIFO ... */ 
		ERRCHECKMAIN(snprintf(msgbuffer, BUFFER_SIZE, CREATE_FIFO_MSG, traders[i]->wfp), SNPRINTF_ERR);
		ERRCHECKMAIN(write(STDOUT_FILENO, LOG_PREFIX, strlen(LOG_PREFIX)), WRITE_ERR);
		ERRCHECKMAIN(write(STDOUT_FILENO, msgbuffer, strlen(msgbuffer)), WRITE_ERR);
		
		/* [PEX] Created FIFO ... */ 
		ERRCHECKMAIN(snprintf(msgbuffer, BUFFER_SIZE, CREATE_FIFO_MSG, traders[i]->rfp), SNPRINTF_ERR);
		ERRCHECKMAIN(write(STDOUT_FILENO, LOG_PREFIX, strlen(LOG_PREFIX)), WRITE_ERR);
		ERRCHECKMAIN(write(STDOUT_FILENO, msgbuffer, strlen(msgbuffer)), WRITE_ERR);

		/* [PEX] Starting trader ... */
		ERRCHECKMAIN(snprintf(msgbuffer, BUFFER_SIZE, LAUNCH_TRADER_MSG, i, argv[i+2]), SNPRINTF_ERR);
		ERRCHECKMAIN(write(STDOUT_FILENO, LOG_PREFIX, strlen(LOG_PREFIX)), WRITE_ERR);
		ERRCHECKMAIN(write(STDOUT_FILENO, msgbuffer, strlen(msgbuffer)), WRITE_ERR);

		// fork
		ERRCHECKMAIN((traders[i]->pid = fork()), FORK_ERR);
		if (traders[i]->pid == 0)
		{
			// free everything on the trader's process
			teardown(
				&pqueue,
				traders, ntraders,
				products, nprods,
				buy_oqueues, sell_oqueues,
				orders, obooks, evs
			);
			// launch trader
			char tid[MAX_INTLEN+1];
			ERRCHECKMAIN(snprintf(tid, MAX_INTLEN+1, "%d", i), SNPRINTF_ERR);
			execl(argv[i+2], argv[i+2], tid, (char *)NULL);
		}
		/*
		NOTE: Opening the FIFO for read-only (and in NONBLOCK mode) before opening the FIFO for write-only
		is important in preventing any "deadblock" (i.e., a process blocking on one pipe, while the other
		blocking on the other pipe, so both processes never unblocks).
		*/
		ERRCHECKMAIN((traders[i]->rfd = open(traders[i]->rfp, O_RDONLY | O_NONBLOCK)), "(TRADER) OPEN ERROR");
		ERRCHECKMAIN((traders[i]->wfd = open(traders[i]->wfp, O_WRONLY)), "(EXCHANGE) OPEN ERROR");

		// disable NONBLOCK mode after both FIFOs have been connected
		int fl = fcntl(traders[i]->rfd, F_GETFL, 0);
		fl &= ~O_NONBLOCK;
		fcntl(traders[i]->rfd, F_SETFL, fl);

		/* [PEX] Connected to ... */ 
		ERRCHECKMAIN(snprintf(msgbuffer, BUFFER_SIZE, CONNECT_FIFO_MSG, traders[i]->wfp), SNPRINTF_ERR);
		ERRCHECKMAIN(write(STDOUT_FILENO, LOG_PREFIX, strlen(LOG_PREFIX)), WRITE_ERR);
		ERRCHECKMAIN(write(STDOUT_FILENO, msgbuffer, strlen(msgbuffer)), WRITE_ERR);

		/* [PEX] Connected to ... */ 
		ERRCHECKMAIN(snprintf(msgbuffer, BUFFER_SIZE, CONNECT_FIFO_MSG, traders[i]->rfp), SNPRINTF_ERR);
		ERRCHECKMAIN(write(STDOUT_FILENO, LOG_PREFIX, strlen(LOG_PREFIX)), WRITE_ERR);
		ERRCHECKMAIN(write(STDOUT_FILENO, msgbuffer, strlen(msgbuffer)), WRITE_ERR);

		// register fifos to epoll
		struct epoll_event ev = {
			.events = EPOLLIN,
			.data.ptr = traders[i]
		};
		ERRCHECKMAIN(epoll_ctl(epfd, EPOLL_CTL_ADD, traders[i]->rfd, &ev), "EPOLL_CTL (ADD) ERROR");
	}

	// open market
	for (int i = 0; i < ntraders; i++) {
		if (traders[i]->pid < 0)
			continue;
		ERRCHECKMAIN(write(traders[i]->wfd, MARKET_OPEN, strlen(MARKET_OPEN)), WRITE_ERR);
		ERRCHECKMAIN(kill(traders[i]->pid, SIGUSR1), KILL_ERR);
	}

	char rbuffer[BUFFER_SIZE]; // buffer for reading traders' commands
	char wbuffer[BUFFER_SIZE]; // buffer for writing commands to traders
	uint64_t collected_fees = 0; // total transaction fees collected
	int traders_left = ntraders; // number of traders remaining in the market
	
	/*
	EVENT LOOP
	*/
	while (1)
	{
		int nevs; // number of events in ready list
		
		// check for disconnected traders while there are no pending signals
		do {
			ERRCHECKMAIN((nevs = epoll_wait(epfd, evs, ntraders, 0)), EPOLL_WAIT_ERR);
			for (int i = 0; i < nevs; i++) {
				struct trader *trader = evs[i].data.ptr;
				if (evs[i].events & EPOLLHUP) {
					ERRCHECKMAIN(snprintf(msgbuffer, BUFFER_SIZE, DISCONNECT_MSG, trader->tid), SNPRINTF_ERR);
					ERRCHECKMAIN(write(STDOUT_FILENO, LOG_PREFIX, strlen(LOG_PREFIX)), WRITE_ERR);
					ERRCHECKMAIN(write(STDOUT_FILENO, msgbuffer, strlen(msgbuffer)), WRITE_ERR);
					ERRCHECKMAIN(epoll_ctl(epfd, EPOLL_CTL_DEL, trader->rfd, NULL), "EPOLL_CTL (DEL) ERROR");
					trader->pid = -1; // set this to avoid read/writing to disconnected pipes
					traders_left--;
				}
			}
			// if there are no traders nor pending signals remaining, close the market
			if (!traders_left && pqueue_isempty(&pqueue))
				goto end;
			
			// else, wait for a while and then recheck for disconnected traders
			usleep(TIMEOUT);
		}
		while (pqueue_isempty(&pqueue));
		
		// get the next trader in queue
		ERRCHECKMAIN((nevs = epoll_wait(epfd, evs, ntraders, 0)), EPOLL_WAIT_ERR);
		struct trader *trader = get_trader(pdequeue(&pqueue), evs, nevs);
		if (trader == NULL) continue;
		
		// parse trader's command (char-by-char until ';')
		char c[1];
		ssize_t nread = 0;
		while (nread < BUFFER_SIZE) {
			ERRCHECKMAIN(read(trader->rfd, c, 1), READ_ERR);
			if (c[0] == ';') {
				rbuffer[nread] = '\0';
				break;
			}
			rbuffer[nread] = c[0];
			nread++;
		}
		if (nread == 0) continue;
		
		/* [PEX] Parsing command: <...> */ 
		ERRCHECKMAIN(snprintf(msgbuffer, BUFFER_SIZE, PARSE_CMD_MSG, trader->tid, rbuffer), SNPRINTF_ERR);
		ERRCHECKMAIN(write(STDOUT_FILENO, LOG_PREFIX, strlen(LOG_PREFIX)), WRITE_ERR);
		ERRCHECKMAIN(write(STDOUT_FILENO, msgbuffer, strlen(msgbuffer)), WRITE_ERR);
		
		// process trader's command
		int is_buy = !strncmp(rbuffer, "BUY", 3);
		int is_sell = !strncmp(rbuffer, "SELL", 4);
		int is_amend = !strncmp(rbuffer, "AMEND", 5);
		int is_cancel = !strncmp(rbuffer, "CANCEL", 6);

		if (is_buy || is_sell)
		{
			int oid;
			char product[PRODUCT_MAXLEN+1];
			int qty;
			int price;
			char null; // if this is filled, then the command contains too many arguments
			
			// parse BUY/SELL
			char *TRADER_CMD = (is_buy ? TRADER_BUY : TRADER_SELL);
			int nscanned = sscanf(rbuffer, TRADER_CMD, &oid, product, &qty, &price, &null);
			
			// check for invalid command format
			if (nscanned != 4) {
				if (trader->pid > 0) {
					ERRCHECKMAIN(write(trader->wfd, INVALID, strlen(INVALID)), WRITE_ERR);
					ERRCHECKMAIN(kill(trader->pid, SIGUSR1), KILL_ERR);
				}
				continue;
			}
			// check for invalid order id
			if (oid != trader->norders) {
				if (trader->pid > 0) {
					ERRCHECKMAIN(write(trader->wfd, INVALID, strlen(INVALID)), WRITE_ERR);
					ERRCHECKMAIN(kill(trader->pid, SIGUSR1), KILL_ERR);
				}
				continue;
			}
			// check for invalid product type
			int pdid;
			if ((pdid = get_pdid(products, nprods, product)) < 0) {
				if (trader->pid > 0) {
					ERRCHECKMAIN(write(trader->wfd, INVALID, strlen(INVALID)), WRITE_ERR);
					ERRCHECKMAIN(kill(trader->pid, SIGUSR1), KILL_ERR);
				}
				continue;
			}
			// check for invalid quantity
			if (qty < 1 || QTY_LIMIT < qty) {
				if (trader->pid > 0) {
					ERRCHECKMAIN(write(trader->wfd, INVALID, strlen(INVALID)), WRITE_ERR);
					ERRCHECKMAIN(kill(trader->pid, SIGUSR1), KILL_ERR);
				}
				continue;
			}
			// check for invalid price
			if (price < 1 || QTY_LIMIT < price) {
				if (trader->pid > 0) {
					ERRCHECKMAIN(write(trader->wfd, INVALID, strlen(INVALID)), WRITE_ERR);
					ERRCHECKMAIN(kill(trader->pid, SIGUSR1), KILL_ERR);
				}
				continue;
			}
			// add order to relevant priority queue
			struct orderqueue *oqueue = (is_buy ? buy_oqueues[pdid] : sell_oqueues[pdid]);
			order_t type = (is_buy ? BUY : SELL);
			orders[trader->tid][oid] = (struct order) {
				.type = type,
				.tid = trader->tid,
				.oid = oid,
				.pdid = pdid,
				.qty = qty,
				.price = price,
				.time = -1 // enqueue will fill this internally
			};
			oenqueue(oqueue, &orders[trader->tid][oid]);
			struct order order = orders[trader->tid][oid];
			
			// log to relevant orderbook
			addlog(&obooks[pdid], order);
			
			// update trader's number of orders
			trader->norders++;

			// confirm to trader
			if (trader->pid > 0) {
				ERRCHECKMAIN(snprintf(wbuffer, BUFFER_SIZE, ACCEPTED, oid), SNPRINTF_ERR);
				ERRCHECKMAIN(write(trader->wfd, wbuffer, strlen(wbuffer)), WRITE_ERR);
				ERRCHECKMAIN(kill(trader->pid, SIGUSR1), KILL_ERR);
			}
			// notify other traders
			for (int i = 0; i < ntraders; i++)
			{
				if (i == trader->tid)
					continue;
				if (traders[i]->pid < 0) // trader has already disconnected
					continue;
				ERRCHECKMAIN(snprintf(
					wbuffer, BUFFER_SIZE, MARKET_NOTIFY,
					(type == BUY ? "BUY" : "SELL"), product, qty, price
				), SNPRINTF_ERR);
				ERRCHECKMAIN(write(traders[i]->wfd, wbuffer, strlen(wbuffer)), WRITE_ERR);
				ERRCHECKMAIN(kill(traders[i]->pid, SIGUSR1), KILL_ERR);
			}
			// match orders
			int ret = match_orders(
				order, buy_oqueues, sell_oqueues,
				traders, obooks, &collected_fees,
				wbuffer, msgbuffer
			);
			if (ret < 0)
				goto term;
		}
		else if (is_amend)
		{
			int oid;
			int qty;
			int price;
			char null; // if this is filled, then the command contains too many arguments

			// parse AMEND
			int nscanned = sscanf(rbuffer, TRADER_AMEND, &oid, &qty, &price, &null);

			// check for invalid command format
			if (nscanned != 3) {
				if (trader->pid > 0) {
					ERRCHECKMAIN(write(trader->wfd, INVALID, strlen(INVALID)), WRITE_ERR);
					ERRCHECKMAIN(kill(trader->pid, SIGUSR1), KILL_ERR);
				}
				continue;
			}
			// check for invalid order id
			if (oid > trader->norders) {
				if (trader->pid > 0) {
					ERRCHECKMAIN(write(trader->wfd, INVALID, strlen(INVALID)), WRITE_ERR);
					ERRCHECKMAIN(kill(trader->pid, SIGUSR1), KILL_ERR);
				}
				continue;
			}
			// check for invalid quantity
			if (qty < 1 || QTY_LIMIT < qty) {
				if (trader->pid > 0) {
					ERRCHECKMAIN(write(trader->wfd, INVALID, strlen(INVALID)), WRITE_ERR);
					ERRCHECKMAIN(kill(trader->pid, SIGUSR1), KILL_ERR);
				}
				continue;
			}
			// check for invalid price
			if (price < 1 || QTY_LIMIT < price) {
				if (trader->pid > 0) {
					ERRCHECKMAIN(write(trader->wfd, INVALID, strlen(INVALID)), WRITE_ERR);
					ERRCHECKMAIN(kill(trader->pid, SIGUSR1), KILL_ERR);
				}
				continue;
			}
			// remove order from relevant priority queue
			struct order order = orders[trader->tid][oid];
			struct orderqueue *oqueue = (order.type == BUY ? buy_oqueues[order.pdid] : sell_oqueues[order.pdid]);
			struct order *popped_order;
			if ((popped_order = opopqueue(oqueue, &order)) == NULL) {
				if (trader->pid > 0) {
					ERRCHECKMAIN(write(trader->wfd, INVALID, strlen(INVALID)), WRITE_ERR);
					ERRCHECKMAIN(kill(trader->pid, SIGUSR1), KILL_ERR);
				}
				continue;
			}
			// check if returned order matches with requested order
			if (!order_isequal(order, *popped_order)) {
				ERRCHECKMAIN(snprintf(
					msgbuffer, BUFFER_SIZE, POPQUEUE_ERR,
					order.oid, order.tid,
					popped_order->oid, popped_order->tid
				), SNPRINTF_ERR);
				ERRCHECKMAIN(write(STDOUT_FILENO, msgbuffer, strlen(msgbuffer)), WRITE_ERR);
				goto term;
			}
			// amend order details
			popped_order->qty = qty;
			popped_order->price = price;

			// add amended order back into relevant priority queue
			oenqueue(oqueue, popped_order);

			// amend original order in relevant order book
			if (order.price == price) {
				// if the price is unchanged, then we need only adjust the quantity
				if (setlog(&obooks[order.pdid], order, qty) < 0) {
					ERRCHECKMAIN(snprintf(
						msgbuffer, BUFFER_SIZE, LOGDEL_ERR,
						(order.type == BUY ? "BUY" : "SELL"), order.qty, order.price
					), SNPRINTF_ERR);
					ERRCHECKMAIN(write(STDOUT_FILENO, msgbuffer, strlen(msgbuffer)), WRITE_ERR);
					goto term;
				}
			} else {
				// else, we need to remove the log completely...
				if (dellog(&obooks[order.pdid], order) < 0) {
					ERRCHECKMAIN(snprintf(
						msgbuffer, BUFFER_SIZE, LOGDEL_ERR,
						(order.type == BUY ? "BUY" : "SELL"), order.qty, order.price
					), SNPRINTF_ERR);
					ERRCHECKMAIN(write(STDOUT_FILENO, msgbuffer, strlen(msgbuffer)), WRITE_ERR);
					goto term;
				}
				// ...and add a new log
				addlog(&obooks[order.pdid], *popped_order);
			}
			// amend original order in relevant order list
			orders[trader->tid][oid].qty = qty;
			orders[trader->tid][oid].price = price;

			// confirm to trader
			if (trader->pid > 0) {
				ERRCHECKMAIN(snprintf(wbuffer, BUFFER_SIZE, AMENDED, oid), SNPRINTF_ERR);
				ERRCHECKMAIN(write(trader->wfd, wbuffer, strlen(wbuffer)), WRITE_ERR);
				ERRCHECKMAIN(kill(trader->pid, SIGUSR1), KILL_ERR);
			}
			// notify other traders
			for (int i = 0; i < ntraders; i++)
			{
				if (i == trader->tid)
					continue;
				if (traders[i]->pid < 0) // trader has already disconnected
					continue;
				ERRCHECKMAIN(snprintf(
					wbuffer, BUFFER_SIZE, MARKET_NOTIFY,
					(order.type == BUY ? "BUY" : "SELL"), products[order.pdid], qty, price
				), SNPRINTF_ERR);
				ERRCHECKMAIN(write(traders[i]->wfd, wbuffer, strlen(wbuffer)), WRITE_ERR);
				ERRCHECKMAIN(kill(traders[i]->pid, SIGUSR1), KILL_ERR);
			}
			// match orders
			int ret = match_orders(
				order, buy_oqueues, sell_oqueues,
				traders, obooks, &collected_fees,
				wbuffer, msgbuffer
			);
			if (ret < 0)
				goto term;
		}
		else if (is_cancel)
		{
			int oid;
			char null; // if this is filled, then the command contains too many arguments

			// parse CANCEL
			int nscanned = sscanf(rbuffer, TRADER_CANCEL, &oid, &null);

			// check for invalid command format
			if (nscanned != 1) {
				if (trader->pid > 0) {
					ERRCHECKMAIN(write(trader->wfd, INVALID, strlen(INVALID)), WRITE_ERR);
					ERRCHECKMAIN(kill(trader->pid, SIGUSR1), KILL_ERR);
				}
				continue;
			}
			// check for invalid order id
			if (oid > trader->norders) {
				if (trader->pid > 0) {
					ERRCHECKMAIN(write(trader->wfd, INVALID, strlen(INVALID)), WRITE_ERR);
					ERRCHECKMAIN(kill(trader->pid, SIGUSR1), KILL_ERR);
				}
				continue;
			}
			// remove order from relevant priority queue
			struct order order = orders[trader->tid][oid];
			struct orderqueue *oqueue = (order.type == BUY ? buy_oqueues[order.pdid] : sell_oqueues[order.pdid]);
			struct order *popped_order;
			if ((popped_order = opopqueue(oqueue, &order)) == NULL) {
				if (trader->pid > 0) {
					ERRCHECKMAIN(write(trader->wfd, INVALID, strlen(INVALID)), WRITE_ERR);
					ERRCHECKMAIN(kill(trader->pid, SIGUSR1), KILL_ERR);
				}
				continue;
			}
			// check if returned order matches with requested order
			if (!order_isequal(order, *popped_order)) {
				ERRCHECKMAIN(snprintf(
					msgbuffer, BUFFER_SIZE, POPQUEUE_ERR,
					order.oid, order.tid,
					popped_order->oid, popped_order->tid
				), SNPRINTF_ERR);
				ERRCHECKMAIN(write(STDOUT_FILENO, msgbuffer, strlen(msgbuffer)), WRITE_ERR);
				goto term;
			}
			// remove order from relevant order book
			if (dellog(&obooks[order.pdid], order) < 0) {
				ERRCHECKMAIN(snprintf(
					msgbuffer, BUFFER_SIZE, LOGDEL_ERR,
					(order.type == BUY ? "BUY" : "SELL"), order.qty, order.price
				), SNPRINTF_ERR);
				ERRCHECKMAIN(write(STDOUT_FILENO, msgbuffer, strlen(msgbuffer)), WRITE_ERR);
				goto term;
			}
			// amend original order in relevant order list
			orders[trader->tid][oid].qty = 0;
			orders[trader->tid][oid].price = 0;

			// confirm to trader
			if (trader->pid > 0) {
				ERRCHECKMAIN(snprintf(wbuffer, BUFFER_SIZE, CANCELLED, oid), SNPRINTF_ERR);
				ERRCHECKMAIN(write(trader->wfd, wbuffer, strlen(wbuffer)), WRITE_ERR);
				ERRCHECKMAIN(kill(trader->pid, SIGUSR1), KILL_ERR);
			}
			// notify other traders
			for (int i = 0; i < ntraders; i++)
			{
				if (i == trader->tid)
					continue;
				if (traders[i]->pid < 0) // trader has already disconnected
					continue;
				ERRCHECKMAIN(snprintf(
					wbuffer, BUFFER_SIZE, MARKET_NOTIFY,
					(order.type == BUY ? "BUY" : "SELL"), products[order.pdid], 0, 0
				), SNPRINTF_ERR);
				ERRCHECKMAIN(write(traders[i]->wfd, wbuffer, strlen(wbuffer)), WRITE_ERR);
				ERRCHECKMAIN(kill(traders[i]->pid, SIGUSR1), KILL_ERR);
			}
		}
		else // invalid order type
		{
			ERRCHECKMAIN(write(trader->wfd, INVALID, strlen(INVALID)), WRITE_ERR);
			ERRCHECKMAIN(kill(trader->pid, SIGUSR1), KILL_ERR);
		}
		/* [PEX] --ORDERBOOK-- */
		ERRCHECKMAIN(write(STDOUT_FILENO, LOG_PREFIX, strlen(LOG_PREFIX)), WRITE_ERR);
		ERRCHECKMAIN(write(STDOUT_FILENO, ORDERBOOK_HDR, strlen(ORDERBOOK_HDR)), WRITE_ERR);
		for (int i = 0; i < nprods; i++)
		{
			struct orderbook obook = obooks[i];
			/* [PEX] Product: ...; Buy levels: ...; Sell levels: ... */
			ERRCHECKMAIN(snprintf(
				msgbuffer, BUFFER_SIZE, ORDERBOOK_PROD,
				products[i], obook.buy_lvls, obook.sell_lvls
			), SNPRINTF_ERR);
			ERRCHECKMAIN(write(STDOUT_FILENO, LOG_PREFIX, strlen(LOG_PREFIX)), WRITE_ERR);
			ERRCHECKMAIN(write(STDOUT_FILENO, msgbuffer, strlen(msgbuffer)), WRITE_ERR);

			for (struct orderlog *current = obook.sell_top; current != NULL; current = current->next)
			{
				char *label = (current->norders == 1 ? "order" : "orders");
				/* [PEX] SELL ... ... @ $... (... order(s)) */
				ERRCHECKMAIN(snprintf(
					msgbuffer, BUFFER_SIZE, ORDERBOOK_LOG,
					"SELL", current->qty, current->price, current->norders, label
				), SNPRINTF_ERR);
				ERRCHECKMAIN(write(STDOUT_FILENO, LOG_PREFIX, strlen(LOG_PREFIX)), WRITE_ERR);
				ERRCHECKMAIN(write(STDOUT_FILENO, msgbuffer, strlen(msgbuffer)), WRITE_ERR);
			}
			for (struct orderlog *current = obook.buy_top; current != NULL; current = current->next)
			{
				char *label = (current->norders == 1 ? "order" : "orders");
				/* [PEX] BUY ... ... @ $... (... order(s)) */
				ERRCHECKMAIN(snprintf(
					msgbuffer, BUFFER_SIZE, ORDERBOOK_LOG,
					"BUY", current->qty, current->price, current->norders, label
				), SNPRINTF_ERR);
				ERRCHECKMAIN(write(STDOUT_FILENO, LOG_PREFIX, strlen(LOG_PREFIX)), WRITE_ERR);
				ERRCHECKMAIN(write(STDOUT_FILENO, msgbuffer, strlen(msgbuffer)), WRITE_ERR);
			}
		}
		/* [PEX] --POSITIONS-- */
		ERRCHECKMAIN(write(STDOUT_FILENO, LOG_PREFIX, strlen(LOG_PREFIX)), WRITE_ERR);
		ERRCHECKMAIN(write(STDOUT_FILENO, POSITIONS_HDR, strlen(POSITIONS_HDR)), WRITE_ERR);
		for (int i = 0; i < ntraders; i++)
		{
			/* [PEX] Trader ...: */
			ERRCHECKMAIN(snprintf(msgbuffer, BUFFER_SIZE, POSITIONS_TRADER, i), SNPRINTF_ERR);
			ERRCHECKMAIN(write(STDOUT_FILENO, LOG_PREFIX, strlen(LOG_PREFIX)), WRITE_ERR);
			ERRCHECKMAIN(write(STDOUT_FILENO, msgbuffer, strlen(msgbuffer)), WRITE_ERR);
			
			for (int j = 0; j < nprods; j++)
			{
				struct trader *tr = traders[i];
				/* [PEX] ... ... ($...) */
				ERRCHECKMAIN(snprintf(
					msgbuffer, BUFFER_SIZE, POSITIONS_INFO,
					products[j], tr->pos.qties[j], tr->pos.prices[j]
				), SNPRINTF_ERR);
				ERRCHECKMAIN(write(STDOUT_FILENO, msgbuffer, strlen(msgbuffer)), WRITE_ERR);
				if (j < nprods-1)
					ERRCHECKMAIN(write(STDOUT_FILENO, ", ", strlen(", ")), WRITE_ERR);
			}
			ERRCHECKMAIN(write(STDOUT_FILENO, "\n", strlen("\n")), WRITE_ERR);
		}
	}
	/*
	Standard exit (after all traders have disconnected)
	*/
	end:
		/* [PEX] Trading completed */
		ERRCHECKMAIN(write(STDOUT_FILENO, LOG_PREFIX, strlen(LOG_PREFIX)), WRITE_ERR);
		ERRCHECKMAIN(write(STDOUT_FILENO, END_MSG, strlen(END_MSG)), WRITE_ERR);

		/* [PEX] Exchange fees collected: ... */
		ERRCHECKMAIN(snprintf(msgbuffer, BUFFER_SIZE, COLLECTED_FEES_MSG, collected_fees), SNPRINTF_ERR);
		ERRCHECKMAIN(write(STDOUT_FILENO, LOG_PREFIX, strlen(LOG_PREFIX)), WRITE_ERR);
		ERRCHECKMAIN(write(STDOUT_FILENO, msgbuffer, strlen(msgbuffer)), WRITE_ERR);

		// unlink fifos and teardown exchange
		clean_fifos(traders, ntraders);
		teardown(
			&pqueue,
			traders, ntraders,
			products, nprods,
			buy_oqueues, sell_oqueues,
			orders, obooks, evs
		);
		return EXIT_SUCCESS;
	/*
	Forced exit (following a fatal error)
	*/
	term:
		// unlink fifos and teardown exchange
		clean_fifos(traders, ntraders);
		teardown(
			&pqueue,
			traders, ntraders,
			products, nprods,
			buy_oqueues, sell_oqueues,
			orders, obooks, evs
		);
		return EXIT_FAILURE;
}
