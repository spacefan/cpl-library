#!/bin/bash
if [ -z "$CPL_PATH" ] 
then
    echo "CPL_PATH variable is not set."
else
    export CPL_LIBRARY_PATH="$CPL_PATH/lib"
    if [ -z "$PYTHONPATH" ] 
    then
        export PYTHONPATH="$CPL_PATH/src/bindings/python"
    else
        export PYTHONPATH="$CPL_PATH/src/bindings/python:$PYTHONPATH"
    fi
    export CPLPY_PATH="$CPL_PATH/src/bindings/python/cplpy"
fi