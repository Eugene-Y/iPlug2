set pagination off
handle SIGPIPE nostop noprint
run
info threads
bt 12
thread apply all bt 4
