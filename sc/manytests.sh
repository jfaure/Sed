if ! [ -f genexpr.out ]; then g++ -o genexpr.out exprgen.cpp; fi
count=1000
(( (arg = 0$1) || (arg = 3) ))
for a in $(seq $count); do
  test=$(./genexpr.out $arg)
  if ! diff \
    <(sed -f sedcalc <<< $test 2>/dev/null) \
    <(bc <<< $test 2>/dev/null); then
    echo $test
    let ++ok
  fi
done
echo failed $ok / $count
