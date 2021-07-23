#! /bin/bash

expect_ret() {
    expected=$1
    cppfile=$(pwd)/$2.cpp
    args=$3

    echo -n $cppfile
    cppillr run -showtokens $cppfile $args >stdout || exit 1
    actual="$?"

    if [ "$actual" == "$expected" ] ; then
        echo ": ok"
    else
        echo ":1: failed, expected exit code=$expected, actual=$actual"
        exit 1
    fi
}

# Expect a specific return value
expect_ret 42 ret42
