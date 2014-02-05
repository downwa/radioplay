#gcc -o hellopulse hellopulse.c $(pkg-config --cflags --libs pulse-simple)
#g++ -o hellopulse hellopulse.c -lpulse -lpulse-simple
g++ -o pulsetest pulsetest.cc -lpulse -lpulse-simple
./pulsetest testtone.wav
