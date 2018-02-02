# entity-generator

Use this application to quickly load entity data into Conduce.

## prerequisites

This requires cmake and boost to build.  To install them on
osx run:

    brew install cmake
    brew install boost

## build/run

1. Clone repository
1. `cd entity-generator` #or whatever the new repo dir is
1. `mkdir build`
1. `cd build`
1. `cmake ../`
1. `make`
1. `cd ..`
1. `./build/src/entity-generator/entity-generator`

# usage

The library will provide the most recent up to date info:

    ./build/src/entity-generator/entity-generator --help

You will need an API key and a dataset to push data to.  If you do not have these,
the simplest method for creating them is via the conduce-python-api CLI, Install
the conduce-python-api and then either look at the help menu or read the docs for more info.
