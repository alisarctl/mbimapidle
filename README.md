# Introduction

**mbimapidle** maintains persistent server connection using IMAP protocol 
(RFC 2177) IDLE extension, it can be configured to run email sync command
when it receives notification from the imap server about new messages.

# Requirements & Installation

**mbimapidle** requires **openssl** version 3 and above, it comes with a simple BSD
style makefile (on Linux® often you just have to install bmake).

1. BSD make
2. openssl-3

## BSD
```
$ make
# make install PREFIX=/usr/local
```

## Linux (openrc) 
```
$ bmake
# bmake install PREFIX=/usr RC=openrc
```

## Linux (systemd)
```
$ bmake
# bmake install PREFIX=/usr RC=systemd
```

# Configuration file
Configuration file is located at

```
$XDG_CONFIG_HOME/mbimapidle/mbimapidlerc
```

which is normally located at 

```
~/.config/mbimapidle/mbimapidlerc
```

example

```mbimapidlerc
[general]
verbose="true"

[mbox1]
hostname = "hostname1"
password  =  "pass1" # Comment
username = "user1"
sync_cmd = "/usr/bin/mbsync -a"
idle_timeout = "10"
check_certificate="false"
port = "993"
tls_type="ssl"
auth = "plain"

[mbox2]
hostname = "hostname2"
password  =  "pass2" # Comment
username = "user2"
sync_cmd = "/usr/bin/mbsync -a"
idle_timeout = "20"
check_certificate="false"
port = "143"
tls_type = "starttls"
auth = "plain"

[gmail]
hostname = "imap.gmail.com"
pass_cmd  =  "/home/user/bin/renew-token"
username = "user2"
sync_cmd = "/usr/bin/mbsync -a"
port = "993"
check_certificate="true"
auth = "XOAUTH2"
```

# User service file

## openrc

/etc/user/init.d/mbimapidle

```
#!/sbin/openrc-run

description="Execute command on IMAP mailbox changes"
command="/usr/bin/mbimapidle"
supervisor=supervise-daemon
```

```
$ rc-update -U add mbimapidle
$ rc-service -U mbimapidle start
```

## systemd
TODO

# TODO
* Buffer size adaptation
* Redirect sync command output to logs
* Fix "FIXMEs"
* man page
* complete command line arguments

# DONE
* XOAUTH2 support
* password command configuration
* Plain communication

