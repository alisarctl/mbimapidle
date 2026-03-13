#!/bin/sh

set -ex

PACKAGES="
  libssl-dev
  bmake
"

sudo -E apt-get -y install $PACKAGES
