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
	git add -v *.c *.cc *.h *.hh *.sh *.txt LICENSE README.md Makefile* vars new-mplay
	git commit -v
	git push

bin:
	mkdir -p $(ARCH)
	make -f ../Makefile.$(ARCH) -C $(ARCH)/ #2>&1 | more

install: bin
	cd $(ARCH) && sudo cp -av lib*.so /lib/i386-linux-gnu/

installpi: bin
	cd $(ARCH) && sudo cp -av lib*.so /usr/lib/

run: bin install
	sudo mount --bind $(HOME)/RadioSD/UPDATE3ABN /media/RadioSD && cd $(ARCH) && ln -sf ../get*.sh . && ./play3abn && cd .. && sudo umount /media/RadioSD
