#!/bin/sh

echo "Deploying site using rsync(1)..."
rm -f radio.iglooware.com/3abnrelease/*~ # Don't deploy backup files
rm -f radio.iglooware.com/admin/*~ # Don't deploy backup files
rsync --group --progress -avz -e "ssh -l wdowns" --delete radio.iglooware.com/admin/ radio.iglooware.com:/home/wdowns/www/radio.iglooware.com/admin/
echo "Deploying site using rsync(2)..."
rsync --group --progress -avz -e "ssh -l wdowns" --delete radio.iglooware.com/3abnrelease/ radio.iglooware.com:/home/wdowns/www/radio.iglooware.com/3abnrelease/
echo "Deploying site using scp(3)..."
scp radio.iglooware.com/* wdowns@radio.iglooware.com:/home/wdowns/www/radio.iglooware.com/
true
