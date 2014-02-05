//#include "play3abn.h"

#include "outscreen_saver.hh"

#ifdef ARM
void OutScreenSaver::backLight(int onOff /* 1=on, 0=off*/) {
	int fb = open("/dev/backlight", O_NONBLOCK);
	if (fb < 0) { perror("Error opening /dev/backlight"); sleep(1); }
	if (ioctl(fb, onOff) < 0) {
		int saveErrno=errno;
		FILE *backlight=fopen("/dev/backlight","wb");
		if(backlight) {
			fprintf(backlight,"%d",onOff);
			fclose(backlight);
		}
		else {
			syslog(LOG_ERR,"Error opening /dev/backlight: %s",strerror(errno));
			errno=saveErrno; syslog(LOG_ERR,"After backlight error: %s",strerror(errno));
			sleep(1);
			return;
		}
	}
	close(fb);
}

void OutScreenSaver::screenOnOff(int onOff) {
	unsigned int vOnOff;
	if (onOff) vOnOff = VESA_NO_BLANKING; else vOnOff = VESA_POWERDOWN;
	int fb = open("/dev/fb0", O_NONBLOCK);
	if (fb < 0) { perror("Error opening /dev/fb0"); sleep(1); return; }
	if (ioctl(fb, FBIOBLANK, vOnOff) < 0) { sleep(1); perror("VESA on/off error"); }
	close(fb);
}

//int isArm=0;
int OutScreenSaver::tty_mode_was;
struct ezfb OutScreenSaver::fb = {0};
void OutScreenSaver::screenReset() {
	if(!ezfb_init(&fb, EZFB_NO_SAVE /*EZFB_SAVE_SCREEN*/)) { 
return; }
	ezfb_init(&fb, EZFB_NO_SAVE /*EZFB_SAVE_SCREEN*/);
	tty_mode_was=KD_TEXT; // Apparently buggy ezfb_tty_text does not do this as it should
	ezfb_tty_text();
	ezfb_release(&fb);
}
#endif
void OutScreenSaver::screenSave() {
	clear();
	refresh();
#ifdef ARM
	screenReset();
#endif
  // NOTE: Return without restoring screen
}

//------------------------------------------------------------------------------------------------
//------------------------------------------------------------------------------------------------
//------------------------------------------------------------------------------------------------
//------------------------------------------------------------------------------------------------
//------------------------------------------------------------------------------------------------

void OutScreenSaver::Execute(void* arg) {
	syslog(LOG_ERR,"OutScreenSaver:Execute");
	savetid("play3abn-screenSaver");
	while(!Util::checkStop()) {
		//alive("play3abn-screenSaver");
		ShowScreen();
		setIT("saveCounter",SAVEDELAY);
		while(getI("saveCounter")>0 && !Util::checkStop()) {
			setIT("saveCounter",getIT("saveCounter")-1); sleep(1);
		}
		for(setIT("saveCounter",SAVEDELAY); getIT("saveCounter")>0 && !Util::checkStop(); setIT("saveCounter",getIT("saveCounter")-1)) {
			sleep(1);
		}
		HideScreen();
		while(getIT("saving") && !Util::checkStop()) { sleep(1); }
		out->Activate();
	}
	syslog(LOG_ERR,"OutScreenSaver:EXITED");
}

#ifdef ARM
void OutScreenSaver::ShowScreen() {
	screenReset();
	screenOnOff(1);
	backLight(1);
	syslog(LOG_ERR,"Screen reset.");
	setIT("saving",0);
}
#else
void OutScreenSaver::ShowScreen() { }
#endif

#ifdef ARM
void OutScreenSaver::HideScreen() {
	setIT("saving",1);
	screenSave();
	screenOnOff(0);
	backLight(0);
	syslog(LOG_ERR,"Screen blanked.");
}
#else
void OutScreenSaver::HideScreen() { }
#endif
