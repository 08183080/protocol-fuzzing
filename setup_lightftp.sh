#!/bin/bash  
  

DIR="./benchmark/subjects/FTP/LightFTP"  
  
if [ ! -d "$DIR" ]; then  
    echo "错误: lightftpd目录不存在"  
    exit 1  
fi  
  
echo "复制ChatAFL变体到proftpd目录..."  
  
rm -r $DIR/aflnet 2>&1 >/dev/null  
cp -r aflnet $DIR/aflnet  
  
rm -r $DIR/chatafl 2>&1 >/dev/null  
cp -r ChatAFL $DIR/chatafl  

rm -r $DIR/xpgfuzz 2>&1 >/dev/null  
cp -r xpgfuzz $DIR/xpgfuzz
  
# 构建proftpd Docker镜像  
echo "构建proftpd Docker镜像..."  
cd $DIR  
docker build . -t lightftp --build-arg MAKE_OPT $NO_CACHE  
  
echo "lightftp Docker镜像构建完成！"  
  
# 验证镜像  
if docker images | grep -q "lightftp"; then  
    echo "✓ lightftpd镜像已成功创建"  
    docker images | grep lightftp  
else  
    echo "✗ lightftpd镜像创建失败"  
    exit 1  
fi