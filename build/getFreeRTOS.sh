#!/bin/sh 

git clone git://github.com/foss-xtensa/amazon-freertos.git FreeRTOS
cd FreeRTOS
git checkout 4864ea463f2fd9eca8029c6796675f31f31acf50
cd lib/FreeRTOS/portable/XCC/Xtensa
xt-make clean
xt-make
