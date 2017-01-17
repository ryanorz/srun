# srun
This is a service for generating a sandbox to execute commands.
Give you a security environment to execute the third-party command and service.

只想安安静静的执行一个命令，怎么就那么难

* 担心程序使用用内存太多，系统卡掉怎么办
* 担心程序使用网络搞破坏怎么办
* 担心死循环不退出而想设置超时怎么办
* 需要限制执行优先级怎么办
* 想要降权或者提权，或只想以匿名身份执行怎么办
* 需要执行太多命令难管理怎么办
* 不是想执行个命令就要开个虚拟机或者装个docker吧
* 命令执行结果要与其他程序交互怎么办
* ...

## Features

* Small, fast and utilizing system resource little.
* Using CGroups to restrict memory use.
* Using namespaces to restrict network use.
* Set command chroot.
* Set command uid/gid.
* Set command timeout.
* Get command output.
* Set command executing at backend.
* A tool `srunctl` is to control command execution.

# Dependencies

| Type    | Name             | Dev Version | Min Version | ubuntu                  | rhel             |
|--------:|-----------------:|------------:|------------:|------------------------:|-----------------:|
| Kernal  | linux            | 4.8.15      | 2.6.27      | -                       | -                |
| Build   | g++              | 5.3.1       | 4.8         | g++                     | gcc-c++          |
|         | cmake            | 3.5.1       | 3.0         | cmake                   | cmake            |
|         | make             | 4.1         |             | make                    | make             |
|         | pkg-config       | 0.29        |             | pkg-config              | pkgconfig        |
|         | protoc           | 3.0, 2.6.1  | 2.6.1       | protobuf-compiler       | protobuf-compiler|
| Service | systemd          | 229         |             | systemd                 | systemd          |
|         | rsyslog          | 8.16.0      |             | rsyslog                 | rsyslog          |
| Library | boost-system     | 1.58        | 1.58        | libboost-system-dev     | boost-devel      |
|         | boost-filesystem | 1.58        | 1.58        | libboost-filesystem-dev | boost-devel      |
|         | libprotobuf      | 3.0, 2.6.1  | 2.6.1       | libprotobuf-dev         | protobuf         |

# Build && install

```sh
$ cd <Source Dir>
$ mkdir build && cd build
$ cmake -DCMAKE_INSTALL_PREFIX=<Prefix> ..
$ make
$ sudo make install
```

# Start the service

Before you use the tool `srunctl`, you should start the service.

```sh
$ sudo systemctl start srund
```

# Tutorials

[Tutorials入门手册](doc/tutorials.md)
