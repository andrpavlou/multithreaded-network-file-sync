CC = gcc

CFLAGS 		= -Wall -Iinclude
OBJDIR 		= obj
SRCDIR 		= src
BINDIR 		= bin
UTILSDIR	= utils


CLIENT_SRC 		= $(SRCDIR)/nfs_client.c
MANAGER_SRC 	= $(SRCDIR)/nfs_manager.c
CONSOLE_SRC 	= $(SRCDIR)/nfs_console.c
ADD_CMD_SRC 	= $(SRCDIR)/add_cmd.c
CANCEL_CMD_SRC 	= $(SRCDIR)/cancel_cmd.c
UTILS_SRC 		= $(UTILSDIR)/utils.c
SYNC_INFO_SRC	= $(UTILSDIR)/sync_info.c
SYNC_TASK_SRC	= $(UTILSDIR)/sync_task.c



CLIENT_OBJ 		= $(OBJDIR)/nfs_client.o
MANAGER_OBJ 	= $(OBJDIR)/nfs_manager.o
CONSOLE_OBJ 	= $(OBJDIR)/nfs_console.o
UTILS_OBJ 		= $(OBJDIR)/utils.o
SYNC_INFO_OBJ 	= $(OBJDIR)/sync_info.o
SYNC_TASK_OBJ 	= $(OBJDIR)/sync_task.o
ADD_CMD_OBJ 	= $(OBJDIR)/add_cmd.o
CANCEL_CMD_OBJ 	= $(OBJDIR)/cancel_cmd.o


CLIENT_BIN 	= $(BINDIR)/nfs_client
MANAGER_BIN = $(BINDIR)/nfs_manager
CONSOLE_BIN = $(BINDIR)/nfs_console


all: client utils manager console sync_info sync_task add_cmd cancel_cmd

$(OBJDIR):
	mkdir -p $(OBJDIR)

$(BINDIR):
	mkdir -p $(BINDIR)

utils: $(UTILS_OBJ)
$(UTILS_OBJ): $(UTILS_SRC) | $(OBJDIR)
	$(CC) $(CFLAGS) -c $< -o $@

sync_info: $(SYNC_INFO_OBJ)
$(SYNC_INFO_OBJ): $(SYNC_INFO_SRC) | $(OBJDIR)
	$(CC) $(CFLAGS) -c $< -o $@

sync_task: $(SYNC_TASK_OBJ)
$(SYNC_TASK_OBJ): $(SYNC_TASK_SRC) | $(OBJDIR)
	$(CC) $(CFLAGS) -c $< -o $@


add_cmd: $(ADD_CMD_OBJ)
$(ADD_CMD_OBJ): $(ADD_CMD_SRC) $(UTILS_OBJ) | $(OBJDIR)
	$(CC) $(CFLAGS) -c $< -o $@

cancel_cmd: $(CANCEL_CMD_OBJ)
$(CANCEL_CMD_OBJ): $(CANCEL_CMD_SRC) $(UTILS_OBJ) $(SYNC_INFO_OBJ) | $(OBJDIR)
	$(CC) $(CFLAGS) -c $< -o $@



client: $(CLIENT_BIN) 
$(CLIENT_BIN): $(CLIENT_OBJ) $(UTILS_OBJ) | $(BINDIR)
	$(CC) $(CFLAGS) $^ -o $@ -lpthread

$(CLIENT_OBJ): $(CLIENT_SRC) | $(OBJDIR)
	$(CC) $(CFLAGS) -c $< -o $@ 


manager: $(MANAGER_BIN) 
$(MANAGER_BIN): $(MANAGER_OBJ) $(UTILS_OBJ) $(SYNC_INFO_OBJ) $(SYNC_TASK_OBJ) $(ADD_CMD_OBJ) $(CANCEL_CMD_OBJ) | $(BINDIR)
	$(CC) $(CFLAGS) $^ -o $@ -lpthread

$(MANAGER_OBJ): $(MANAGER_SRC) | $(OBJDIR)
	$(CC) $(CFLAGS) -c $< -o $@ 


console: $(CONSOLE_BIN) 
$(CONSOLE_BIN): $(CONSOLE_OBJ) $(UTILS_OBJ)  $(SYNC_INFO_OBJ) $(SYNC_TASK_OBJ) | $(BINDIR)
	$(CC) $(CFLAGS) $^ -o $@ -lpthread

$(CONSOLE_OBJ): $(CONSOLE_SRC) | $(OBJDIR)
	$(CC) $(CFLAGS) -c $< -o $@ 


clean:
	@rm source2/*
	@rm source3/*
	@rm -rf obj bin
	@rm source4/*
	@rm source5/*


.PHONY: all clean utils sync_info console manager 
