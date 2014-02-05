#ifndef __WAVEHH__
#define __WAVEHH__

typedef unsigned long DWORD;
typedef unsigned char BYTE;
typedef unsigned short WORD;
typedef short SAMPLE;

typedef union WaveChunk {
	struct {
		BYTE	ckID[4]; 		// Chunk ID: "RIFF"
		DWORD cksize;  		// Chunk size: 4 + 24 + (8 + M * Nc * Ns + (0 or 1)) // (file length+36)
		BYTE	WAVEID[4];	// WAVE ID: "WAVE"
	} a; // sizeof(a)==12
	struct {
		BYTE	ckID[4]; 		// Chunk ID: "fmt "
		DWORD cksize;  		// Chunk size: 4 + 24 + (8 + M * Nc * Ns + (0 or 1))
		WORD	sampleDataFormat;	// 1=PCM,85=MP3,257=IBM uLaw,258=IBM A-Law,259=IBM AVC ADPCM format
		WORD	nChannels; // e.g. 1 or 2
		DWORD	samplesPerSecond; // e.g. 16000, 22050, 44100, 48000
		DWORD	avgNBytesPerSecond; // samplesPerSecond*nChannels*blockAlign
		WORD	blockAlign; // e.g. 2
		WORD	sigBitsPerSample;	// PCM Data only (e.g. 16)
	} b; // sizeof(b)==24
	struct {
		BYTE	ckID[4]; 		// Chunk ID: "data"
		DWORD cksize;  		// Chunk size: M * Nc* Ns
		BYTE	data[16];		// Sampled data: M * Nc * Ns	Nc * Ns channel-interleaved M-byte samplesPerSecond
	} c; // sizeof(c)==24 but only read 8 to avoid reading part of the data
} WaveChunkType;

#endif