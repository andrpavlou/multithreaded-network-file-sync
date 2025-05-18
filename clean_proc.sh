#!/bin/bash

# Get the PID of the current bash session
BASH_PID=$$

# Get the TTY of the current session (to limit process killing to this terminal)
CURRENT_TTY=$(tty | sed 's:/dev/::')

# Get the list of processes in the current terminal, excluding bash and this script
for pid in $(ps -eo pid,tty,comm | awk -v bash_pid=$BASH_PID -v tty=$CURRENT_TTY '$2 == tty && $1 != bash_pid && $3 != "ps" && $3 != "grep" {print $1}'); do
    # Check if the process is a non-interactive command, and not a shell or terminal manager
    if ! ps -p $pid -o comm= | grep -q -e "^bash$" -e "^sh$" -e "^zsh$" -e "^screen$" -e "^tmux$"; then
        echo "Killing process: $pid"
        kill -9 $pid
    fi
done
clear