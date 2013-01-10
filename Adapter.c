#include <stdio.h>
#include <stdlib.h>
#include <sys/time.h>
#include <X11/Xutil.h>
#include <X11/xpm.h>
#include <math.h>
#include <X11/extensions/XTest.h>
#include "sail.h"
#include "misc.h"
#include <signal.h>
#include <errno.h>
#include <string.h>
#include <X11/XWDFile.h>
#include <X11/Xlib.h>
#include <unistd.h>
#include <sys/ipc.h>
#include <sys/sem.h>
#include <sys/types.h>
#include <sys/shm.h>
#include <stdint.h>
#include <arpa/inet.h>
#include <errno.h>
#include<iostream>

#define BUF 1000
#define VFBBUF 302700
#include <time.h>
#include <iostream>
/* error */
void die(int exit_code, char *msg) {
	fprintf(stderr, "%s\n", msg);
	exit(exit_code);
}
using namespace std; 
#define WIN_W 2560
#define WIN_H 2048

#define RESIZE_WIN 2560
#define RESIZE_HEIGHT 2048


long get_dtime(void){
	struct timeval tv;
	gettimeofday(&tv,NULL);
	return ((tv.tv_sec)*1000000 + (tv.tv_usec));
}


static char *rgbbuffer;

int main(int argc, char *argv[])
{
	XInitThreads();

	sail sageInf; // sail object
	sailConfig scfg;
	scfg.init("sagevfb.conf");
	scfg.setAppName("TDA");

	scfg.rank = 0;

	scfg.resX = WIN_W;
	scfg.resY = WIN_H;

	sageRect renderImageMap;
	renderImageMap.left = 0.0;
	renderImageMap.right = 1.0;
	renderImageMap.bottom = 0.0;
	renderImageMap.top = 1.0;

	scfg.imageMap = renderImageMap;
	scfg.pixFmt = PIXFMT_8888_INV;
	//        scfg.rowOrd = BOTTOM_TO_TOP;
	scfg.rowOrd = TOP_TO_BOTTOM;
	scfg.master = true;
	scfg.nwID = 1;

	sageInf.init(scfg);
	std::cout << "sail initialized " << std::endl;

	Display*	dpy;
	Window		root, child;
	Window		win;
	int		screen;


	FILE *fp,*fp2 = NULL;
	char buf[BUF];
	void *shared_data;
	uint32_t header_size;
	XWDFileHeader h;
	static unsigned char vfbbuf[VFBBUF];
//	char cmdline[100];
	
	char *cmdline = "/usr/bin/Xvfb :2 -shmem -screen 0 2560x2048x24 2>&1 ";
//	strcpy(cmdline,"/usr/bin/Xvfb :2 -shmem -screen 0 700x700x24 2>&1 ");
	if ((fp = popen(cmdline,"r")) == NULL){
		die(EXIT_FAILURE, cmdline);
	}
	sleep(2);
	
	if((dpy = XOpenDisplay(":2.0")) == NULL){
		die(1,"Can't open Display" );
	}
	
        root = DefaultRootWindow(dpy);
	 char *cmdline2 = "DISPLAY=:2.0 ../app/rasmol/src/rasmol ../app/rasmol/data/7lyz.pdb";
//	char *cmdline2 = "DISPLAY=:2.0 ../app/sample/sample_original";  
      if ((fp2 = popen(cmdline2,"r")) == NULL){
                die(EXIT_FAILURE, cmdline2);
        }
	sleep(2);
	/* for getting application window data */
	Window rootID, parentID;
	Window *childID;
//	unsigned int rootID, parentID, childID, child_num;
	unsigned int child_num;
	XQueryTree(dpy,root,&rootID,&parentID,&childID,&child_num);
	printf("rootID=%u,parentID=%u,child_num=%u\n",rootID,parentID,child_num);
	
	
	int i;
	for(i=0;i<child_num;i++)
	{ 
		XWindowAttributes attributes;
		XGetWindowAttributes(dpy, childID[i], &attributes);
		int map_state = attributes.map_state;
		if(map_state == 2)
		{
			XMoveWindow(dpy,childID[i],0,0);
		    XResizeWindow(dpy,childID[i],RESIZE_WIN,RESIZE_HEIGHT);
		}
	
	}

	/*  appHandler  */
	sageInf.initAppMessageHandler(dpy, root);
	
	if(fgets(buf, BUF, fp) == NULL)
		die(1, "Failed to get output from Xvfb");
	printf("%s\n",buf);

	char *p = strstr(buf, "shmid");
	if(p == NULL) {
		fprintf(stderr, "Output from Xvfb: %s\n", buf);
		die(1, "shmid is not detected");
	}
	p+= strlen("shmid ");
	int shm_id =atoi(p);
	printf("id : %d\n",shm_id);

	shared_data = shmat(shm_id, 0, 0);
	if((int)shared_data == -1){
		perror("shmat failure");
		exit(EXIT_FAILURE);
	}
	//	shmid_ds shm_data;
	struct shmid_ds shm_data;
	if(shmctl(shm_id, IPC_STAT, &shm_data) == 1){
		perror("shmctl STAT failure");
		exit(EXIT_FAILURE);
	}
	printf("shm_pid=%d,shm_segmentsize=%d\n",shm_data.shm_cpid,shm_data.shm_segsz);


	memcpy(&h,shared_data,sizeof(h));
	// memcpy(&header_size,shared_data,sizeof(uint32_t));
	h.header_size = ntohl(h.header_size);
	printf("header : %u\n",h.header_size);
	size_t colormap_size = sizeof(XColor)*ntohl(h.ncolors);
	printf("Xcolor: %u\n",colormap_size);

	char *data_ptr = (char *)shared_data;
	data_ptr += (size_t)h.header_size + (size_t)colormap_size;

	rgbbuffer = (char *)sageInf.getBuffer();

	double looptime = 0;
	printf("startloop\n");
	fflush(fp);
	long lpstart=0;
        long lpend=0;

	
	i = 0;
	long lptime = 0;
	double sleeptime = 0.0;

	
	int *data_ptrpre;
	void *old_frame = malloc(WIN_W*WIN_H*4);
	uint64_t *old_frame_ptr;
	uint64_t *current_frame_ptr;
	bool changed = false;
	int k;
	long check_start[1000] = {0};
	long check_end[1000] = {0};
	
	printf("whilebefore\n");

	lpstart = get_dtime();
	
	while(1){
		if(i==0){
		changed = true;
		}
//		tmp_prev = reinterpret_cast <uint64_t *>(data_ptr);
		
		if(changed){
		memcpy(rgbbuffer, data_ptr, WIN_W*WIN_H*4);
		memcpy(old_frame, data_ptr, WIN_W*WIN_H*4);

		sageInf.swapBuffer();
		rgbbuffer = (char *)sageInf.getBuffer();
		}
		old_frame_ptr = reinterpret_cast<uint64_t *>(old_frame);
		current_frame_ptr = reinterpret_cast<uint64_t *>(data_ptr);
		changed = false;
	
//		check_start[i] = get_dtime();
		for(k=0;k<(WIN_W*WIN_H*4/sizeof(uint64_t));k++)
			{	
//				printf("enter: check%d\n",k);
				if(*old_frame_ptr != *current_frame_ptr)
				{
				changed = true;
	//			printf("changed: %d\n", k);
	//			printf("1: %x\n", *old_frame_ptr );
	//			printf("2: %x\n", *current_frame_ptr);
				break;
				}
		//	printf("out: checkout%d\n",k);
			old_frame_ptr++;
			current_frame_ptr++;
			}
//		check_end[i] = get_dtime();
//		changed=false;
//		printf("looplooop%dlllllllllll\n",i);
		i++;
		
	}

//	lpend = get_dtime();
//	looptime = lpend - lpstart;

//	printf("looptime = %lf\n",(double)looptime/1000000);
	
	long check_all = 0;
	for(i=0;i<1000;i++)
	{

	check_all += check_end[i] -check_start[i];

	}
	printf("check time: %lf\n",(double)check_all/1000000);
	
	XDestroyWindow(dpy,root);
	XCloseDisplay(dpy);


	if(shmctl(shm_id,IPC_RMID,0) == 1){
		perror("shmctl detatch failure");
		exit(EXIT_FAILURE);
	}

	free(old_frame);
	pclose(fp);
	exit (EXIT_SUCCESS);
}
