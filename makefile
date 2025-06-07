CC = gcc

CFLAGS 		= -Wall -Iinclude -g
CMDSRCDIR	= src/commands
DATASRCDIR	= src/data_structures
NFSSRCDIR	= src/main
CLIENTSRC 	= src/client
MGRTSRC 	= src/manager
UTILSDIR	= src/utils

OBJDIR 		= obj
BINDIR 		= bin


CLIENT_SRC 		= $(NFSSRCDIR)/nfs_client.c
MANAGER_SRC 	= $(NFSSRCDIR)/nfs_manager.c
CONSOLE_SRC 	= $(NFSSRCDIR)/nfs_console.c

ADD_CMD_SRC 	= $(CMDSRCDIR)/add_cmd.c
CANCEL_CMD_SRC 	= $(CMDSRCDIR)/cancel_cmd.c

CLIENT_CON_SRC 	= $(CLIENTSRC)/client_connection_handler.c
MGR_READ_SRC 	= $(CLIENTSRC)/read_from_manager.c

MGR_WORKER_SRC 	= $(MGRTSRC)/manager_worker_pool.c

UTILS_SRC 		= $(UTILSDIR)/utils.c
SOCKET_UTILS_SRC= $(UTILSDIR)/socket_utils.c

SYNC_INFO_SRC	= $(DATASRCDIR)/sync_info_list.c
SYNC_TASK_SRC	= $(DATASRCDIR)/sync_task_queue.c


CLIENT_OBJ 		= $(OBJDIR)/nfs_client.o
MANAGER_OBJ 	= $(OBJDIR)/nfs_manager.o
CONSOLE_OBJ 	= $(OBJDIR)/nfs_console.o
UTILS_OBJ 		= $(OBJDIR)/utils.o
SOCKET_UTILS_OBJ= $(OBJDIR)/socket_utils.o
ADD_CMD_OBJ 	= $(OBJDIR)/add_cmd.o
CANCEL_CMD_OBJ 	= $(OBJDIR)/cancel_cmd.o
SYNC_INFO_OBJ 	= $(OBJDIR)/sync_info_list.o
SYNC_TASK_OBJ 	= $(OBJDIR)/sync_task_queue.o
CLIENT_CON_OBJ 	= $(OBJDIR)/client_connection_handler.o
MGR_READ_OBJ 	= $(OBJDIR)/read_from_manager.o
MGR_WORKER_OBJ 	= $(OBJDIR)/manager_worker_pool.o



CLIENT_BIN 	= $(BINDIR)/nfs_client
MANAGER_BIN = $(BINDIR)/nfs_manager
CONSOLE_BIN = $(BINDIR)/nfs_console


all: client utils manager console sync_info sync_task add_cmd cancel_cmd socket_utils client_con read_from_manager manager_worker

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
$(ADD_CMD_OBJ): $(ADD_CMD_SRC) $(UTILS_OBJ) $(SOCKET_UTILS_OBJ) | $(OBJDIR)
	$(CC) $(CFLAGS) -c $< -o $@

cancel_cmd: $(CANCEL_CMD_OBJ)
$(CANCEL_CMD_OBJ): $(CANCEL_CMD_SRC) $(UTILS_OBJ) $(SYNC_INFO_OBJ) | $(OBJDIR)
	$(CC) $(CFLAGS) -c $< -o $@

socket_utils: $(SOCKET_UTILS_OBJ)
$(SOCKET_UTILS_OBJ): $(SOCKET_UTILS_SRC) | $(OBJDIR)
	$(CC) $(CFLAGS) -c $< -o $@

client_con: $(CLIENT_CON_OBJ)
$(CLIENT_CON_OBJ): $(CLIENT_CON_SRC) $(SOCKET_UTILS_OBJ) | $(OBJDIR)
	$(CC) $(CFLAGS) -c $< -o $@

read_from_manager: $(MGR_READ_OBJ)
$(MGR_READ_OBJ): $(MGR_READ_SRC) | $(OBJDIR)
	$(CC) $(CFLAGS) -c $< -o $@

manager_worker: $(MGR_WORKER_OBJ)
$(MGR_WORKER_OBJ): $(MGR_WORKER_SRC) $(SYNC_TASK_OBJ) $(CANCEL_CMD_OBJ) $(ADD_CMD_OBJ)  | $(OBJDIR)
	$(CC) $(CFLAGS) -c $< -o $@



client: $(CLIENT_BIN) 
$(CLIENT_BIN): $(CLIENT_OBJ) $(UTILS_OBJ) $(SOCKET_UTILS_OBJ) $(CLIENT_CON_OBJ) | $(BINDIR)
	$(CC) $(CFLAGS) $^ -o $@ -lpthread

$(CLIENT_OBJ): $(CLIENT_SRC) | $(OBJDIR)
	$(CC) $(CFLAGS) -c $< -o $@ 


manager: $(MANAGER_BIN) 
$(MANAGER_BIN): $(MANAGER_OBJ) $(UTILS_OBJ) $(SYNC_INFO_OBJ) $(SYNC_TASK_OBJ) $(ADD_CMD_OBJ) $(CANCEL_CMD_OBJ) $(SOCKET_UTILS_OBJ) $(MGR_WORKER_OBJ)| $(BINDIR)
	$(CC) $(CFLAGS) $^ -o $@ -lpthread

$(MANAGER_OBJ): $(MANAGER_SRC) | $(OBJDIR)
	$(CC) $(CFLAGS) -c $< -o $@ 


console: $(CONSOLE_BIN) 
$(CONSOLE_BIN): $(CONSOLE_OBJ) $(UTILS_OBJ)  $(SYNC_INFO_OBJ) $(SYNC_TASK_OBJ) $(SOCKET_UTILS_OBJ) $(MGR_READ_OBJ) | $(BINDIR)
	$(CC) $(CFLAGS) $^ -o $@ -lpthread

$(CONSOLE_OBJ): $(CONSOLE_SRC) | $(OBJDIR)
	$(CC) $(CFLAGS) -c $< -o $@ 


clean:
	@rm -rf obj bin

clean_dir:
	@rm source2/*
	@rm source3/*
	@rm source4/*
	@rm source5/*


.PHONY: all clean utils sync_info console manager 
