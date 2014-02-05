#!/bin/sh

echo "Deploying application using rsync..."
rsync --group --progress -avz -e "ssh -l wdowns" --exclude '*/dev/*' --delete fullroot/ radio.iglooware.com:/home/wdowns/fullroot/
