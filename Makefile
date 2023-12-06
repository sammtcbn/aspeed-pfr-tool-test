ALL_BIN=aspeed-pfr-tool

all: $(ALL_BIN)

aspeed-pfr-tool: main.o checkpoint.o info.o i2c_utils.o provision.o status.o spdm.o
	$(CC) $(LDFLAGS) -o $@ $^ -li2c -lssl -lcrypto

main.o: main.c
	$(CC) $(CFLAGS) -c $<

checkpoint.o: checkpoint.c
	$(CC) $(CFLAGS) -c $<

info.o: info.c
	$(CC) $(CFLAGS) -c $<

i2c_utils.o: i2c_utils.c
	$(CC) $(CFLAGS) -c $<

provision.o: provision.c
	$(CC) $(CFLAGS) -c $<

status.o: status.c
	$(CC) $(CFLAGS) -c $<

spdm.o: spdm.c
	$(CC) $(CFLAGS) -c $<

clean:
	rm -f *.o $(ALL_BIN)
