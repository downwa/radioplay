#!/bin/sh

echo "60-($(cat ../.dlapsed.txt)*60)" | bc
