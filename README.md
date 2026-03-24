# Introduction

**mbimapidle** maintains persistent server connection using IMAP protocol 
(RFC 2177) IDLE extension, it can be configured to run email sync command
when it receives notification from the imap server about new messages.

# Requirements & Installation

**mbimapidle** only requires **openssl** version 3 and above, and it uses
[mk-configure](https://github.com/cheusov/mk-configure) for its build system,
a Lightweight and easy to deploy replacement for GNU autotools.

1. mk-configure
2. openssl-3

## BSD
```
$ mkcmake
# mkcmake install PREFIX=/usr/local
```

## Linux (openrc) 
```
$ mkcmake
# mkcmake install PREFIX=/usr RC=openrc RCDIR=/etc/user/init.d/
```

## Linux (systemd)
```
$ mkcmake
# mkcmake install PREFIX=/usr RC=systemd RCDIR=/etc/systemd/user
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

Check out [mbimapidlerc](./doc/mbimapidlerc) for a configuration example.

# User service file (openrc and systemd)

Using user service file

## openrc

```
$ rc-update -U add mbimapidle
$ rc-service -U mbimapidle start
```

## systemd
systemctl --user enable mbimapidle
systemctl --user start mbimapidle

# TODO
* man pages
* complete command line arguments

# Features
* STARTTLS, TLS support
* XOAUTH2 support
* password command configuration
* Plain communication

