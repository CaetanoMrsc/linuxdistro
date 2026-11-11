#!/bin/sh

cp $BASE_DIR/../custom-scripts/S41network-config $BASE_DIR/target/etc/init.d
chmod +x $BASE_DIR/target/etc/init.d/S41network-config

cp $BASE_DIR/../custom-scripts/systeminfo.py $BASE_DIR/target/usr/bin
chmod +x $BASE_DIR/target/usr/bin/systeminfo.py

cp $BASE_DIR/../custom-scripts/S42systeminfo $BASE_DIR/target/etc/init.d
chmod +x $BASE_DIR/target/etc/init.d/S42systeminfo

# Tutorial 3.1 - SCHED_LOW_IDLE: compila o thread_runner para o alvo
$HOST_DIR/bin/i686-buildroot-linux-uclibc-gcc -o $BASE_DIR/target/usr/bin/thread_runner $BASE_DIR/../custom-scripts/thread_runner.c -lpthread

# Tutorial 3.1 - Ftrace: monta o tracefs no boot
grep -q tracefs $BASE_DIR/target/etc/fstab || \
	echo "tracefs		/sys/kernel/tracing	tracefs	defaults	0	0" >> $BASE_DIR/target/etc/fstab

# Tutorial 3.2 - Atividade 2: cinco threads imprimindo letras
$HOST_DIR/bin/i686-buildroot-linux-uclibc-gcc -o $BASE_DIR/target/usr/bin/threads $BASE_DIR/../custom-scripts/threads.c -lpthread
