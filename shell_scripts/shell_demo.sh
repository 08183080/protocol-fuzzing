#!/bin/bash
# 2025-11-29, 谢龙龙浪子回头, 准备苦练编程, 以在上海有立锥之地, 11月底于实验室


printf "Hello, shell\n"


if [ 2 -ne 1 ]; then
    echo "2 not equal 1" 
else
    echo "2 equal 1"
fi

printf $$
printf "\n"


val=`expr 1 + 1`  # 不要有额外的空格
printf  "两数之和为：$val\n"

