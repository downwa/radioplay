cd ..; make; cd x86_64; sudo cp *.so /lib/i386-linux-gnu/; valgrind --tool=exp-sgcheck ./mplay
