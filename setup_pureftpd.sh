#!/bin/bash  

DIR="./benchmark/subjects/FTP/PureFTPD"  
  
if [ ! -d "$DIR" ]; then  
    echo "错误: purepd目录不存在"  
    exit 1  
fi  
  
  
rm -r $DIR/aflnet 2>&1 >/dev/null  
cp -r aflnet $DIR/aflnet  
  
rm -r $DIR/chatafl 2>&1 >/dev/null  
cp -r ChatAFL $DIR/chatafl  

rm -r $DIR/xpgfuzz 2>&1 >/dev/null  
cp -r xpgfuzz $DIR/xpgfuzz
  
  
echo "构建pureftpd Docker镜像..."  
cd $DIR  
docker build . -t pure-ftpd --build-arg MAKE_OPT $NO_CACHE  
  
echo "pureftpd Docker镜像构建完成！"  
  
# 验证镜像  
if docker images | grep -q "pure-ftpd"; then  
    echo "✓ pureftpd镜像已成功创建"  
    docker images | grep pureftpd  
else  
    echo "✗ pureftpd镜像创建失败"  
    exit 1  
fi