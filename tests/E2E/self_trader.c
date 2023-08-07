#include "../../pe_trader.h"

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

    // wait for market to open
    while (pending_msgs == 0) pause();
	if ((nread = get_order(rfd, rbuffer, BUFFER_SIZE)) < 0) {
        perror(READ_ERR);
		return term(rfd, wfd, 1);
    }
    if (strncmp(rbuffer, MARKET_OPEN, strlen(MARKET_OPEN)) != 0) {
        write(STDOUT_FILENO, MARKET_NOPEN_ERR, strlen(MARKET_NOPEN_ERR));
        return term(rfd, wfd, 1);
    }
    FILE *fp;
    fp = fopen("./tests/E2E/self_trader.in", "r");

    // event loop:
    int order_id = 0;
    while (1)
    {
        if (fgets(wbuffer, BUFFER_SIZE, fp) == NULL) {
            perror(FGETS_ERR);
            return term(rfd, wfd, 1);
        }
        wbuffer[strlen(wbuffer)-1] = '\0';
        if (strncmp(wbuffer, "DISCONNECT;", strlen("DISCONNECT;")) == 0) {
            fclose(fp);
            return term(rfd, wfd, 0);
        }
        if (write(wfd, wbuffer, strlen(wbuffer)) < 0) {
            perror(WRITE_ERR);
            fclose(fp);
            return term(rfd, wfd, 1);
        }
        // check command type
        char *MARKET_MSG = NULL;
        if (strncmp(wbuffer, "BUY", strlen("BUY")) == 0)
            MARKET_MSG = "ACCEPTED";
        else if (strncmp(wbuffer, "SELL", strlen("SELL")) == 0)
            MARKET_MSG = "ACCEPTED";
        else if (strncmp(wbuffer, "AMEND", strlen("AMEND")) == 0)
            MARKET_MSG = "AMENDED";
        else if (strncmp(wbuffer, "CANCEL", strlen("CANCEL")) == 0)  
            MARKET_MSG = "CANCELLED";
        else {
            write(STDOUT_FILENO, INVALID_CMD_ERR, strlen(INVALID_CMD_ERR));
            return term(rfd, wfd, 1);
        }
        while (1)
        {
            // signal to exchange until a signal is received or max waittime is reached
            int waittime = 0;
            do {
                if (kill(getppid(), SIGUSR1) < 0) {
                    perror(KILL_ERR);
                    return term(rfd, wfd, 1);
                }
                waittime++;
                usleep(TIMEOUT);
            } while (pending_msgs == 0 && waittime < MAX_WAITTIME);

            // retrieve message from exchange
            if ((nread = get_order(rfd, rbuffer, BUFFER_SIZE)) < 0) {
                perror(READ_ERR);
                return term(rfd, wfd, 1);
            }
            // if order has been accepted, continue to next order
            if (strncmp(rbuffer, MARKET_MSG, strlen(MARKET_MSG)) == 0)
                break;
        }
        order_id++;
    }
}
