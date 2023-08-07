/*
 * comp2017 - assignment 3
 * dan nguyen
 * kngu7458
 */

#include "pe_exchange.h"

void teardown(
	struct pidqueue *pidq,
	struct trader **traders, int ntraders,
	char **products, int nprods,
	struct orderqueue **buy_oqueues, struct orderqueue **sell_oqueues,
	struct order **orders, struct orderbook *obooks, struct epoll_event *evs)
{
	for (int i = 0; i < ntraders; i++) {
		close(traders[i]->wfd);
		close(traders[i]->rfd);
		free(traders[i]->pos.qties);
		free(traders[i]->pos.prices);
		free(traders[i]);
		free(orders[i]);
	}
	for (int i = 0; i < nprods; i++) {
		free(products[i]);
		free_obook(&obooks[i]);
		free_oqueue(buy_oqueues[i]);
		free(buy_oqueues[i]);
		free_oqueue(sell_oqueues[i]);
		free(sell_oqueues[i]);
	}
	free_pqueue(pidq);
	free(traders);
	free(products);
	free(buy_oqueues);
	free(sell_oqueues);
	free(orders);
	free(obooks);
	free(evs);
}

void clean_fifos(struct trader **traders, int ntraders)
{
	for (int i = 0; i < ntraders; i++) {
		unlink(traders[i]->wfp);
		unlink(traders[i]->rfp);
	}
}

int read_products(char ***products, char *file)
{
	FILE *fp = fopen(file, "r");
	char buffer[MAX_INTLEN+1];
	if (fgets(buffer, MAX_INTLEN+1, fp) == NULL) {
		perror(FGETS_ERR);
		return -1;
	}
	int nprods = atoi(buffer);
	*products = malloc(nprods*sizeof(char *));
	for (int i = 0; i < nprods; i++) {
		char product[PRODUCT_MAXLEN+2]; // 1 for \n, 1 for \0
		if (fgets(product, PRODUCT_MAXLEN+2, fp) == NULL) {
			perror(FGETS_ERR);
			return -1;
		}
		product[strlen(product)-1] = '\0'; // remove \n
		(*products)[i] = malloc(PRODUCT_MAXLEN+1);
		strncpy((*products)[i], product, PRODUCT_MAXLEN);
	}
	fclose(fp);
	return nprods;
}

struct trader *get_trader(pid_t pid, struct epoll_event *evs, int nevs)
{
	if (pid == -1)
		return NULL;
	for (int i = 0; i < nevs; i++) {
		struct trader *trader = evs[i].data.ptr;
		if (trader->pid == pid)
			return trader;
	}
	return NULL;
}

int get_pdid(char **products, int nprods, char *product)
{
	for (int i = 0; i < nprods; i++) {
		if (strncmp(product, products[i], strlen(products[i])) == 0)
			return i;
	}
	return -1;
}

int order_isequal(struct order o1, struct order o2)
{
	if (o1.tid != o2.tid) return 0;
	if (o1.oid != o2.oid) return 0;
	return 1;
}

