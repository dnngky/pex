#! /bin/bash

TOTAL=0
PASSED=0
for file in ./tests/E2E/*.in; do
    TOTAL=$((TOTAL+1))
    echo -e "test: \033[3;37m${file%.*}\033[0m"
    RESULT=$(./pe_exchange products.txt ${file%.*} | diff ${file%.*}.out -)
    if [[ -z "$RESULT" ]]; then
        echo -e "\033[1;32mPASSED\033[0m"
        PASSED=$((PASSED+1))
    else
        echo -e "\033[1;31mFAILED\033[0m"
        echo -e "$RESULT"
    fi
done
echo -e "\033[1;35m$(((PASSED*100)/TOTAL))% ($PASSED/$TOTAL) tests PASSED.\033[0m"