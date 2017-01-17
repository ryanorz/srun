
# Tutorials

## Usage

```sh
Usage: srunctl list
       srunctl stop PIDS
       srunctl [OPTIONS] -c CMDARGS

        -o outfile   Redirect stdout to outfile
        -e errfile   Redirect stdout to outfile
        -t timeout   Set command execute timeout
        -u user      Set command execute user
        -r chroot    Set command execute chroot
        -g group     Use config of the group name
        -n           Use network
        -q           Quiet mode, do not care output and errput
        -b           Process execute at backend
        -c CMDARGS   Command and arguments
```

## Example1: Execute command `ls`

### `ls /`
```sh
shiy@shiy-pc:~$ srunctl ls
-------------- stdout --------------
bin
boot
cdrom
dev
etc
home
initrd.img
lib
lib64
lost+found
media
mnt
opt
proc
root
run
sbin
srv
sys
tmp
usr
var
vmlinuz
```
The command above will show the result of "ls /". Because the default chroot is "/".

### `ls /root`

```sh
shiy@shiy-pc:~$ srunctl ls /root
stat:   Normal
retval: 2
-------------- stderr --------------
/bin/ls: cannot open directory '/root': Permission denied
```
Because the default program execution user is `nobody`.
We should do it like this.

```sh
shiy@shiy-pc:~$ srunctl -u root -c ls /root
stat:   Normal
retval: 0
```
or like this

```sh
shiy@shiy-pc:~$ srunctl -u root -c ls -a /root
stat:   Normal
retval: 0
-------------- stdout --------------
.
..
.aptitude
.bash_history
.bashrc
.cache
.config
.dbus
.debug
.gconf
.kde
.lesshst
.profile
.recently-used
.ssh
.viminfo
```

## Example2: Execute command `ip addr`

```sh
shiy@shiy-pc:~$ srunctl ip addr
stat:   Normal
retval: 0
-------------- stdout --------------
1: lo: <LOOPBACK> mtu 65536 qdisc noop state DOWN group default qlen 1
    link/loopback 00:00:00:00:00:00 brd 00:00:00:00:00:00

```
Because the service has isolated the network.

```sh
shiy@shiy-pc:~$ srunctl -n -c ip addr
stat:   Normal
retval: 0
-------------- stdout --------------
1: lo: <LOOPBACK,UP,LOWER_UP> mtu 65536 qdisc noqueue state UNKNOWN group default qlen 1
    link/loopback 00:00:00:00:00:00 brd 00:00:00:00:00:00
    inet 127.0.0.1/8 scope host lo
       valid_lft forever preferred_lft forever
    inet6 ::1/128 scope host
       valid_lft forever preferred_lft forever
2: enp2s0: <BROADCAST,MULTICAST,UP,LOWER_UP> mtu 1500 qdisc pfifo_fast state UP group default qlen 1000
    link/ether d4:be:d9:e3:03:91 brd ff:ff:ff:ff:ff:ff
    inet 192.168.220.117/24 brd 192.168.220.255 scope global enp2s0
       valid_lft forever preferred_lft forever
    inet6 fe80::d6be:d9ff:fee3:391/64 scope link
       valid_lft forever preferred_lft forever
```

## Example3: Execute a dead loop program "while" at backend

```sh
shiy@shiy-pc:~$ srunctl -u shiy -b /home/shiy/while
stat:   Normal
retval: 0
shiy@shiy-pc:~$ srunctl list
PID     RUNTIME NETWORK BACKEND TIMEOUT CMD
8488    0:0:4   0       1       0       /home/shiy/while
shiy@shiy-pc:~$ srunctl stop 8488
shiy@shiy-pc:~$ srunctl list
PID     RUNTIME NETWORK BACKEND TIMEOUT CMD
```

1. Execute a dead loop program at backend.
2. List all running in srund service.
3. Stop the running process.
4. List all running in srund service.

Set timeout 5 seconds, the process will stop by SIGTERM after timeout.

```sh
shiy@shiy-pc:~$ srunctl -u shiy -t 5 -b /home/shiy/while
stat:   Normal
retval: 0
shiy@shiy-pc:~$ srunctl list
PID     RUNTIME NETWORK BACKEND TIMEOUT CMD
8546    0:0:2   0       1       5       /home/shiy/while
shiy@shiy-pc:~$ srunctl list
PID     RUNTIME NETWORK BACKEND TIMEOUT CMD
8546    0:0:4   0       1       5       /home/shiy/while
shiy@shiy-pc:~$ srunctl list
PID     RUNTIME NETWORK BACKEND TIMEOUT CMD
```
