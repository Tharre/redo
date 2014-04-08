# What is redo?
`redo` provides an elegant and minimalistic way of rebuilding files from source. The design stems from Daniel J. Bernstein, who published a basic concept about how `redo` should work on his [website](http://cr.yp.to/redo.html).

If you want more detailed information about `redo`, a working python implementation of `redo` already exists [here](https://github.com/apenwarr/redo).

# About this implementation
The goal of this project is not only to implement `redo` in C, but also to rethink and improve it. This also means that this project will not try be compatible with other implementations of `redo`, although this shouldn't prove problematic.

# Project status
The is work in progress, many features are still missing and the behaviour is not set in stone yet. Missing features include dependency checking, parallel builds, recursive checking and probably a lot more.