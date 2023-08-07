#include <stdarg.h>
#include <stddef.h>
#include <setjmp.h>
#include <stdint.h>
#include <stddef.h>
#include "cmocka.h"

#include "../pe_exchange.h"

static void test_pidqueue(void **state)
{
    struct pidqueue pqueue = {
        .head = NULL,
        .tail = NULL,
        .size = 0
    };
    assert_true(pqueue_isempty(&pqueue));
    penqueue(&pqueue, (pid_t)1);
    penqueue(&pqueue, (pid_t)200);
    penqueue(&pqueue, (pid_t)3);
    assert_int_equal(pdequeue(&pqueue), 1);
    assert_int_equal(pdequeue(&pqueue), 200);
    penqueue(&pqueue, (pid_t)69420);
    penqueue(&pqueue, (pid_t)0);
    penqueue(&pqueue, (pid_t)0);
    assert_int_equal(pdequeue(&pqueue), 3);
    assert_int_equal(pdequeue(&pqueue), 69420);
    assert_int_equal(pdequeue(&pqueue), 0);
    assert_int_equal(pdequeue(&pqueue), 0);
    assert_int_equal(pdequeue(&pqueue), -1);
    assert_true(pqueue_isempty(&pqueue));
    penqueue(&pqueue, (pid_t)1);
    free_pqueue(&pqueue);
    assert_true(pqueue_isempty(&pqueue));
}

static void test_orderqueue(void **state)
{
    struct orderqueue *min_oqueue = init_oqueue(MIN_PRIORITYQUEUE);

    int norders = 8;
    struct order orders[8];
    orders[0] = (struct order){ .price = 500, .tid = 0, .oid = 0 };
    orders[1] = (struct order){ .price = 400, .tid = 0, .oid = 1 };
    orders[2] = (struct order){ .price = 450, .tid = 0, .oid = 2 };
    orders[3] = (struct order){ .price = 500, .tid = 0, .oid = 3 };
    orders[4] = (struct order){ .price = 500, .tid = 0, .oid = 4 };
    orders[5] = (struct order){ .price = 400, .tid = 0, .oid = 5 };
    orders[6] = (struct order){ .price = 800, .tid = 0, .oid = 6 };
    orders[7] = (struct order){ .price = 450, .tid = 0, .oid = 7 };

    assert_true(oqueue_isempty(min_oqueue));
    for (int i = 0; i < norders; i++) {
        oenqueue(min_oqueue, &orders[i]);
        usleep(10);
    }
    assert_int_equal(min_oqueue->size, 8);
    struct order *popped_order = opopqueue(min_oqueue, &orders[3]);
    assert_int_equal(popped_order->price, 500);
    assert_int_equal(popped_order->oid, 3);
    assert_null(opopqueue(min_oqueue, &orders[3]));
    struct order *min_order = odequeue(min_oqueue);
    assert_int_equal(min_order->price, 400);
    assert_int_equal(min_order->oid, 1);
    assert_true(min_order->time < popped_order->time);
    assert_int_equal(min_oqueue->size, 6);
    oenqueue(min_oqueue, min_order);
    struct order *new_min_order = opeek(min_oqueue);
    assert_int_equal(new_min_order->price, 400);
    assert_int_equal(new_min_order->oid, 5);
    for (int i = 0; i < norders; i++) {
        if (i == 3) continue;
        assert_non_null(odequeue(min_oqueue));
    }
    free_oqueue(min_oqueue);
    assert_int_equal(min_oqueue->size, 0);
    free(min_oqueue);
}

static void test_orderbook(void **state)
{
    struct orderbook obook = {
        .buy_lvls = 0,
        .sell_lvls = 0,
        .buy_top = NULL,
        .sell_top = NULL
    };
    
    struct order orders[8];
    orders[0] = (struct order){ .type = BUY, .price = 500, .qty = 100 };
    orders[1] = (struct order){ .type = SELL, .price = 400, .qty = 10 };
    orders[2] = (struct order){ .type = BUY, .price = 450, .qty = 2040 };
    orders[3] = (struct order){ .type = BUY, .price = 500, .qty = 6969 };
    orders[4] = (struct order){ .type = BUY, .price = 500, .qty = 420 };
    orders[5] = (struct order){ .type = SELL, .price = 450, .qty = 192 };
    orders[6] = (struct order){ .type = BUY, .price = 800, .qty = 1 };
    orders[7] = (struct order){ .type = SELL, .price = 420, .qty = 42 };

    addlog(&obook, orders[0]);
    assert_int_equal(obook.buy_lvls, 1);
    addlog(&obook, orders[4]);
    assert_int_equal(obook.buy_lvls, 1);
    addlog(&obook, orders[2]);
    assert_int_equal(obook.buy_lvls, 2);
    assert_int_equal(obook.buy_top->norders, 2);
    assert_int_equal(obook.buy_top->price, 500);
    assert_int_equal(obook.buy_top->qty, 520);
    setlog(&obook, orders[0], 50);
    assert_int_equal(obook.buy_top->qty, 470);
    dellog(&obook, orders[4]);
    assert_int_equal(obook.buy_top->qty, 50);
    assert_int_equal(obook.buy_lvls, 2);
    assert_int_equal(obook.buy_top->norders, 1);
    addlog(&obook, orders[6]);
    assert_int_equal(obook.buy_top->price, 800);
    addlog(&obook, orders[1]);
    addlog(&obook, orders[3]);
    addlog(&obook, orders[5]);
    addlog(&obook, orders[7]);
    assert_int_equal(obook.buy_lvls, 3);
    assert_int_equal(obook.sell_lvls, 3);
    assert_int_equal(obook.buy_top->next->qty, 7019);
    assert_int_equal(obook.sell_top->price, 450);
    free_obook(&obook);
    assert_int_equal(obook.buy_lvls, 0);
    assert_int_equal(obook.sell_lvls, 0);
}

int main(void)
{
    const struct CMUnitTest tests[] = {
        cmocka_unit_test(test_pidqueue),
        cmocka_unit_test(test_orderqueue),
        cmocka_unit_test(test_orderbook),
    };
    return cmocka_run_group_tests(tests, NULL, NULL);
}