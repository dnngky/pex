TARGET := pe_trader
CC := gcc
CFLAGS := -Wall -Werror -Wvla -O0 -std=c11
ASFLAGS := -fsanitize=address,leak
VGFLAGS := valgrind --leak-check=yes
LDFLAGS := -lm
CMOCKA := ./tests/libcmocka-static.a

PESUB := pe_internals.c pe_pidqueue.c pe_orderqueue.c pe_orderbook.c

PESRC := pe_exchange.c $(PESUB)
PEOBJ := ${PESRC:.c=.o}
PEEXC := pe_exchange

PTSRC := pe_trader.c
PTOBJ := ${PTSRC:.c=.o}
PTEXC := pe_trader

TSRCS := ./tests/E2E/simple_trader.c ./tests/E2E/simple_trader.c ./tests/E2E/self_trader.c ./tests/E2E/complex_trader.c
TOBJS := ${TSRCS:.c=.o}
TEXCS := ${TSRCS:.c=}

TARGET := $(PEEXC) $(PTEXC)

.PHONY: all
all: $(TARGET)

$(PEEXC): $(PEOBJ)
	$(CC) $(CFLAGS) -o $@ $(PEOBJ) $(LDFLAGS)

$(PTEXC): $(PTOBJ)
	$(CC) $(CFLAGS) -o $@ $(PTOBJ) $(LDFLAGS)

%.o: %.c
	$(CC) -c $<

asan:
	$(CC) $(CFLAGS) -g $(ASFLAGS) -o $(PEEXC)-as $(PEOBJ) $(LDFLAGS)

valgrind:
	$(CC) $(CFLAGS) -g -o $(PEEXC)-vg $(PEOBJ) $(LDFLAGS)

tests: ${TEXCS}
	$(CC) -o ./tests/unit-tests ./tests/unit-tests.c $(CMOCKA) $(PESUB) $(LDFLAGS)
	for f in ${TEXCS}; do $(CC) $(CFLAGS) -o $$f $$f.c $(LDFLAGS); done # E2E tests

run_tests:
	./tests/unit-tests
	bash e2e-tests.sh

clean:
	rm -f $(PEEXC) $(PEEXC)-as $(PEEXC)-vg $(PEOBJ) $(PTEXC) $(PTOBJ) $(TOBJS) ${TEXCS} ./tests/unit-tests
