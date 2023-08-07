/*
 * comp2017 - assignment 3
 * dan nguyen
 * kngu7458
 */

#include "pe_trader.h"

volatile sig_atomic_t pending_msgs;

/*
Close RFD and WFD, and terminate with exit code EXITCD.
*/
int term(int rfd, int wfd, int exitcd)
{
    close(rfd);
    close(wfd);
    return exitcd;
}

/*
Read the earliest pending message, from the pipe specified by RFD,
into BUFFER of size SIZE. Return the number of characters read.
*/
ssize_t get_order(int rfd, char *buffer, size_t size)
{
    char c[1];
    ssize_t nread = 0;
    while (nread < size) {
        read(rfd, c, 1);
        buffer[nread] = c[0];
        nread++;
        if (c[0] == ';')
            break;
    }
    buffer[nread] = '\0';
    pending_msgs--;

    return nread;
}

/*
Update the number of pending messages. This is a signal handler,
to be used by sigaction.
*/
void default_handler(int signum)
{
    pending_msgs++;
}

int main(int argc, char **argv)
{
	if (argc < 1) {
        puts("ERROR: Not enough arguments");
        return EXIT_FAILURE;
    }
    int tid = atoi(argv[1]);
    
    // connect to fifos
    char rfifo[BUFFER_SIZE];
    char wfifo[BUFFER_SIZE];
    if (snprintf(rfifo, BUFFER_SIZE, FIFO_EXCHANGE, tid) < 0) {
        perror(SNPRINTF_ERR);
        return EXIT_FAILURE;
    }
    if (snprintf(wfifo, BUFFER_SIZE, FIFO_TRADER, tid) < 0) {
        perror(SNPRINTF_ERR);
        return EXIT_FAILURE;
    }
    // open fifos
    int rfd = open(rfifo, O_RDONLY);
    if (rfd < 0) {
        perror("(EXCHANGE) OPEN ERROR");
        return EXIT_FAILURE;
    }
    int wfd = open(wfifo, O_WRONLY);
    if (wfd < 0) {
        perror("(TRADER) OPEN ERROR");
        return EXIT_FAILURE;
    }
    // register signal handler
    struct sigaction sa = {
        .sa_flags = SA_RESTART,
        .sa_handler = default_handler
    };
    sigaction(SIGUSR1, &sa, NULL);
    pending_msgs = 0;

    ssize_t nread;
    char rbuffer[BUFFER_SIZE];
    char wbuffer[BUFFER_SIZE];
    char msgbuffer[BUFFER_SIZE];

    // retrieve market open message
    while (pending_msgs == 0) pause();
	if ((nread = get_order(rfd, rbuffer, BUFFER_SIZE)) < 0) {
        perror(READ_ERR);
		return term(rfd, wfd, 1);
    }
    if (strncmp(rbuffer, MARKET_OPEN, strlen(MARKET_OPEN)) != 0) {
        write(STDOUT_FILENO, MARKET_NOPEN_ERR, strlen(MARKET_NOPEN_ERR));
        return term(rfd, wfd, 1);
    }
    int order_id = 0;
    
    // event loop:
    while (order_id <= ORDER_ID_LIMIT)
    {
        // retrieve message from exchange
        while (pending_msgs == 0) pause();
		if ((nread = get_order(rfd, rbuffer, BUFFER_SIZE)) < 0) {
            perror(READ_ERR);
			return term(rfd, wfd, 1);
        }
        // if message is not a notification about a SELL order, continue
        if (strncmp(rbuffer, "MARKET SELL", strlen("MARKET SELL")) != 0)
            continue;
        
        // else, parse sell order
        char type[5];
        char product[PRODUCT_MAXLEN+1];
        int qty;
        int price;

        int nscanned = sscanf(rbuffer, MARKET_NOTIFY, type, product, &qty, &price);
        if (nscanned == EOF) {
            perror(SSCANF_ERR);
            return term(rfd, wfd, 1);
        }
        if (nscanned < 4) {
            if (snprintf(msgbuffer, BUFFER_SIZE, INVALID_MSG_ERR, "PE", "MARKET SELL [product] [qty] [price]", rbuffer) < 0)
                perror(SNPRINTF_ERR);
            else
                write(STDOUT_FILENO, msgbuffer, strlen(msgbuffer));
            return term(rfd, wfd, 1);
        }
        // if maximum quantity is exceeded, terminate
        if (qty > TRADER_QTY_LIMIT)
            return term(rfd, wfd, 1);
        
        // write buy order
        if (snprintf(wbuffer, BUFFER_SIZE, BUY, order_id, product, qty, price) < 0) {
            perror(SNPRINTF_ERR);
            return term(rfd, wfd, 1);
        }
        if (write(wfd, wbuffer, strlen(wbuffer)) < 0) {
            perror(WRITE_ERR);
            return term(rfd, wfd, 1);
        }
        while (1)
        {
            // signal to exchange until a signal is received
            do {
                if (kill(getppid(), SIGUSR1) < 0) {
                    perror(KILL_ERR);
                    return term(rfd, wfd, 1);
                }
                usleep(TIMEOUT);
            } while (pending_msgs == 0);

            // retrieve message from exchange
            if ((nread = get_order(rfd, rbuffer, BUFFER_SIZE)) < 0) {
                perror(READ_ERR);
                return term(rfd, wfd, 1);
            }
            if (snprintf(wbuffer, BUFFER_SIZE, ACCEPTED, order_id) < 0) {
                perror(SNPRINTF_ERR);
                return term(rfd, wfd, 1);
            }
            // if order has been accepted, continue to next order
            if (strncmp(rbuffer, wbuffer, strlen(wbuffer)) == 0)
                break;
        }
        order_id++;
    }
    return term(rfd, wfd, 0);
}
