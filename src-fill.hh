#include "maindec.hh"

#include "thread.hh"
#include "util.hh"

#include <algorithm>
#include <vector>

//#include "utildefs.hh"

using namespace std;

class SrcFill: Thread {
	Util* util;
	Decoder* decoder;
protected:
	void Execute(void* arg);
public:
	SrcFill(Util* util, Decoder* decoder);
};