#include "maindec.hh"
//#include "wavewriter.hh"

void Decoder::init(Taudioq* audioq, Util *util) {
	isMp3=false; isOgg=false; isWav=false; fadein=0;
	nchannels=0; samplerate=0; trackSampleCount=0; isWav=false; this->util=util;
	decmp3=new DecodeMp3(util); decogg=new DecodeOgg(util); decwav=new DecodeWav(util);
	this->audioq=audioq; blockindex=0;
}

int Decoder::Decode(const char *filename, int seeksecs) {
	MARK
	// filename="/sdcard/download/OTR624~ON_THE_ROAD~Reigning_In_Righteousness~.ogg";
	isMp3=false; isOgg=false; isWav=false; this->filename=filename; this->seeksecs=seeksecs;
	int slen=strlen(filename);
MARK
	if(slen>4 && strcmp(&filename[slen-4],".ogg")!=0) { // If not an .ogg, try .mp3
MARK
		isMp3=decmp3->isValid(filename,nchannels, samplerate, trackSampleCount,seclen);
	}
MARK
//	syslog(LOG_ERR,"isMp3=%d",isMp3);
if(!isMp3) { 
  isOgg=decogg->isValid(filename,nchannels, samplerate, trackSampleCount,seclen); 
printf("NOFAILGO\n");
//	syslog(LOG_ERR,"isOgg=%d",isOgg);
}
MARK
	if(!isMp3 && !isOgg) { isWav=decwav->isValid(filename,nchannels, samplerate, trackSampleCount,seclen); 
	syslog(LOG_ERR,"isWav=%d",isWav);
	}
MARK
	
// syslog(LOG_ERR,"FAKE DECODING FOR TEST: isOgg=%d",isOgg);
// FILE *pcmin=decogg->Open(filename,seeksecs);
// util->longsleep(5);
// if(pcmin) fclose(pcmin);
// decogg->Finish(99);
// return 0;

MARK
	if     (isOgg) { return Enqueue(decogg->Open(filename,seeksecs),seeksecs); }
	else if(isMp3) { return Enqueue(decmp3->Open(filename,seeksecs),seeksecs); }
	else if(isWav) { return Enqueue(decwav->Open(filename,seeksecs),seeksecs); }
MARK
	syslog(LOG_ERR,"Unknown file type: %s",filename);
MARK
	return -1; // Unknown type
}

int Decoder::Enqueue(FILE *pcmin, int seeksecs) {
	if(!pcmin) { return -9; }
	int fileindex=getIT("fileindex")+1;
	setIT("fileindex",fileindex);
	setST("playfile",filename);
	
	/** filename e.g. /tmp/play3abn/tmp/playtmp-2011-11-21 11:10:28 0108 106-HEAVEN'S REALLY GONNA SHINE-Chuck Wagon Gang.ogg **/
	bool isFiller=false;
	const char *pp=strstr(filename,"/playtmp-");
	if(pp) { /** e.g. /playtmp-2011-11-21 11:10:28 0108 106-HEAVEN **/
					 /**                1111111111 222222222 3333 **/
					 /**      0123456789 123456789 123456789 1234 **/
		pp=&pp[34];
		isFiller=(isdigit(pp[0]) && isdigit(pp[1]) && isdigit(pp[2]) && pp[3]=='-');
	}
	int debug=util->getInt("debug",0);
	if(seclen==-1) { seclen=SecLen(filename); } // Last chance to correct seclen.  Hope it works
	setIT("playsec",seeksecs);
	setIT("curplaylen",seclen);
	int secs=seclen;
	trackSampleCount=seclen*samplerate;
	int hh=secs/3600;
	int hsecs=secs%3600;
	int mm=hsecs/60;
	int ss=hsecs%60;
	float frac=seclen-secs;
	int tt=frac*10.0;
	if(debug) {
		syslog(LOG_ERR,"Bitstream is %ld channel, %ldHz, %ld samples (%02d:%02d:%02d.%d), seek=%d,filename=%s",
			nchannels,samplerate,trackSampleCount,hh,mm,ss,tt,seeksecs,filename);
	}
	else {
		syslog(LOG_ERR,"Play %ld ch, %ldHz, (%02d:%02d:%02d.%d), seek=%d: %s",
			nchannels,samplerate,hh,mm,ss,tt,seeksecs,filename);
	}

// #include "wavewriter.hh"
// 	WaveWriter* wr=new WaveWriter(util);
// 	wr->WriteHeader(filename,stdout, nchannels, samplerate, trackSampleCount);
	int bufferLeft=0;
	int elapsed=seeksecs;
	bool isSilent=false;
	int needed=0;
	while(!feof(pcmin)){
		if(bufferLeft==0) { bufferLeft=nchannels*samplerate*sizeof(SAMPLE); }
		int bufferSize=bufferLeft;
		char* data=new char[bufferSize];
		if(!data) { syslog(LOG_ERR,"maindec: memory failure: %s",strerror(errno)); break; }
		int ofs=0;
		do {
			size_t ret=fread(&data[ofs], 1, bufferLeft, pcmin);
			if (ret <= 0) { break; }
			ofs+=ret; bufferLeft-=ret;
		} while(bufferLeft>0);
		#ifdef ARM
		int armDouble=getId("armdouble",1); // Default to doubling to stereo (but set this to 0 on e.g. Kindle)
		if(nchannels==1 && armDouble==1) { // Need to double to stereo on some ARM hardware
			char *pcmdbl=new char[bufferSize*sizeof(SAMPLE)];
			long xb=0;
			for(int xa=0; xa<bufferSize; xa+=2) {
				pcmdbl[xb]=data[xa]; // Sample one
				pcmdbl[xb+1]=data[xa+1];
				pcmdbl[xb+2]=data[xa]; // Sample two
				pcmdbl[xb+3]=data[xa+1];
				xb+=4;
			}
			delete data;
			data=pcmdbl;
			bufferSize*=2; bufferLeft*=2;
		}
		#endif
		int datalen=bufferSize-bufferLeft;
		SAMPLE maxsamp=0;
		SAMPLE amaxsamp=0;
		// Filler should already be boosted and not need to be (boosting it causes static problems)
		bool doBoost=isFiller?false:(getId("isboosted",1)==1);
		setIT("doboost",doBoost);
		isSilent=fixAmplitude((SAMPLE *)data,datalen/sizeof(SAMPLE),doBoost,maxsamp,amaxsamp);
//delete data; data=NULL;
		audioq->enq(data,datalen,fileindex,blockindex++,samplerate,nchannels,string(filename),trackSampleCount,doBoost,maxsamp,amaxsamp);
		setIT("playsec",elapsed++);
		needed=((int)seclen)-elapsed;
		if(isSilent && needed>5) { break; }
	}
	fclose(pcmin);
	if(isSilent && needed>5) { setIT("silentfill",needed); }
	else { setIT("silentfill",0); }
//sleep(1);
	syslog(LOG_INFO,"DECODING COMPLETE: filename=%s",filename);
	return bufferLeft;
}
float Decoder::SecLen(const char *filename) {
	int slen=strlen(filename);
	seclen=-1;
	if(slen>4 && strcmp(&filename[slen-4],".ogg")!=0) { // If not an .ogg, try .mp3
		seclen=decmp3->SecLen(filename);
	}
	if(seclen<0 && strcmp(&filename[slen-4],".ogg")==0) {
		seclen=decogg->SecLen(filename);
		if(seclen<0) {
			syslog(LOG_ERR,"Removing corrupt .ogg: filename=%s",filename);
			remove(filename);
		}
	}
	if(seclen<0) { seclen=decwav->SecLen(filename); }
	return seclen;
}

