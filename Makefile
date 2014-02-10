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
	git add -v *.c *.cc *.h *.hh *.sh *.txt LICENSE README.md Makefile* vars
	git commit -v
	git push

bin:
	mkdir -p $(ARCH)
	make -f ../Makefile.$(ARCH) -C $(ARCH)/ #2>&1 | more
