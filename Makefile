ARCH = $(shell uname -m)

default: bin

gitconfig:
	git config --global push.default simple
	git config --global credential.helper 'cache --timeout=3600'

checkout:
	#git fetch origin
	#git merge origin/master
	git pull

checkin: # e.g. downwa
	#git push -v origin master
	#git push -v https://downwa@github.com/downwa/radioplay master
	git add -v *.c *.cc *.h *.hh *.sh *.txt LICENSE README.md Makefile* vars ogglen
	git commit -v
	git push

bin:
	mkdir -p $(ARCH)
	make -f ../Makefile.$(ARCH) -C $(ARCH)/ #2>&1 | more

install: bin
	cd $(ARCH) && sudo cp -av lib*.so /lib/i386-linux-gnu/

release:
	date +%Y%m%d%H%M%S >vars/release.txt
	
installpi: bin release
	sed -i.bak -e 's@^1:23.*@1:2345:respawn:/home/pi/radioplay/playradio.sh </dev/tty1 >/dev/tty1 2>\&1@g' -e 's@^2:23.*@2:23:respawn:/usr/bin/alsamixer </dev/tty2 >/dev/tty2 2>\&1@g' -e 's@^3:23.*@3:23:respawn:/usr/bin/tail -f /var/log/syslog </dev/tty3 >/dev/tty3 2>\&1@g' /etc/inittab
	which bc || apt-get install bc
	cd $(ARCH) && sudo cp -av lib*.so /usr/lib/

run: bin install
	sudo mount --bind $(HOME)/RadioSD/UPDATE3ABN /media/RadioSD && cd $(ARCH) && ln -sf ../get*.sh . && ./play3abn && cd .. && sudo umount /media/RadioSD

splay: splay.cc
	c++ -O -fno-inline -g -Wall -Werror -m32 -L./libs/ -L./libs/32 -L./libs/64 -I./libs -I./include -o splay splay.cc src-fill.cc thread.cc -L. -ldecoder -lradioutil -lvorbisidec -lpthread -lpulse -lpulse-simple -lasound -lvorbisfile

runtest:
	make install && make splay && cd x86_64/; ../splay; cd ..
