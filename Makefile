ARCH = $(shell uname -i)

default: bin

gitconfig:
	git config --global push.default simple
	git config --global credential.helper 'cache --timeout=3600'

checkout:
	#git fetch origin
	#git merge origin/master
	git pull

checkin: # e.g. downwa
	git add -v *.c *.cc *.h *.hh *.sh *.txt LICENSE README.md Makefile*
	git commit -v
	git push
	#git push -v origin master
	#git push -v https://downwa@github.com/downwa/radioplay master

bin:
	mkdir -p $(ARCH)
	make -f ../Makefile.$(ARCH) -C $(ARCH)/ 2>&1 | more
