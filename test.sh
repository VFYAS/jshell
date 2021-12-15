#!/bin/bash
TEST="tests_main/tests/test"
ANS="tests_main/keys/test"
TESTER=tests_main/tester
MAIN=solution
TESTS_AMOUNT=6
VALGRIND_FLAGS="--leak-check=full --show-leak-kinds=all --track-origins=yes --verbose"

for i in $(seq 0 $TESTS_AMOUNT)
do
    case $i in
        5) code=127;;
        1|3|4) code=0;;
        2) code=2;;
        *) code=1;;
    esac
    echo -n $i": "
    $TESTER $MAIN $TEST$i $ANS$i $code
    if [[valgrind ./$MAIN < $TEST$i &> /dev/null --error-exitcode=1 $VALGRIND_FLAGS]]
    then
        echo $i "test failed valgrind!"
    fi
done
