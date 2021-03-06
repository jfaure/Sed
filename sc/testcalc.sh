tests=( # generate more by looping ./gentest.sh
    '1+1'
    'lel'
    '1+9'
    '1+999'
    '1-99'
    '1*0'
    '0*2'
    '3*0'
    '2*2'
    '5 * 6'
    '(3 * 4)'
    '(1 + 5) / (2 - 5)'
    '4 * 5 -(-3)'
    '2-9*(2)'
    '11 % -6 + 2 * 9 / 8'
    '8 % -5'
    '9/-(-3-9*2)+5'
    '5+(2-2)'
    '4*2-9+5-9/2/2+4'
    '2*2*2*2'
    '9%5/2'
    '12/2/2%2'
    '12/5*2'
    '2-3*9'
    '1-1-1-1-1-1-1-1-1-(1-1-1-1-1)'
    '-1-(-1-(-1-(-1)))'
    '2*3/2*3/2%9'
    '1*(2*(3*(4*(5*(6)))))'
    '9/9/9/9%2%2+9%4/1/1*(-7-2-1+2*-2-4)'
    '-(-6-(-2-5-7))-4-(-(5-10))-(3-9+4-12)'
    '9*(2-4%6/-1)/-(-3-8*-2)-(7-(2/4%3*2*4-6+(5-2-9*-9)))'
    '1*2*3*(1+2+3+(1-2-3-(1%2%3%(1/2/3))))'
    '-3-2-(-1+-1+-2)/-(2-1)-7%-5+2'
    '-2*-3-(2)%-2-0%7/6%(9/-4)/6'
    '-7*7*(4-6*(-4%8-(8)*-1*8+5))'
    '1*2*(-0)*(-9*-7%5/1%(6+-4+0))'
    '-6%4*1+2-1-(7-(-3+-6)/-6*0)'
    '1%-6%-7%1'
    '9*5+0%(4)'
    '9*4*-0*3'
    '8-6%(6)-8'
    '2%(-7+7)+(4)'
    '-9%-4/7/9'
    '-3%7/5/(2)'
    '8+0/(2+8)'
    '-4/5+-4-9'
    '-8%1%9%(8)'
    '-1/-3*0'
    '-0*(2-7)'
    '2+-0-(6)'
    '1/-5*0'
    '-0%(-3%(0))'
    '-0*(-9%2)'
    '-0-6/1'
    '-9*-8/-0+2'
    '-2-6/(2)/2'
    '-1+(0)/-5%7'
    '-3-4/(1-2)'
    '-4-3/(-4%(9))'
    '5-4%4/6'
    '-4*(0)*(-0)%1'
    '-8*4*(0)*5'
    '-2%-6/-2%2'
    '-1%2/-4/(8)'
    '-1*-0+(-2%5)'
    '4%7*-7%4'
    '5-4*(-0)-(8)'
    '3*0+2-0'
    '-0*0%-4/4'
    )
let 'ok=ko=0'
for a in "${tests[@]}"; do
  if save=$(diff \
    <($1sed -f sedcalc <<< "$a" 2>/dev/null) \
    <(bc <<< "$a" 2>/dev/null)); then
    let ++ok;
  else
    if ((!ko)); then echo failed:; fi
    let ++ko;
    echo -e "\e[91m$a";
  fi
done

let 'result=100 * ok / (save=(ok + ko))'
echo -e "\e[94m\n--- $result% ($ok/$save) ---"
