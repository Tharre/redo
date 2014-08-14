# What is redo?
`redo` provides an elegant and minimalistic way of rebuilding files from source.
The design stems from Daniel J. Bernstein, who published a basic concept about
how `redo` should work on his [website](http://cr.yp.to/redo.html).

If you want more detailed information about `redo`, a working python
implementation already exists [here](https://github.com/apenwarr/redo).

# About this implementation
The goal of this project is not only to implement `redo` in C, but also to
rethink and improve it. As a consequence, this project will _not_ be compatible
with other implementations of `redo`.

# Project status [![Build Status](https://travis-ci.org/Tharre/redo.svg?branch=master)](https://travis-ci.org/Tharre/redo)
This is work in progress, many features are still missing and the behaviour is
not set in stone yet. Missing features include parallel builds, recursive
checking for do-files, automatic cleanup of built files and probably a lot more.
