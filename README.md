# srun
This is a service for generating a sandbox to execute commands.

## Features

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

