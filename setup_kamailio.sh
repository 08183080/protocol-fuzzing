#!/bin/bash  
  
DIR="./benchmark/subjects/SIP/Kamailio"  
  
if [ ! -d "$DIR" ]; then  
    echo "错误: kamailio目录不存在"  
    exit 1  
fi  
  

  
rm -r $DIR/aflnet 2>&1 >/dev/null  
cp -r aflnet $DIR/aflnet  
  
rm -r $DIR/chatafl 2>&1 >/dev/null  
cp -r ChatAFL $DIR/chatafl  

rm -r $DIR/xpgfuzz 2>&1 >/dev/null  
cp -r xpgfuzz $DIR/xpgfuzz
  

echo "构建kamailio Docker镜像..."  
cd $DIR  
docker build . -t kamailio --build-arg MAKE_OPT $NO_CACHE  
  
echo "kamailio Docker镜像构建完成！"  
  
# 验证镜像  
if docker images | grep -q "kamailio"; then  
    echo "✓ kamailio镜像已成功创建"  
    docker images | grep kamailio  
else  
    echo "✗ kamailio镜像创建失败"  
    exit 1  
fi