void performAction(int &scrnum, int rownum, bool isLocal) {
	if(rownum==9999) { return; } // Dummy action to avoid any action on row 0
	int debug=getInt("debug",0);
	if(debug) { dolog("performAction: scrnum=%d,rownum=%d,scheduleRow=%d,detailStatus=%d",scrnum,rownum,scheduleRow,detailStatus); }
	if(rownum==0) {
		if(debug) { dolog("performAction: Cycling audio source/mount status"); }
		int audiosource=getInt("audiosource",0);
		audiosource++;
		if(audiosource>=(int)arraylen(chooser->playlists)) { audiosource=-1; }
		setInt("audiosource",audiosource);
		if(audiosource == -1) {
			Curler::denyMount=true;
			Curler::unmount("performAction");
		}
		else if(audiosource > -1) {
			Curler::denyMount=false;
			Curler::domount("performAction");
			initDirs();
		}
	}
	else if(rownum==1) { // Toggle Main menu
		if(debug) { dolog("performAction: Toggling main menu"); }
		scrnum=(scrnum!=1)?1:0;
	}
	switch(scrnum) {
		case 0:  {
			if(rownum==2) {
				int vol=getInt("volume",30);
				if(vol==100) { setVol(30); }
				else { setVol(vol+5); }
				if(debug) { dolog("volume %d",vol+5); }
			}
			else if(rownum==3) {
				int isBoosted=getInt("isboosted",1);
				setInt("isboosted",1-isBoosted);
			}
			else if(rownum==4) {
				int showPublicIp=getInt("showpublicip",0);
				setInt("showpublicip",1-showPublicIp);
			}
			else if(rownum==5) {
				showScheduleDate=(showScheduleDate?false:true);
			}
			else if(rownum>=scheduleRow) {
				dolog("detail");
				if(rownum==detailRow) { detailStatus=1-detailStatus; }
				else { detailRow=rownum; detailStatus=1; }
				if(detailStatus==0) { detailRow=-1; detailCount=0; }
			}
		}
		break;
		case 1: {
			switch(rownum) {
				case 2: {
					if(debug) { dolog("Update"); }
					if(system("/bin/update >/tmp/update.log.txt 2>&1 &")==-1) { dolog("performAction:system:update failed: %s",strerror(errno)); }
				}
				break;
				case 3: {
					int alpha=getInt("alpha",0); // Do not install alpha releases by default
					setInt("alpha",1-alpha);
				}
				break;
				case 4: {
					int cache=getInt("cache",1); // Use cache by default
					setInt("cache",1-cache);
				}
				break;
				case 5: {
					setInt("debug",1-debug);
				}
				break;
				case 6: { // Show downloads
					scrnum=2;
				}
				break;
				case 7: { // Show log
					scrnum=3;
				}
				break;
				case 8: { // Show playlist (main screen)
					scrnum=0;
				}
				break;
				case 9: {
					oscreen->Shutdown();
					dolog("Reboot");
					prepareShutdown(1);
					if(system("./reboot.sh")==-1) { dolog("performAction:system:reboot failed: %s",strerror(errno)); }
					exit(11);
				}
				break;
				case 10: {
					if(isLocal) { // Only local requests can power off the machine (for safety)
						oscreen->Shutdown();
						dolog("Shutdown");
						prepareShutdown(0);
						if(system("./poweroff.sh")==-1) { dolog("performAction:system:poweroff failed: %s",strerror(errno)); }
						exit(10);
					}
				}
				break;
				case 11: {
					doExit=true;
					oscreen->Shutdown();
					dolog("Exit");
					prepareShutdown(0);
					remove("/tmp/upgrading.flag");
					exit(9);
				}
				break;
				case 13: { // Show clients
					scrnum=4;
				}
				break;
			} // END SWITCH(rownum)
		} // END CASE(1)
		break;
		case 2: {
			if(rownum>=3) {
				dolog("detail");
				if(rownum==detailRow) { detailStatus=1-detailStatus; }
				else { detailRow=rownum; detailStatus=1; }
				if(detailStatus==0) { detailRow=-1; detailCount=0; }
			}
		}
		break;
	} // END SWITCH(scrnum)
}
