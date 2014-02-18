export LD_LIBRARY_PATH=.
valgrind --gen-suppressions=all --track-origins=yes --leak-check=full --malloc-fill=0xEF --free-fill=0x20 --log-file=valgrind.log aoss ./mplay
