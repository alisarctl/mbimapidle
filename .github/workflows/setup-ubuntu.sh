#!/bin/sh

set -ex

PACKAGES="
  pkg-config
  libssl-dev
  bmake
"

sudo -E apt-get -y install $PACKAGES
