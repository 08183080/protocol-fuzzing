#!/bin/bash  
  
if [ -z $KEY ]; then  
    echo "NO OPENAI API KEY PROVIDED! Please set the KEY environment variable"  
    exit 0  
fi  
  
echo "开始为exim重新构建Docker镜像..."  
  
# 更新API密钥（模仿setup.sh的步骤）  
for x in ChatAFL ChatAFL-CL1 ChatAFL-CL2 xpgfuzz;  
do  
  sed -i "s/#define OPENAI_TOKEN \".*\"/#define OPENAI_TOKEN \"$KEY\"/" $x/chat-llm.h  
done  
  
# 只复制ChatAFL变体到Live555目录  
LIVE555_DIR="./benchmark/subjects/SMTP/Exim"  
  
if [ ! -d "$LIVE555_DIR" ]; then  
    echo "错误: exim目录不存在"  
    exit 1  
fi  
  
echo "复制ChatAFL变体到exim目录..."  
  
rm -r $LIVE555_DIR/aflnet 2>&1 >/dev/null  
cp -r aflnet $LIVE555_DIR/aflnet  
  
rm -r $LIVE555_DIR/chatafl 2>&1 >/dev/null  
cp -r ChatAFL $LIVE555_DIR/chatafl  

rm -r $LIVE555_DIR/xpgfuzz 2>&1 >/dev/null  
cp -r xpgfuzz $LIVE555_DIR/xpgfuzz
  
# rm -r $LIVE555_DIR/chatafl-cl1 2>&1 >/dev/null  
# cp -r ChatAFL-CL1 $LIVE555_DIR/chatafl-cl1  
  
# rm -r $LIVE555_DIR/chatafl-cl2 2>&1 >/dev/null  
# cp -r ChatAFL-CL2 $LIVE555_DIR/chatafl-cl2  
  
# 构建exim Docker镜像  
echo "构建exim Docker镜像..."  
cd $LIVE555_DIR  
docker build . -t exim --build-arg MAKE_OPT $NO_CACHE  
  
echo "exim Docker镜像构建完成！"  
  
# 验证镜像  
if docker images | grep -q "exim"; then  
    echo "✓ exim镜像已成功创建"  
    docker images | grep exim  
else  
    echo "✗ exim镜像创建失败"  
    exit 1  
    fii
