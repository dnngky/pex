# PEX: A virtual marketplace simulator

## Overview

The exchange was intended to be able to handle many traders and orders at a time. It maintains a `pidqueue`, implemented using a linked list with pointers to both ends, which is accessible by the signal handler. This relieves the exchange of the need to process orders immediately as they arrives, which may not be possible if it is currently processing some other order. Each trader is maintained as a struct which contains information about their respective pipes, trader ID, process ID, and positions. BUY/SELL orders are maintained in a max and min `orderqueue` (priority queue), respectively, for each product. This allows the retrieval of highest-priced BUY/lowest-priced SELL orders in constant time, to be matched. The time of enqueue is considered for tie-breaking between identical orders. The exchange also maintains an `orderbook` for each product, implemented as sorted linked lists to ensure information can be retrieved and logged in linear time.

A comprehensive overview of the exchange is available [here](https://lucid.app/publicSegments/view/665f197b-92a8-488e-b006-e6d461659143). Note that a PDF file may be downloaded; alternatively, an online version is available [here](https://lucid.app/documents/view/17f6aa55-b642-4269-88ed-acf29ab33a38).

For enquiries, contact dan.kynguyen2@outlook.com.

## Ensuring fault tolerance

The fault-tolerance of the trader is ensured via two key design choices. Firstly,
a simple `pending_msgs` counter is incremented by the signal handler whenever a
signal arrives. This allows the trader to keep track of the number of pending
messages and appropriately waits when there are none, which prevents busy waiting.
Secondly, an inner while loop is implemented after the trader has written its buy
order to its pipe. The trader then repeatedly signal the exchange (with a brief
pause between each `kill` call) until it receives back a signal. It then verifies
that the message confirms its order has been accepted, after which it breaks out
of the while loop and waits for the next order; else, it goes back and resends the
signal. This implementation ensures that a message is eventually received by the
exchange, even if the initial signals become lost due to race conditions.

## Running tests

The unit tests verify the correctness of the main data structures implemented by
the exchange: `pidqueue`, `orderqueue`, and `orderbook`. For `orderqueue`, only
the minimum type was tested, since the difference between both types are trivial
(the only difference is the formula for computing each node's key). The end-to-end
tests verify the exchange's ability to handle a range of order "traffic":
1. `simple_trader` sends two simple BUY and SELL orders before disconnecting.
2. `self_trader` sends two pair of BUY/SELL orders and tests the exchange's ability
   to match orders.
3. `complex_trader` sends all possible commands (BUY/SELL/AMEND/CANCEL) and mainly
   tests the exchange's management of orderbooks and trader positions.

To run the tests, simply enter `make tests` (to compile the tests) followed by `make run_tests` to the
terminal. NOTE: the end-to-end tests may get stuck on rare occassions. If that
happens, simply end the execution (CTRL+C) and re-enter the commands.