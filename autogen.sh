#! /bin/sh

mkdir -p m4
intltoolize --force --automake
autoreconf --install --force
