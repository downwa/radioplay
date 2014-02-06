ARCH = $(shell uname -i)

downwa:
	git add -v *.c *.cc *.h *.hh *.sh *.txt LICENSE README.md Makefile*
	git commit -v
	#git push -v origin master
	git push -v https://downwa@github.com/downwa/radioplay master

bin:
	mkdir -p $(ARCH)
	make -f ../Makefile.$(ARCH) -C $(ARCH)/ 2>&1 | more
