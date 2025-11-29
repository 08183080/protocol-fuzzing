#!/bin/bash  
  
DIR="./benchmark/subjects/DAAP/forked-daapd"  
  
if [ ! -d "$DIR" ]; then  
    echo "错误: forked-daapd目录不存在"  
    exit 1  
fi  
  
  
rm -r $DIR/aflnet 2>&1 >/dev/null  
cp -r aflnet $DIR/aflnet  
  
rm -r $DIR/chatafl 2>&1 >/dev/null  
cp -r ChatAFL $DIR/chatafl  

rm -r $DIR/xpgfuzz 2>&1 >/dev/null  
cp -r xpgfuzz $DIR/xpgfuzz
  

echo "构建forked-daapd Docker镜像..."  
cd $DIR  
docker build . -t forked-daapd --build-arg MAKE_OPT $NO_CACHE  
  
echo "forked-daapd Docker镜像构建完成！"  
  
# 验证镜像  
if docker images | grep -q "forked-daapd"; then  
    echo "✓ forked-daapd镜像已成功创建"  
    docker images | grep forked-daapd  
else  
    echo "✗ forked-daapd镜像创建失败"  
    exit 1  
fi
