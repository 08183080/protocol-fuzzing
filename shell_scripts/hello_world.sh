#!/bin/bash

#写一个最简单的脚本，输出"Hello, World!"

# echo "Hello, World!" 

echo_str="Hello, World!"
# echo $echo_str

test() {
    echo "全局变量值：$echo_str"
    echo "传入的第一个参数值：$1"
    echo "传入的第二个参数值：$2"
    echo "传入的第三个参数值：$3"
}

test $echo_str 111