#!/bin/bash  
  

DIR="./benchmark/subjects/HTTP/Lighttpd1"  
  
if [ ! -d "$DIR" ]; then  
    echo "错误: Lighttpd1目录不存在"  
    exit 1  
fi  
  
  
rm -r $DIR/aflnet 2>&1 >/dev/null  
cp -r aflnet $DIR/aflnet  
  
rm -r $DIR/chatafl 2>&1 >/dev/null  
cp -r ChatAFL $DIR/chatafl  

rm -r $DIR/xpgfuzz 2>&1 >/dev/null  
cp -r xpgfuzz $DIR/xpgfuzz
  
  

echo "构建Lighttpd1 Docker镜像..."  
cd $DIR  
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