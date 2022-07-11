CFLAGS = -Wall
CC = gcc	

OBJSDIR = objs
#$(shell mkdir -p $(OBJSDIR))

DEPS = client.h

OBJS = $(OBJSDIR)/client.o \
	$(OBJSDIR)/child_proc.o \
	$(OBJSDIR)/request.o \
	$(OBJSDIR)/create_client_socket.o \
	$(OBJSDIR)/rd_wr.o \
	$(OBJSDIR)/functions.o 

client: $(OBJS) 
	$(CC) $(CFLAGS) -o $@ $(OBJS)  -lpthread

$(OBJSDIR)/client.o: client.c client.h
	$(CC) $(CFLAGS) -c client.c -o $@

$(OBJSDIR)/child_proc.o: child_proc.c client.h
	$(CC) $(CFLAGS) -c child_proc.c -o $@

$(OBJSDIR)/request.o: request.c client.h
	$(CC) $(CFLAGS) -c request.c -o $@

$(OBJSDIR)/create_client_socket.o: create_client_socket.c client.h
	$(CC) $(CFLAGS) -c create_client_socket.c -o $@

$(OBJSDIR)/rd_wr.o: rd_wr.c client.h
	$(CC) $(CFLAGS) -c rd_wr.c -o $@

$(OBJSDIR)/functions.o: functions.c client.h
	$(CC) $(CFLAGS) -c functions.c -o $@

clean:
	rm -f client
	rm -f $(OBJSDIR)/*.o
