set pagination off
handle SIGPIPE nostop noprint
run
info threads
bt 16
thread apply all bt 6
