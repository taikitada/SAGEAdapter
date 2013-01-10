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
#define WIN_W 1500
#define WIN_H 1500

#define RESIZE_WIN 1500
#define RESIZE_HEIGHT 1500


long get_dtime(void){
	struct timeval tv;
	gettimeofday(&tv,NULL);
	return ((tv.tv_sec) * 1000000 + (tv.tv_usec));
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
	
	char *cmdline = "/usr/bin/Xvfb :2 -shmem -screen 0 1500x1500x24 2>&1 ";
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

	long looptime = 0;
	double starttime[1000];
	double endtime[1000];
	printf("startloop\n");
	fflush(fp);
	long lpstart;
        long lpend;
	double memtime_start[1000];
	double memtime_end[1000];
	double swapBuffer_start[1000];
	double swapBuffer_end[1000];
	double getbuffer_start[1000];
	double getbuffer_end[1000];
	
	for(i=0;i<1000;i++)
	{
	memtime_start[i] = 0.0;
	memtime_end[i] = 0.0;
	swapBuffer_start[i] = 0.0;
        swapBuffer_end[i] = 0.0;
	getbuffer_start[i] =  0.0;
        getbuffer_end[i] = 0.0;

	}

	
	i = 0;
	long lptime = 0;
	double sleeptime = 0.0;

	lpstart = get_dtime();
	while(i<1000){
	//	double lpstart;
	//	double lpend;
//		lpstart = get_dtime();
//		starttime[i] = lpstart;
	/*	XWindowAttributes attributes;
		XGetWindowAttributes(dpy, root, &attributes);
		int x = attributes.x;
		int y = attributes.y;
		int width = attributes.width;
		int height = attributes.height;
	*/
	//	memtime_start[i] = get_dtime();
		memcpy(rgbbuffer, data_ptr, WIN_W*WIN_H*4);
	//	memtime_end[i] = get_dtime();
	//	swapBuffer_start[i] = get_dtime();
		sageInf.swapBuffer();
	//	swapBuffer_end[i] = get_dtime();
	//	getbuffer_start[i] = get_dtime();
		rgbbuffer = (char *)sageInf.getBuffer();
	//	getbuffer_end[i] = get_dtime();

//		lpend = get_dtime();
//		lptime = lpend - lpstart;
//		looptime += lptime;
//		printf("%d\n",lptime);
	
//		if(lptime < 0.033)
//		{ 
//			printf("loop = %d:time\n",i);
//			sleeptime = 0.033 - lptime;
//			usleep(int(sleeptime*1000000));
//		}				
	
//	endtime[i] = lpend;
	//	printf("loop:%d,start:%f,end:%f\n",i,lpstart,lpend);
		i++;
		//if(i==999) 
		//break;
		

		//usleep(25000);
	}
	lpend = get_dtime();
	looptime += lpend - lpstart;
/*	XQueryTree(dpy,root,&rootID,&parentID,&childID,&child_num);
        printf("rootID=%u,parentID=%u,child_num=%u\n",rootID,parentID,child_num);
	double memcpytime = 0.0;
	for(i=0;i<1000;i++)
	{
		memcpytime += memtime_end[i] - memtime_start[i];
	}
	printf("memcpytime :  %f\n",memcpytime);
	
	double swapBuffertime = 0.0;
	for(i=0;i<1000;i++)
	{
		swapBuffertime += swapBuffer_end[i] - swapBuffer_start[i];
	}

	printf("swapBuffertime : %f\n",swapBuffertime);
	
	double getbuffertime = 0.0;	

	for(i=0;i<1000;i++)
	{
		getbuffertime += getbuffer_end[i] - getbuffer_start[i];
	}
	printf("getbuffer : %f\n",getbuffertime);


*/
	        XWindowAttributes attributes;
        printf("XWindowAttribute\n");
        for(i=0;i<child_num;i++){        
	XGetWindowAttributes(dpy, childID[i], &attributes);
                printf("XGetAttributes\n");
                int map_state = attributes.map_state;

        printf("map_state:%d,childID:%d\n",map_state,i);
}

/*
	for(i=0;i<1000;i++){
	looptime += (endtime[i] - starttime[i]);
	printf("%d\n",i);
	}
*/
	 /* for getting application window data */
       // XQueryTree(dpy,root,(Window*)&rootID,(Window*)&parentID,(Window**)&childID,&child_num);
  /*      Window *grachildID;
	// XQueryTree(dpy,root,&rootID,&parentID,&childID,&child_num);
    Window sss,ttt;
	int x, y, wx, wy,a[10];
	unsigned int key;

 XQueryPointer(dpy,root,&sss,&ttt,&x,&y,&wx,&wy,&key);
	printf("root= %d,root_return=%d,child_return=%d\n",root,sss,ttt);	    
	
//	XQueryTree(dpy,childID[8],&rootID,&parentID,&grachildID,&child_num);
  //     printf("rootID=%u,parentID=%u,child_num=%u,child_name=\n",rootID,parentID,child_num);	
	
	XQueryTree(dpy,root,&rootID,&parentID,&childID,&child_num);
	for(i=0;i<9;i++){
	a[i] = ntohs(childID[i]);
	 printf("rootID=%u,parentID=%u,child_num=%u,child_name=%u\n",rootID,parentID,child_num,childID[i]);	
	}
	Window *dummy, *dummydummy,*dummydummydummy;
	XQueryTree(dpy,2097314,&rootID,&parentID,&dummy,&child_num);
       printf("rootID=%u,parentID=%u,child_num=%d,child_name=%u\n",rootID,parentID,child_num,dummy[0]);
       printf("rootID=%u,parentID=%u,child_num=%d,child_name=%u\n",rootID,parentID,child_num,dummy[1]);
	XQueryTree(dpy,2097272,&rootID,&parentID,&dummydummy,&child_num);
       printf("rootID=%u,parentID=%u,child_num=%d,child_name=%u\n",rootID,parentID,child_num,dummydummy[0]);
       printf("rootID=%u,parentID=%u,child_num=%d,child_name=%u\n",rootID,parentID,child_num,dummydummy[1]);
	




	XQueryTree(dpy,2097153,&rootID,&parentID,&dummydummydummy,&child_num);
       printf("rootID=%u,parentID=%u,child_num=%d,child_name=%u\n",rootID,parentID,child_num,dummydummydummy[0]);
       printf("rootID=%u,parentID=%u,child_num=%d,child_name=%u\n",rootID,parentID,child_num,dummydummydummy[1]);

*/
	printf("looptime = %d\n",looptime);

	XDestroyWindow(dpy,root);
	XCloseDisplay(dpy);


	if(shmctl(shm_id,IPC_RMID,0) == 1){
		perror("shmctl detatch failure");
		exit(EXIT_FAILURE);
	}


	pclose(fp);
	exit (EXIT_SUCCESS);
}
