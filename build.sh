#!/bin/bash
git submodule update --init --recursive
export PATH=$PATH:/home/build/inkbox/compiled-binaries/arm-kobo-linux-gnueabihf/bin/
make distclean
/home/build/inkbox/compiled-binaries/qt-bin/qt-linux-5.15.2-kobo/bin/qmake .
make -j$(nproc)