#!/bin/sh

set -e

AGR1="$1"
ARG2="$2"

if [ -z "$AGR1" ]; then
    BRANCH=master
fi
BRANCH=$AGR1

if [ -z "$ARG2" ]; then
    REPO_URL=https://atomgit.com/openeuler/ubturbo.git
fi
REPO_URL=$ARG2

# 拉取制品仓（构建仓）
rm -rf ubturbo
git clone -b openEuler-24.03-LTS-SP3 https://gitcode.com/SMAP_Build/ubturbo.git
cd ubturbo

# 在制品仓中进行操作
git clone -b "$BRANCH" "$REPO_URL"

cd ubturbo
git log -2
cd ..

rm -rf ubturbo-1.0.0.tar.gz
tar -zcf ubturbo-1.0.0.tar.gz ubturbo/
rm -rf ubturbo/

git add ubturbo-1.0.0.tar.gz
git commit -m "update: $REPO_URL:$BRANCH"
git push origin openEuler-24.03-LTS-SP3

cd ..
rm -rf ubturbo