# Notes

- `select()` system call takes linear time with the amount of sockets to monitor and this is limitation of the kernel.
- `epoll()` and `kqueue()` are modern replacement which are efficient and offer more events.
- `listen()` call is non-blocking. OS raises an event on socket when a connection arrives. [More Info](https://stackoverflow.com/questions/4530187/what-happens-when-we-say-listen-to-a-port)
- `select()`, `poll()`, `epoll()`, and `kqueue()` are non-blocking mechanisms.
- `overlapped I/O`, `completion ports` are asynchronous mechanisms.

# Tools

Following are the list of CLI programs on MacOS for network inspection:

- `ifconfig`: lists out info about network interfaces
- `kextstat`: displays kernel extension stats
- `networksetup`
- `odutil`
- `dns-sd`
- `/etc/inetd.conf`: configure console-only app into networked ones. [merged into launchd](https://en.wikipedia.org/wiki/Inetd#:~:text=As%20of%20version%20Mac%20OS,dedicated%20to%20a%20single%20function.)
- `file --mime-type <filename>`: displays `Content-Type` value which can be used for HTTP header data
