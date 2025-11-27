#!/bin/bash  
  
if [ -z $KEY ]; then  
    echo "NO OPENAI API KEY PROVIDED! Please set the KEY environment variable"  
    exit 0  
fi  
  
echo "开始为重新构建Docker镜像..."  
  
# 更新API密钥（模仿setup.sh的步骤）  
for x in ChatAFL ChatAFL-CL1 ChatAFL-CL2 xpgfuzz;  
do  
  sed -i "s/#define OPENAI_TOKEN \".*\"/#define OPENAI_TOKEN \"$KEY\"/" $x/chat-llm.h  
done  
  
# 只复制ChatAFL变体到Live555目录  
LIVE555_DIR="./benchmark/subjects/HTTP/Lighttpd1"  
  
if [ ! -d "$LIVE555_DIR" ]; then  
    echo "错误: Lighttpd1目录不存在"  
    exit 1  
fi  
  
echo "复制ChatAFL变体到Lighttpd1目录..."  
  
rm -r $LIVE555_DIR/aflnet 2>&1 >/dev/null  
cp -r aflnet $LIVE555_DIR/aflnet  
  
rm -r $LIVE555_DIR/chatafl 2>&1 >/dev/null  
cp -r ChatAFL $LIVE555_DIR/chatafl  

rm -r $LIVE555_DIR/xpgfuzz 2>&1 >/dev/null  
cp -r xpgfuzz $LIVE555_DIR/xpgfuzz
  
  

echo "构建Lighttpd1 Docker镜像..."  
cd $LIVE555_DIR  
docker build . -t lighttpd1 --build-arg MAKE_OPT $NO_CACHE  
  
echo "lighttpd1 Docker镜像构建完成！"  
  
# 验证镜像  
if docker images | grep -q "lighttpd1"; then  
    echo "✓ Lighttpd1镜像已成功创建"  
    docker images | grep Lighttpd1  
else  
    echo "✗ Lighttpd1镜像创建失败"  
    exit 1  
fi