int match_orders(
	struct order order, struct orderqueue **buy_oqueues, struct orderqueue **sell_oqueues,
	struct trader **traders, struct orderbook *obooks, int64_t *collected_fees,
	char *wbuffer, char *msgbuffer)
{
	while (1)
	{
		// check that neither of the priority queues are empty
		if (oqueue_isempty(buy_oqueues[order.pdid]) || oqueue_isempty(sell_oqueues[order.pdid]))
			break;
		
		struct order *max_buy = opeek(buy_oqueues[order.pdid]);
		struct order *min_sell = opeek(sell_oqueues[order.pdid]);
		
		// check that new order is either max-priced BUY or min-priced SELL
		if (!order_isequal(order, *max_buy) &&
			!order_isequal(order, *min_sell))
			break;
		
		// check that max-priced BUY is at least as large as min-priced SELL
		if (max_buy->price < min_sell->price)
			break;
		
		struct trader *buyer = traders[max_buy->tid];
		struct trader *seller = traders[min_sell->tid];
		int fill_qty;
		int64_t matching_price;
		int64_t transaction_fee;

		// update orders after matching
		if (max_buy->qty < min_sell->qty)
		{
			fill_qty = max_buy->qty;
			odequeue(buy_oqueues[order.pdid]);
			if (dellog(&obooks[order.pdid], *max_buy) < 0) {
				ERRCHECKFUNC(snprintf(
					msgbuffer, BUFFER_SIZE, LOGDEL_ERR,
					(max_buy->type == BUY ? "BUY" : "SELL"), max_buy->qty, max_buy->price
				), SNPRINTF_ERR);
				ERRCHECKFUNC(write(STDOUT_FILENO, msgbuffer, strlen(msgbuffer)), WRITE_ERR);
				return -1;
			}
			if (setlog(&obooks[order.pdid], *min_sell, min_sell->qty-fill_qty) < 0) {
				ERRCHECKFUNC(snprintf(
					msgbuffer, BUFFER_SIZE, LOGDEL_ERR,
					(max_buy->type == BUY ? "BUY" : "SELL"), max_buy->qty, max_buy->price
				), SNPRINTF_ERR);
				ERRCHECKFUNC(write(STDOUT_FILENO, msgbuffer, strlen(msgbuffer)), WRITE_ERR);
				return -1;
			}
			min_sell->qty -= fill_qty;
		}
		else if (max_buy->qty > min_sell->qty)
		{
			fill_qty = min_sell->qty;
			odequeue(sell_oqueues[order.pdid]);
			if (dellog(&obooks[order.pdid], *min_sell) < 0) {
				ERRCHECKFUNC(snprintf(
					msgbuffer, BUFFER_SIZE, LOGDEL_ERR,
					(max_buy->type == BUY ? "BUY" : "SELL"), max_buy->qty, max_buy->price
				), SNPRINTF_ERR);
				ERRCHECKFUNC(write(STDOUT_FILENO, msgbuffer, strlen(msgbuffer)), WRITE_ERR);
				return -1;
			}
			if (setlog(&obooks[order.pdid], *max_buy, max_buy->qty-fill_qty) < 0) {
				ERRCHECKFUNC(snprintf(
					msgbuffer, BUFFER_SIZE, LOGDEL_ERR,
					(max_buy->type == BUY ? "BUY" : "SELL"), max_buy->qty, max_buy->price
				), SNPRINTF_ERR);
				ERRCHECKFUNC(write(STDOUT_FILENO, msgbuffer, strlen(msgbuffer)), WRITE_ERR);
				return -1;
			}
			max_buy->qty -= fill_qty;
		}
		else
		{
			fill_qty = max_buy->qty;
			odequeue(buy_oqueues[order.pdid]);
			odequeue(sell_oqueues[order.pdid]);
			if (dellog(&obooks[order.pdid], *max_buy) < 0) {
				ERRCHECKFUNC(snprintf(
					msgbuffer, BUFFER_SIZE, LOGDEL_ERR,
					(max_buy->type == BUY ? "BUY" : "SELL"), max_buy->qty, max_buy->price
				), SNPRINTF_ERR);
				ERRCHECKFUNC(write(STDOUT_FILENO, msgbuffer, strlen(msgbuffer)), WRITE_ERR);
				return -1;
			}
			if (dellog(&obooks[order.pdid], *min_sell) < 0) {
				ERRCHECKFUNC(snprintf(
					msgbuffer, BUFFER_SIZE, LOGDEL_ERR,
					(max_buy->type == BUY ? "BUY" : "SELL"), max_buy->qty, max_buy->price
				), SNPRINTF_ERR);
				ERRCHECKFUNC(write(STDOUT_FILENO, msgbuffer, strlen(msgbuffer)), WRITE_ERR);
				return -1;
			}
		}
		// calculate prices/fees and update traders' positions
		buyer->pos.qties[order.pdid] += fill_qty;
		seller->pos.qties[order.pdid] -= fill_qty;
		if (max_buy->time < min_sell->time) {
			matching_price = (int64_t)max_buy->price*fill_qty;
			transaction_fee = round(matching_price*FEE_PERCENTAGE);
			buyer->pos.prices[order.pdid] -= matching_price;
			seller->pos.prices[order.pdid] += matching_price-transaction_fee;
		} else {
			matching_price = (int64_t)min_sell->price*fill_qty;
			transaction_fee = round(matching_price*FEE_PERCENTAGE);
			buyer->pos.prices[order.pdid] -= matching_price+transaction_fee;
			seller->pos.prices[order.pdid] += matching_price;
		}
		// update total collected exchange fees
		*collected_fees += transaction_fee;

		// confirm to buyer and seller
		if (traders[max_buy->tid]->pid > 0) {
			ERRCHECKFUNC(snprintf(wbuffer, BUFFER_SIZE, FILL, max_buy->oid, fill_qty), SNPRINTF_ERR);
			ERRCHECKFUNC(write(buyer->wfd, wbuffer, strlen(wbuffer)), WRITE_ERR);
			ERRCHECKFUNC(kill(buyer->pid, SIGUSR1), KILL_ERR);
		}
		if (traders[min_sell->tid]->pid > 0) {
			ERRCHECKFUNC(snprintf(wbuffer, BUFFER_SIZE, FILL, min_sell->oid, fill_qty), SNPRINTF_ERR);
			ERRCHECKFUNC(write(seller->wfd, wbuffer, strlen(wbuffer)), WRITE_ERR);
			ERRCHECKFUNC(kill(seller->pid, SIGUSR1), KILL_ERR);	
		}
		/* [PEX] Match: Order ..., New Order ..., value: ..., fee: ... */
		struct order *old_order;
		struct order *new_order;
		if (order.type == BUY) {
			old_order = min_sell;
			new_order = max_buy;
		} else {
			old_order = max_buy;
			new_order = min_sell;
		}
		ERRCHECKFUNC(snprintf(
			msgbuffer, BUFFER_SIZE, MATCH_MSG,
			old_order->oid, old_order->tid,
			new_order->oid, new_order->tid,
			matching_price, transaction_fee
		), SNPRINTF_ERR);
		ERRCHECKFUNC(write(STDOUT_FILENO, LOG_PREFIX, strlen(LOG_PREFIX)), WRITE_ERR);
		ERRCHECKFUNC(write(STDOUT_FILENO, msgbuffer, strlen(msgbuffer)), WRITE_ERR);
	}
	return 0;
}
