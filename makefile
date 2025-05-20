CC = gcc

CFLAGS 		= -Wall -Iinclude
OBJDIR 		= obj
SRCDIR 		= src
BINDIR 		= bin
UTILSDIR	= utils


CLIENT_SRC 		= $(SRCDIR)/nfs_client.c
MANAGER_SRC 	= $(SRCDIR)/nfs_manager.c
UTILS_SRC 		= $(UTILSDIR)/utils.c
SYNC_INFO_SRC	= $(UTILSDIR)/sync_info.c
SYNC_TASK_SRC	= $(UTILSDIR)/sync_task.c


CLIENT_OBJ 		= $(OBJDIR)/nfs_client.o
MANAGER_OBJ 	= $(OBJDIR)/nfs_manager.o
UTILS_OBJ 		= $(OBJDIR)/utils.o
SYNC_INFO_OBJ 	= $(OBJDIR)/sync_info.o
SYNC_task_OBJ 	= $(OBJDIR)/sync_task.o


CLIENT_BIN 	= $(BINDIR)/nfs_client
MANAGER_BIN = $(BINDIR)/nfs_manager


all: client utils manager sync_info sync_task

$(OBJDIR):
	mkdir -p $(OBJDIR)

$(BINDIR):
	mkdir -p $(BINDIR)

utils: $(UTILS_OBJ)
$(UTILS_OBJ): $(UTILS_SRC) | $(OBJDIR)
	$(CC) $(CFLAGS) -c $< -o $@

sync_infos: $(SYNC_INFO_OBJ)
$(SYNC_INFO_OBJ): $(SYNC_INFO_SRC) | $(OBJDIR)
	$(CC) $(CFLAGS) -c $< -o $@

sync_task: $(SYNC_TASK_OBJ)
$(SYNC_TASK_OBJ): $(SYNC_TASK_SRC) | $(OBJDIR)
	$(CC) $(CFLAGS) -c $< -o $@



client: $(CLIENT_BIN) 
$(CLIENT_BIN): $(CLIENT_OBJ) $(UTILS_OBJ) | $(BINDIR)
	$(CC) $(CFLAGS) $^ -o $@

$(CLIENT_OBJ): $(CLIENT_SRC) | $(OBJDIR)
	$(CC) $(CFLAGS) -c $< -o $@


manager: $(MANAGER_BIN) 
$(MANAGER_BIN): $(MANAGER_OBJ) $(UTILS_OBJ)  $(SYNC_INFO_OBJ) $(SYNC_TASK_OBJ) | $(BINDIR)
	$(CC) $(CFLAGS) $^ -o $@

$(MANAGER_OBJ): $(MANAGER_SRC) | $(OBJDIR)
	$(CC) $(CFLAGS) -c $< -o $@




clean:
	@rm -rf obj bin



.PHONY: all clean utils sync_info console manager 
