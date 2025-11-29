#!/bin/bash  
  
LIVE555_DIR="./benchmark/subjects/RTSP/Live555"  
  
if [ ! -d "$LIVE555_DIR" ]; then  
    echo "错误: Live555目录不存在"  
    exit 1  
fi  
  
rm -r $LIVE555_DIR/aflnet 2>&1 >/dev/null  
cp -r aflnet $LIVE555_DIR/aflnet  
  
rm -r $LIVE555_DIR/chatafl 2>&1 >/dev/null  
cp -r ChatAFL $LIVE555_DIR/chatafl  

rm -r $LIVE555_DIR/xpgfuzz 2>&1 >/dev/null  
cp -r xpgfuzz $LIVE555_DIR/xpgfuzz
  
  
# 构建Live555 Docker镜像  
echo "构建Live555 Docker镜像..."  
cd $LIVE555_DIR  
docker build . -t live555 --build-arg MAKE_OPT $NO_CACHE  
  
echo "Live555 Docker镜像构建完成！"  
  
# 验证镜像  
if docker images | grep -q "live555"; then  
    echo "✓ live555镜像已成功创建"  
    docker images | grep live555  
else  
    echo "✗ live555镜像创建失败"  
    exit 1  
fi