/** FIXME: checkAmplitude should only boost non-filler files (they are already loud enough) **/
/** FIXME: Better yet, let the downloader scan the ogg file and set a boost value for it, once and for all **/
/** FIXME: Then the boost does not have to be re-calculated, only executed **/
bool Decoder::fixAmplitude(SAMPLE *samples,int numSamples, bool doBoost, SAMPLE &maxsamp, SAMPLE &amaxsamp) {
	SAMPLE val,smax=0;
	int ampSamples=numSamples; // /100;
	for(int i=0; i<ampSamples; i++) {
		val=samples[i];
		if(val < 0) val = -val; /* ABS */
		if(val > smax) { smax = val; }
	}
	amaxsamp=maxsamp=smax;
	/************************ Check avg(max amplitude) **********************/
	maxsamps.push_back(maxsamp); //*2);
	if(maxsamps.size()>ADJLEN) { maxsamps.erase(maxsamps.begin()); }
	vector<SAMPLE>::iterator cii;
	int count=0;
	//unsigned long sumAll=0L;
	SAMPLE maxAll=0;
	for(cii=maxsamps.begin(); cii!=maxsamps.end(); cii++,count++) {
		SAMPLE max=(SAMPLE)(*cii);
		//sumAll+=max;
		if(max>maxAll) { maxAll=max; }
	}
	if(maxAll<500) { syslog(LOG_ERR,"Decoder::fixAmplitude: SILENCE DETECTED: maxAll=%d",maxAll); return true; }
	if(doBoost) {
		//unsigned long maxAll=(sumAll/count);
		//maxAll+=4096; // Give some headroom		
		if(maxAll<=0) { maxAll=1; }
		/************************ ADJUST AMPLITUDE **********************/
		if(maxAll>30000) { maxAll=30000; }
		int adjustRatio=32768*1000/maxAll;
		if(adjustRatio<=10000) { // Only boost 10x maximum (silence should not be boosted to noise!)
			for(int i=0; i<numSamples; i++) {
				val=samples[i];
				val=(val*adjustRatio/1000);
				samples[i]=val;
				if(val < 0) val = -val; /* ABS */
				if(val > smax) { smax = val; }
			}
			amaxsamp=smax;
		}
	}
	if(fadein>0) {
		int debug=getI("debug");
		int index=FADEINMAX-fadein;
		int ofs=index*numSamples;
		int fadeSamples=FADEINMAX*numSamples;
		if(debug) { syslog(LOG_ERR,"fadein=%d,ofs=%d",fadein,ofs); }
		for(int i=0; i<numSamples; i++) { samples[i]=(samples[i]*(ofs+i)/fadeSamples); }
		fadein--;
	}
/*	else if(fadeout>0) { // FIXME: fadeout still happens early on long files
		int index=FADEOUTMAX-fadeout;
		int ofs=index*numSamples;
		int fadeSamples=FADEOUTMAX*numSamples;
		if(debug) { syslog(LOG_ERR,"fadeout=%d,ofs=%d",fadeout,ofs); }
		for(int i=0; i<numSamples; i++) { samples[i]=(samples[i]*(fadeSamples-ofs-i)/fadeSamples); }
		fadeout--;
	}*/
	return false;
}

/*
int main(int argc, char **argv) {
	if(argc<3) { Util *util=new Util("maindec"); syslog(LOG_ERR,"Usage: maindec [filename] [seeksecs]"); exit(1); }
	return (new Decoder())->Decode(argv[1],atoi(argv[2]));
}
*/
