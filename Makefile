all:
	arm-linux-gnueabihf-gcc src/main.c src/getstream.c -o build/playaudioAPP \
	-I include \
	-L lib \
	-lasound -lavcodec -lavdevice -lavfilter -lavformat -lavutil -lpostproc -lswresample -lswscale

clean:
	rm build/*

copy:
	sudo cp build/playaudioAPP /home/ubuntu20/workdir/tftpdir/rootfs/home/greenqueen/ -f