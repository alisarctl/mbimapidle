# Introduction

mbimapidle maintains persistent server connection using IMAP protocol 
(RFC 2177) IDLE extension, it can be configured to run email sync command
when it receives notification from the imap server about new messages.

# Requirements & Installation

mbimapidle requires openssl version 3 and above, it comes with a simple BSD
style makefile (on Linux often you just have to install bmake).

## BSD
```
make
make install PREFIX=/usr/local
```

## Linux
```
$ bmake
# bmake install PREFIX=/usr
```

# Configuration file
Configuration file is located at

$XDG_CONFIG_HOME/mbimapidle/mbimapidlerc

which is normally located at 

~/.config/mbimapidle/mbimapidlerc

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
tls_type="ssl"
port = "993"

[mbox2]
hostname = "hostname2"
password  =  "pass2"
username = "user2"
sync_cmd = "/usr/bin/mbsync -a"
port = "143"
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

$ rc-update -U add mbimapidle
$ rc-service -U mbimapidle start

## systemd
TODO

# TODO
* Adaptative timeout
* configuration to skip certificate validation
* password command configuration
* SIGUSR1 support to restart the process
* oauth https://learn.microsoft.com/en-us/exchange/client-developer/legacy-protocols/how-to-authenticate-an-imap-pop-smtp-application-by-using-oauth
* Handle sync_command not found
* Redirect sync command output to logs
* Buffer size
* Fix "FIXMEs"
* man page
* complete command line arguments
