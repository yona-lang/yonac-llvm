#!/usr/bin/env sh

cloc --exclude-list-file=.gitignore --exclude-dir=vcpkg_installed,out,lib,node_modules,.cache .
