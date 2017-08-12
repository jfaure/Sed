if ! [ -f genexpr.out ]; then g++ -o genexpr.out exprgen.cpp; fi
(( (arg = 0$1) || (arg = 3) ))
test=$(./genexpr.out $arg)
echo $test
diff <(sed -f sedcalc <<< $test) <(bc <<< $test)
