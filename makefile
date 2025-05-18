CC = gcc

CFLAGS = -Wall -Iinclude
OBJDIR = obj
BINDIR = bin


CLIENT_SRC 		= src/nfs_client.c
UTILS_SRC 		= utils/utils.c


CLIENT_OBJ 		= $(OBJDIR)/nfs_client.o
UTILS_OBJ 		= $(OBJDIR)/utils.o


CLIENT_BIN 	= $(BINDIR)/client


all: client utils

$(OBJDIR):
	mkdir -p $(OBJDIR)

$(BINDIR):
	mkdir -p $(BINDIR)

utils: $(UTILS_OBJ)
$(UTILS_OBJ): $(UTILS_SRC) | $(OBJDIR)
	$(CC) $(CFLAGS) -c $< -o $@




client: $(CLIENT_BIN) 
$(CLIENT_BIN): $(CLIENT_OBJ) $(UTILS_OBJ)  | $(BINDIR)
	$(CC) $(CFLAGS) $^ -o $@

$(CLIENT_OBJ): $(CLIENT_SRC) | $(OBJDIR)
	$(CC) $(CFLAGS) -c $< -o $@



clean:
	@rm -rf obj bin



.PHONY: all clean utils sync_info console manager 
