/*
 * comp2017 - assignment 3
 * dan nguyen
 * kngu7458
 */

#include "pe_exchange.h"

static struct orderlog *init_log(struct order order)
{
    struct orderlog *new_log = malloc(sizeof(struct orderlog));
    new_log->qty = order.qty;
    new_log->price = order.price;
    new_log->norders = 1;
    new_log->next = NULL;
    return new_log;
}

void addlog(struct orderbook *obook, struct order order)
{   
	// if orderbook is empty, create a new log as the head
	if (order.type == BUY && obook->buy_top == NULL) {
		obook->buy_top = init_log(order);
		obook->buy_lvls++;
		return;
	}
    if (order.type == SELL && obook->sell_top == NULL) {
		obook->sell_top = init_log(order);
		obook->sell_lvls++;
		return;
	}
    // if order is higher than the highest-priced log, set it as the new head
    if (order.type == BUY && order.price > obook->buy_top->price) {
        struct orderlog *new_log = init_log(order);
        new_log->next = obook->buy_top;
        obook->buy_top = new_log;
        obook->buy_lvls++;
        return;
    }
    if (order.type == SELL && order.price > obook->sell_top->price) {
        struct orderlog *new_log = init_log(order);
        new_log->next = obook->sell_top;
        obook->sell_top = new_log;
        obook->sell_lvls++;
        return;
    }
    struct orderlog *cur_log = (order.type == BUY ? obook->buy_top : obook->sell_top);
	while (1)
	{
        // if a log with identical price is found, add new order into it
		if (cur_log->price == order.price) {
			cur_log->qty += order.qty;
			cur_log->norders++;
			return;
		}
        // if we reach the tail, create a new log for the order as the new tail
		if (cur_log->next == NULL) {
            cur_log->next = init_log(order);
            if (order.type == BUY)
                obook->buy_lvls++;
            else
                obook->sell_lvls++;
            return;
        }
        // if the order is smaller than current log and greater than next log,
        // create a new log in the middle
		if (order.price < cur_log->price && order.price > cur_log->next->price)
        {
            struct orderlog *new_log = init_log(order);
            new_log->next = cur_log->next;
            cur_log->next = new_log;
            if (order.type == BUY)
                obook->buy_lvls++;
            else
                obook->sell_lvls++;
            return;
        }
        cur_log = cur_log->next;
	}
}

int setlog(struct orderbook *obook, struct order order, int qty)
{
	if (order.type == BUY && order.price == obook->buy_top->price)
    {
		obook->buy_top->qty -= order.qty-qty;
		if (qty == 0)
			obook->buy_top->norders--;
		if (obook->buy_top->norders == 0) {
            struct orderlog *new_head = obook->buy_top->next;
			free(obook->buy_top);
            obook->buy_top = new_head;
			obook->buy_lvls--;
		}
		return 0;
	}
    if (order.type == SELL && order.price == obook->sell_top->price)
    {
		obook->sell_top->qty -= order.qty-qty;
		if (qty == 0)
			obook->sell_top->norders--;
		if (obook->sell_top->norders == 0) {
            struct orderlog *new_head = obook->sell_top->next;
			free(obook->sell_top);
            obook->sell_top = new_head;
			obook->sell_lvls--;
		}
		return 0;
	}
    struct orderlog *cur_log = (order.type == BUY ? obook->buy_top : obook->sell_top);
	while (cur_log->next)
	{
		if (cur_log->next->price == order.price)
        {
			cur_log->next->qty -= order.qty-qty;
			if (qty == 0)
				cur_log->next->norders--;
			if (cur_log->next->norders == 0) {
				struct orderlog *discard_log = cur_log->next;
				cur_log->next = cur_log->next->next;
				free(discard_log);
				if (order.type == BUY)
					obook->buy_lvls--;
				else
					obook->sell_lvls--;
			}
			return 0;
		}
		cur_log = cur_log->next;
	}
	return -1;
}

int dellog(struct orderbook *obook, struct order order)
{
	return setlog(obook, order, 0);
}

int free_obook(struct orderbook *obook)
{
	struct orderlog *cur_log;
    cur_log = obook->buy_top;
	while (cur_log) {
		struct orderlog *discard_log = cur_log;
		cur_log = cur_log->next;
		free(discard_log);
	}
    cur_log = obook->sell_top;
    while (cur_log) {
        struct orderlog *discard_log = cur_log;
        cur_log = cur_log->next;
        free(discard_log);
    }
	obook->buy_top = NULL;
	obook->sell_top = NULL;
	obook->buy_lvls = 0;
	obook->sell_lvls = 0;
}
