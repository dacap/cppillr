#! /bin/bash

if [[ "$CPPILLR" == "" ]] ; then
    CPPILLR="cppillr"
fi

return_expr_cpp=$(pwd)/return_expr.cpp

expect_return_expr() {
    expected="$1"
    expr="$2"

    echo -n $(pwd)/return_expr.cpp
    cat return_expr.cpp | \
	sed -e "s/\${EXPR}/$expr/" | \
	tee _tmp.cpp | \
	$CPPILLR run -- >_stdout
    actual="$?"

    if [ "$actual" == "$expected" ] ; then
        echo ": ok $expr"
    else
        echo ":1: failed $expr, expected exit code=$expected, actual=$actual"
        exit 1
    fi
}

# Expect a specific return value for the given expression
expect_return_expr 1 1
expect_return_expr 5 5
