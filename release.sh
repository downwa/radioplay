#!/bin/sh

date +"%Y%m%d%H%M%S" >vars/release.txt
#which boa >/dev/null || sudo apt-get install boa
sudo ln -sf $HOME/workspace/play3abn/patches/ /opt/lampp/htdocs/3abnrelease
sudo ln -sf $HOME/workspace /opt/lampp/htdocs/workspace
cd fullroot/
./mkpatch.sh
cd ../
sudo ln -sf $HOME/workspace/play3abn/fullroot-128M.img.bin /opt/lampp/htdocs/
