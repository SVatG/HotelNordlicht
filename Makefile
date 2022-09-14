#3DS_IP := 10.1.36.183
#3DS_IP := 192.168.6.217
# 3DS_IP := 10.1.1.116
#3DS_IP = 192.168.8.96
#3DS_IP := 172.20.10.8
3DS_IP := 192.168.1.243

all: upload

clean:
	$(MAKE) -C build clean

binary:
	$(MAKE) -C build all
	cp build/*.3dsx .

upload: binary
	3dslink -a $(3DS_IP) nordlicht22.3dsx

test: binary
	cp nordlicht22.3dsx /mnt/c/temp/run.3dsx
	# /mnt/c/Users/lorenzdiener/AppData/Local/citra/nightly-mingw/citra-qt.exe "C:/temp/run.3dsx"
	/mnt/c/Users/halcy/AppData/Local/citra/nightly-mingw/citra-qt.exe "C:/temp/run.3dsx"