#! /bin/bash

# Unsetting $TMUX in order to be able to launch new sessions from tmux
unset TMUX

echo "Killing ABCD"
tmux kill-session -t ABCD
