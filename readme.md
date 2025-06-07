README - Network File Synchronization System (SysPro 2025 - HW2)
=========================================================

- Student: [sdi2100223]
- Course: System Programming - K24
- Assignment: HW2 - Network File System - NFS
---------------------------------------------------------
1. Compilation Instructions
---------------------------------------------------------

To build everything:

    $ make

This creates:
- `bin/nfs_manager`
- `bin/nfs_console`
- `bin/nfs_client`

Build phases:

    $ make manager 
    $ make console
    $ make client
    $ make xxxxxx (all the utils linked with the above for seperate building)

To clean all generated files:

    $ make clean

This removes object files, binaries, named pipes, logs.

---------------------------------------------------------
2. Execution Instructions
---------------------------------------------------------

1. Launch the clients on a remote host (seperate terminals):

   ```
   $ bin/nfs_client -p port_number
   $ bin/nfs_client -p port_number
   ```

2. Launch the manager:


    ```
   $ bin/nfs_manager -l logfile -c configfile -n worker_limit -p port -b queue_buff_size
    ```

   flag, -n can be ignored with default worker limit: 5

2. Launch the console (in another terminal):

   ```
   $ bin/nfs_console -l logfile -h host_ip -p host_port
   ```


---------------------------------------------------------
3. Source File Structure
---------------------------------------------------------

```
src/
├── client/
│   ├── client_connection_handler.c  : Function utils used by threads, accomplishing parallel connections to the different hosts
│   └── read_from_manager.c          : Read command requests from manager's workers

├── commands/
│   ├── add_cmd.c                    : Implements the 'add' sync/enqueue command add 
│   └── cancel_cmd.c                 : Implements the 'cancel' sync/enqueue command

├── data_structures/
│   ├── sync_info_list.c             : Linked list managing sync directory metadata
│   └── sync_task_queue.c            : Thread-safe queue for managing add/cancel commands

├── main/
│   ├── nfs_client.c                 : Entry point for the NFS client
│   ├── nfs_console.c                : Entry point for the NFS console
│   └── nfs_manager.c                : Entry point for the NFS manager

├── manager/
│   └── manager_worker_pool.c        : Worker thread pool created by manager

├── utils/
│   ├── socket_utils.c               : Socket helper functions (connect, write, read)
│   └── utils.c                      : General-purpose utilities (parsing, arg_checking, reading files)

```
---------------------------------------------------------
4. Design
---------------------------------------------------------

Strictly followed the paper instructions and piazza suggestions.


---------------------------------------------------------
5. Example
---------------------------------------------------------

5.1 Config file sample (source/target directories must exist, otherwise error will be displayed):
```
source_dir1@xxx.xxx.xx.xx:2323 target_dir2@xxx.xxx.xx.xx:2424
source_dir1@xxx.xxx.xx.xx:2525 target_dir3@xxx.xxx.xx.xx:2626
```
5.2 Build
``` 
$ make
```
5.3.1 Run clients on each of the ips
``` 
bin/nfs_client -p 2323
bin/nfs_client -p 2424
bin/nfs_client -p 2525
bin/nfs_client -p 2626

```
5.3.2 Run the manager
 
```
bin/nfs_manager -l logs/manager.log -c config.txt -n 5 -p 2525 -b 10
```


5.3.3 Run the console
 
```
bin/nfs_console -l logs/console.log -h localhost -p 2525
```
  1. Log directories should exist, if given like this
  2. Can use host's IP as well instead of localhost

5.4
Directories listed in the configuration file will be fully synchronized. The initiation of tasks (ADD, PULL, PUSH) will be recorded in the manager log. Nothing  to be displayed to the console as it has nothing to do with the starting synchronization. Following, the user can request for commands as: 
```
> add source_dir1@xxx.xxx.xx.xx:2323 target_dir2@xxx.xxx.xx.xx:port
> cancel source_dir1
> cancel non_existent_dir
> shutdown
``` 

1. With the commands being recorded in the console's log
2. Initial responses from manager to nfs_console's console
3. Add/cancel commands recorded in manager's log
4. PUSH/PULL results stored in manager's log





