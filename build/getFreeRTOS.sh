#!/bin/sh 

git clone git://github.com/foss-xtensa/amazon-freertos.git FreeRTOS
cd FreeRTOS
git checkout 225cbe0e468c38e62264e8ba5128fccaea347033 
cd portable/XCC/Xtensa
xt-make clean
xt-make
