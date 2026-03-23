#!/bin/sh

set -ex

PACKAGES="
  pkg-config
  libssl-dev
  mk-configure
"

sudo -E apt-get -y install $PACKAGES
