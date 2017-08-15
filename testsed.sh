readarray tests <tests.txt

input=$(seq 5)
for a in "${tests[@]}"; do
mine=$(sed=./sed; eval echo "$a")
sed="$(sed=sed; eval echo "$a")"
echo -e "\e[94m$sed\e[0m"
diff <($mine <<< $input) <($sed <<< $input)
done
