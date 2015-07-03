#!/bin/sh
#-------------------------------------------------------------------------------
# Change here, if needed

QT_FLAGS="-I /usr/include/qt/ -fPIC"

#-------------------------------------------------------------------------------

CXX="clang++ -Qunused-arguments -Xclang -load -Xclang ClangMoreWarningsPlugin.so -Xclang -add-plugin -Xclang more-warnings -c ${QT_FLAGS}"

for folder in */ ; do
    cd ${folder}
    $CXX main.cpp -o /tmp/foo.o &> compile.output

    if [ ! $? ] ; then echo "build error! See ${folder}compile.output" ; exit -1 ; fi

    grep "warning:" compile.output &> test.output

    if ! diff -q test.output test.expected &> /dev/null ; then
        echo "[FAIL] $folder"
        echo
        diff -Naur test.output test.expected
    else
        echo "[OK] $folder"
    fi

    cd ..
done