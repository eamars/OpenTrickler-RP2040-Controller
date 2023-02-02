target remote tcp:localhost:3333

monitor reset init
monitor reset
monitor sleep 500

load

monitor halt
monitor reset
