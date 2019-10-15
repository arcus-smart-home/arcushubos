/*
 *
 *  eMMC Test Driver 
 *
 *
 *  Filename:		emmcparm.c
 *  Description:	Program to dump eMMC Phison key information
 *
 *
 *  Version:		1.0
 *  Date:		    23 Apr 2014
 *  Authors:		Micron Semiconductor Italia
 *
 *  THE PRESENT SOFTWARE WHICH IS FOR GUIDANCE ONLY AIMS AT PROVIDING CUSTOMERS WITH
 *  CODING INFORMATION REGARDING THEIR PRODUCTS IN ORDER FOR THEM TO SAVE TIME. AS A
 *  RESULT, MICRON SHALL NOT BE HELD LIABLE FOR ANY DIRECT, INDIRECT OR CONSEQUENTIAL 
 *  DAMAGES WITH RESPECT TO ANY CLAIMS ARISING FROM THE CONTENT OF SUCH SOFTWARE 
 *  AND/OR THE USE MADE BY CUSTOMERS OF THE CODING INFORMATION CONTAINED HEREIN IN
 *  CONNECTION WITH THEIR PRODUCTS.
 *
 *  Version History
 *
 *  Ver.		Date				Comments
 *
 *  0.9         March 2014			Marco Redaelli: first release		
 *  1.0         April 2014			Francesco Camarda: added -i and -V option	
 *
 */

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <getopt.h>
#include <fcntl.h>
#include <inttypes.h> 		
#include <linux/types.h>
#include <linux/mmc/ioctl.h>
#include <sys/ioctl.h>
#include <asm/byteorder.h>
#include <stdbool.h>

//#include "emmc.h"

#define MAXFLDSIZE 127 		// longest possible field + 1 = 31 byte field
#define MAXFLDS 127 		// maximum possible number of fields 
#define MMC_BLOCK_MAJOR 179
#define DEV_NAME "/dev/mmcblk0"

 /* Extract a byte from a 32-bit u32  respect to data flow*/
#define byte3(x)    ((__u8)(x))
#define byte2(x)    ((__u8)(x >>  8))
#define byte1(x)    ((__u8)(x >> 16))
#define byte0(x)    ((__u8)(x >> 24))
#define DEBUG false
//#define DEBUG true

int fd;					// file pointer to /dev/mmcblkX
FILE *in;				// file pointer to TESTFILE
static struct mmc_ioc_cmd mmc_local_cmd = {0};
static __u32 buffer_ptr[MMC_IOC_MAX_BYTES];  //store data in/out eMMC, max 0x100 x 512B
//static unsigned int buffer_ptr[MMC_IOC_MAX_BYTES];  //store data in/out eMMC, max 0x100 x 512B

/*******************************************************************************
Function:     open_devicefile (char *device_name) 
Description:  open mmc block device file
*******************************************************************************/
int open_devicefile (char *device_name) 
{
	int fd = open(device_name, O_RDWR); 

	if (fd < 0) {
		printf("Cannot open device file: %s\n", device_name);
		perror("File Open Error");
		exit(EXIT_FAILURE);
	}
	return fd;
}



/*******************************************************************************
Function:     *file_open (const char *filename,const char *mode)
Description:  open file 
*******************************************************************************/
FILE *file_open (const char *filename,const char *mode)
{
	FILE* in=fopen(filename,mode); 

	if (in == NULL) {
		printf("cannot open file: %s\n", filename);
		perror("File open error");
		exit(EXIT_FAILURE);
	}
	
	return in;
}
/*******************************************************************************
Function:     issue_cmd(char arr[][MAXFLDSIZE],int fldcnt, bool DBUG)
Description:  Issue eMMC cmds via ioctl()
*******************************************************************************/
int issue_cmd(char arr[][MAXFLDSIZE],int fldcnt, bool DBUG)
{
	int ii,tran_size;

	//mmc_ioc_cmd_set_data(mmc_local_cmd,buffer_ptr);
	
	// pass the pointer of read/write
	mmc_local_cmd.opcode = atoi(arr[0]);
	mmc_local_cmd.arg = strtoul(arr[1],0,16);
	mmc_local_cmd.flags = (int)strtoul(arr[2],0,16);
	mmc_local_cmd.write_flag = (int)strtoul(arr[3],0,16);
	mmc_local_cmd.blksz = (int)strtoul(arr[4],0,16);
	mmc_local_cmd.blocks = (int)strtoul(arr[5],0,16);
	mmc_local_cmd.is_acmd = (int)strtoul(arr[7],0,16);
	mmc_local_cmd.data_timeout_ns = (int)strtoul(arr[8],0,16);
	mmc_local_cmd.cmd_timeout_ms = (int)strtoul(arr[9],0,16);
	mmc_local_cmd.postsleep_min_us = (int)strtoul(arr[10],0,16);
	mmc_local_cmd.postsleep_max_us = (int)strtoul(arr[11],0,16);

	tran_size=mmc_local_cmd.blksz*mmc_local_cmd.blocks;

	// load pattern from file 
//	if ( (strstr(arr[6], ".pat") > 0) && 
//	     (mmc_local_cmd.write_flag == 1) && 
//	     (mmc_local_cmd.blocks > 0) ) {
//		if (load_pattern(arr[6],buffer_ptr) != tran_size) {
//			printf ("transfer size =%d do not match with pattern size\n",
//					mmc_local_cmd.blksz*mmc_local_cmd.blocks);
//			exit(EXIT_FAILURE);
//		}
//	}

	// Sending command down to kernel space
	// condiziono con DBUG per non sporcare l'output
	if (ioctl(fd,MMC_IOC_CMD,&mmc_local_cmd) && DBUG) {
		perror("\nERROR: failure in Sending Command");
		printf("\nERROR: failure in Sending Command");
		//exit(EXIT_FAILURE);
		//exit(-1);
	}
	
	// Output datalog
	//
	if (DBUG) {
			printf("CMD%2s (%8s): %08"PRIx32" %08"PRIx32" %08"PRIx32" %08"PRIx32"\n",
			arr[0],arr[1],mmc_local_cmd.response[0],
			mmc_local_cmd.response[1],mmc_local_cmd.response[2],
			mmc_local_cmd.response[3]);
	}
//	 print & [save] datalog - data, if data
	if ((mmc_local_cmd.write_flag == 0) && (mmc_local_cmd.blocks > 0)&& DBUG) {
		
		// save pattern
//		if (strstr(arr[6], ".pat") > 0) 
//			save_pattern(arr[6],buffer_ptr,tran_size);
//
//		printf ("LBA=%08"PRIx32" BLOCKLEN=%08"PRIx32" no.BLOCKs=%08"PRIx32"\n",
//				mmc_local_cmd.arg,mmc_local_cmd.blksz,mmc_local_cmd.blocks);
//				
		for (ii=0; ii < (mmc_local_cmd.blksz*mmc_local_cmd.blocks/4); ii++) {
//			// revert order due to x86 endianess
//			buffer_ptr[ii] = ((buffer_ptr[ii] & 0x000000FF) << 24) | 
//				         ((buffer_ptr[ii] & 0x0000FF00) << 8) | 
//					 ((buffer_ptr[ii] & 0x00FF0000) >> 8) | 
//					 ((buffer_ptr[ii] & 0xFF000000) >> 24); 
			printf("%08x ",buffer_ptr[ii]); if (ii % 8 == 7) printf ("\n");
		}
		printf ("\n");
	}
}

/*******************************************************************************
Function:     parse( char *record, char *delim, char arr[][MAXFLDSIZE],int *fldcnt)		
Description:  split the line and fill arr[]
*******************************************************************************/
int parse( char *record, char *delim, char arr[][MAXFLDSIZE],int *fldcnt)
{
	int fld=0;

	if ( strncmp(record, "//", strlen("//")) ) {
		char*p=strtok(record,delim);
		
		while (p != NULL) {
			strcpy(arr[fld],p);
			fld++;
			p=strtok(NULL,delim);
		}
		*fldcnt=fld;
	}
	return fld;
}

/*******************************************************************************
Function:     parse_options( int argc, char *argv[], parsed_options_t *parsed_opts )
Description:  Parsing emmcparm arguments
*******************************************************************************/
typedef struct
{
	char device_file[20];// Device file e.g. /dev/mmcblk0
	int action[100];
} parsed_options_t;

static void parse_options( int argc, char *argv[], parsed_options_t *parsed_opts )
{
	int cmd[10], index;
	int i=0;

	struct option options[] =
	{
		{ "help", 0, NULL, 'h' },
		{ "version", 0, NULL, 'V' },
		{ "info", 0, NULL, 'i' },
		{ "Info", 0, NULL, 'I' },
		{ "spare_block", 0, NULL, 'S' },
		{ "bad_block", 0, NULL, 'B' },
		{ "erase_count", 0, NULL, 'E' },
		{ "conf_minor", required_argument, NULL, 'M' },//not used
		{ 0 }
	};
// Default value if no arguments
	strcpy(parsed_opts->device_file,DEV_NAME);
// Main loop for Parsing
	while(1)
	{
		cmd[i] = getopt_long(argc, argv, "hViISBEM:", options, &index);
		if (DEBUG) printf("c loop=%c,\n",cmd[i]);
		if (cmd[i]==-1){
			if (DEBUG) printf("opt -1: No options more options");
			parsed_opts->action[i]=cmd[i];
			break;
		}
		switch( cmd[i] )
		{
	
		case 'h':
			//parsed_opts->minor = strtol( optar(g, NULL, 0 );
			if (DEBUG) printf("opt h: to be develop\n");
			parsed_opts->action[i]=cmd[i];
			break;
		case 'V':
			//parsed_opts->minor = strtol( optarg, NULL, 0 );
			parsed_opts->action[i]=cmd[i];
			if (DEBUG) printf("opt V: to be develop\n");
			break;
		case 'i':
			//parsed_opts->minor = strtol( optarg, NULL, 0 );
			parsed_opts->action[i]=cmd[i];
			if (DEBUG) printf("opt i: to be develop\n");
			break;
		case 'I':
			//parsed_opts->minor = strtol( optarg, NULL, 0 );
			parsed_opts->action[i]=cmd[i];
			if (DEBUG) printf("opt I: to be develop\n");
			break;
		case 'S':
			//parsed_opts->minor = strtol( optarg, NULL, 0 );
			parsed_opts->action[i]=cmd[i];
			if (DEBUG) printf("opt S: to be develop\n");
			break;
		case 'B':
			//parsed_opts->minor = strtol( optarg, NULL, 0 );
			parsed_opts->action[i]=cmd[i];
			if (DEBUG) printf("opt B: to be develop\n");
			break;
		case 'E':
			//parsed_opts->minor = strtol( optarg, NULL, 0 );
			parsed_opts->action[i]=cmd[i];
			if (DEBUG) printf("opt E: to be develop\n");
			break;		
		case 0:
			printf("opt 0: No options?");
			break;
				
		case '?':   
			fprintf(stderr, "emmcparm: invalid option - '?'\n");
		default:
			fprintf(stderr, "emmcparm: invalid option - default\n");
			exit(1);
		}
		i++;
	}
	if (argc-optind >= 1) strcpy(parsed_opts->device_file,argv[optind]);//
}
/*******************************************************************************
Function:     parse_options( int argc, char *argv[], parsed_options_t *parsed_opts )
Description:  Prints usage information for this program to STREAM (typically stdout or stderr), and exit the
			  program with EXIT_CODE.			
*******************************************************************************/
void print_usage (const char* program_name, FILE* stream, int exit_code)
{
  fprintf (stream, "Usage:  %s [options]... [device file]...\n", program_name);
  fprintf (stream,
           "\noptions: \n"
           "  -h  --help\t\t Display this usage information\n"
           "  -V  --version\t\t Print eMMC info\n"
           "  -i  --info\t\t Get eMMC registers (CID, CSD and ECSD) from /sys \n"
           "  -I  --Info\t\t Dump eMMC registers (CID, CSD and ECSD) via commands (CMD8/9/10) \n"
           "  -S  --spare_block\t Print Spare Block info\n"
           "  -B  --bad_block\t Print Bad Block info\n"
           "  -E  --erase_count\t Print Erase Count info\n"
           "\ndevice file: \n"
           "  e.g.  /dev/mmcblk0 \n");
  exit (exit_code);
}
/*******************************************************************************
Function:     void to_bigendian( )
Description:  Prints usage information for this program to STREAM (typically stdout or stderr), and exit the
			  program with EXIT_CODE.			
*******************************************************************************/
void to_bigendian(bool PRINT_ON)
{
int ii=0;

		for (ii=0; ii < (mmc_local_cmd.blksz*mmc_local_cmd.blocks/4); ii++) {
			buffer_ptr[ii] = ( __cpu_to_be32(buffer_ptr[ii]));
			if (PRINT_ON) {	
				printf("%08x ",buffer_ptr[ii]); 
				if (ii % 8 == 7) printf ("\n");
			}
		}

}
/*******************************************************************************
Function:     void emmc_linux_info( )
Description:  
*******************************************************************************/
void emmc_linux_info(int i)
{
int ii=0;
unsigned int block[1024];
unsigned int bl[1024];
const char *filename ="/sys/kernel/debug/mmc1/mmc1:0001/ext_csd";

FILE* pippo=fopen(filename,"r"); 

if (pippo == NULL) {
	printf("cannot open file: %s\n", DEV_NAME);
	perror("File open error");
	exit(EXIT_FAILURE);
}
else{
	char *shell_cmd[1][200];
	*shell_cmd[0]=
	"echo -n \"Name = \"\n"
	"cat /sys/block/mmcblk0/device/name\n"
	"echo -n \"Manif ID = \"\n"
	"cat /sys/block/mmcblk0/device/manfid\n"
	"echo -n \"Manufacturing Date = \"\n"
	"cat /sys/block/mmcblk0/device/date\n";
//	"echo -n \"FW revision = \"\n"
//	"cat /sys/block/mmcblk0/device/fwrev\n";

	*shell_cmd[1]=
	"echo -n \"Name = \"\n"
	"cat /sys/block/mmcblk0/device/name\n"
	"echo -n \"Manif ID = \"\n"
	"cat /sys/block/mmcblk0/device/manfid\n"
	"echo -n \"Manufacturing Date = \"\n"
	"cat /sys/block/mmcblk0/device/date\n"
	"echo -n \"SN = \"\n"
	"cat /sys/block/mmcblk0/device/serial\n"
	"echo -n \"Main Partition Size in Blocks of 512B = \"\n"  
	"cat /sys/block/mmcblk0/size\n"
	"echo -n \"Boot Partition Size in Blocks of 512B = \"\n"
	"cat /sys/block/mmcblk0boot0/size\n"
	"cat /sys/kernel/debug/mmc0/ios\n"
	"echo -n \"CID = \"\n"
	"cat /sys/block/mmcblk0/device/cid\n"
	"echo -n \"CSD = \"\n"
	"cat /sys/block/mmcblk0/device/csd\n"
	"echo -n \"ECSD = \"\n";
//	"cat /sys/kernel/debug/mmc0/mmc0\\:0001/ext_csd \n";
// esecuzione
	system(*shell_cmd[i]);
// ECSD manipulation
if (i== 1) {
ii=0;
   do
   {
      block[ii] = fgetc(pippo);
      if( feof(pippo) )
      {
          break;
      }
     ii++;
   }while(1);
	for (ii=1023; ii > -1 ;ii--) {
		if (ii % 8 ==7 ) {
			bl[ii-0]=block[ii-7];
			bl[ii-1]=block[ii-6];
			bl[ii-2]=block[ii-5];
			bl[ii-3]=block[ii-4];
			bl[ii-4]=block[ii-3];
			bl[ii-5]=block[ii-2];
			bl[ii-6]=block[ii-1];
			bl[ii-7]=block[ii-0];
		}
		 if(ii % 64 == 63) printf("\n      ");if (ii % 8 == 7) printf (" "); printf("%c",bl[ii]);
	}
	printf ("\n");
}
};
fclose(pippo);
}
/*******************************************************************************
Function:     void to_bigendian( )
Description:  Prints usage information for this program to STREAM (typically stdout or stderr), and exit the
			  program with EXIT_CODE.			
*******************************************************************************/
int main(int argc, char **argv)
{
//
// To manage command switch arguments
//	
	int i=0;
	parsed_options_t parsed_opts;

	parse_options( argc, argv, &parsed_opts );
	
	while(DEBUG && parsed_opts.action[i]!=-1) {
	printf("command no.%i=%c\n",i,parsed_opts.action[i++]);
	}
	if (DEBUG) printf("device file=%s\n",parsed_opts.device_file);
//
// To manage eMMC access via ioctl()
//
	char record[1024]={0x0}, arr[MAXFLDS][MAXFLDSIZE]={0x0};
	int fldcnt=0, recordcnt=0;	
	mmc_ioc_cmd_set_data(mmc_local_cmd,buffer_ptr);
	// whack record into fields and issue command down to kernel
//		if ( parse(record,",",arr,&fldcnt) ) 
//			issue_cmd(arr,fldcnt);
	// 		
	i=0;
	while(parsed_opts.action[i]!=-1) {
	switch( parsed_opts.action[i++] )
		{
	
		case 'h':
			print_usage (argv[0],stdout, 0);
			break;
		case 'V':
			emmc_linux_info(0); // option 0 call FW
			break;
		case 'i':
			emmc_linux_info(1); // option 1 calls ph info
			break;
		case 'I':
			fd = open_devicefile(parsed_opts.device_file);	// open mmc block device
			strcpy(record,"7,0x00001,0x195,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x100000,0x200000");
			if ( parse(record,",",arr,&fldcnt) ) issue_cmd(arr,fldcnt,DEBUG);
			to_bigendian(DEBUG);
			//printf("STATUS %#08x \n",buffer_ptr[0]); 
			//strcpy(record,"13,0x10000,0x195,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0");
			//if ( parse(record,",",arr,&fldcnt) ) issue_cmd(arr,fldcnt,DEBUG);
			//to_bigendian(DEBUG);
			//printf("STATUS %#08x \n",buffer_ptr[0]); 
			printf("CID = \n"); 
			strcpy(record,"10,0x10000,0x7,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0");
			if ( parse(record,",",arr,&fldcnt) ) issue_cmd(arr,fldcnt,true);
			to_bigendian(DEBUG);
			//printf("CID %#08x \n",buffer_ptr[0]); 
			printf("CSD = \n"); 
			strcpy(record,"9,0x10000,0x7,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0");
			if ( parse(record,",",arr,&fldcnt) ) issue_cmd(arr,fldcnt,true);
			to_bigendian(DEBUG);
			//printf("CSD %#08x \n",buffer_ptr[0]); 
			strcpy(record,"7,0x10000,0x195,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0");
			if ( parse(record,",",arr,&fldcnt) ) issue_cmd(arr,fldcnt,DEBUG);
			printf("ECSD = \n"); 
			strcpy(record,"8,0x00000,0xb5,0x0,0x200,0x1,0x0,0x0,0x0,0x0,0x0,0x0");
			if ( parse(record,",",arr,&fldcnt) ) issue_cmd(arr,fldcnt,true);
			to_bigendian(DEBUG);
			//printf("ECSD %#08x \n",buffer_ptr[0]); 
			close(fd);
			break;
		case 'S':
			fd = open_devicefile(parsed_opts.device_file);	// open mmc block device
			strcpy(record,"56,0x00011,0xb5,0x0,0x200,0x1,0x0,0x0,0x0,0x0,0x0,0x0");
			//sprintf(record,MMC_GEN_CMD,",0x00011,0xb5,0x0,0x200,0x1,0x0,0x0,0x0,0x0,0x0,0x0"); 
			if ( parse(record,",",arr,&fldcnt) ) issue_cmd(arr,fldcnt,DEBUG);
			to_bigendian(DEBUG);
			printf("Total spare block count: %u \n",(byte0(buffer_ptr[1])<< 8)+ byte1(buffer_ptr[1])); 
			printf("Used spare blocks[%%]: %2.2f \n",((double)((byte0(buffer_ptr[0])<< 8)+ byte1(buffer_ptr[0])+(byte2(buffer_ptr[0])<< 8)+ byte3(buffer_ptr[0]))/((double)(byte0(buffer_ptr[1])<< 8)+ byte1(buffer_ptr[1])))); 
			close(fd);
			break;
		case 'B':
			fd = open_devicefile(parsed_opts.device_file);	// open mmc block device
			strcpy(record,"56,0x00011,0xb5,0x0,0x200,0x1,0x0,0x0,0x0,0x0,0x0,0x0");
			if ( parse(record,",",arr,&fldcnt) ) issue_cmd(arr,fldcnt,DEBUG);
			to_bigendian(DEBUG);
			printf("Total initial bad block count: %u \n",(byte0(buffer_ptr[0])<< 8)+ byte1(buffer_ptr[0])); 
			printf("Total later bad block count  : %u \n",(byte2(buffer_ptr[0])<< 8)+ byte3(buffer_ptr[0])); 
			close(fd);
			break;
		case 'E':
			fd = open_devicefile(parsed_opts.device_file);	// open mmc block device
			strcpy(record,"56,0x00023,0xb5,0x0,0x200,0x1,0x0,0x0,0x0,0x0,0x0,0x0");
			if ( parse(record,",",arr,&fldcnt) ) issue_cmd(arr,fldcnt,DEBUG);
			to_bigendian(DEBUG);
			printf("MLC area erase count Min/Max/Ave: %u %u %u \n",(byte0(buffer_ptr[0])<< 8)+ byte1(buffer_ptr[0]),(byte2(buffer_ptr[0])<< 8)+ byte3(buffer_ptr[0]),(byte0(buffer_ptr[1])<< 8)+ byte1(buffer_ptr[1]) ); 
			strcpy(record,"56,0x00025,0xb5,0x0,0x200,0x1,0x0,0x0,0x0,0x0,0x0,0x0");
			if ( parse(record,",",arr,&fldcnt) ) issue_cmd(arr,fldcnt,DEBUG);
			to_bigendian(DEBUG);
			printf("SLC area erase count Min/Max/Ave: %u %u %u \n",(byte0(buffer_ptr[0])<< 8)+ byte1(buffer_ptr[0]),(byte2(buffer_ptr[0])<< 8)+ byte3(buffer_ptr[0]),(byte0(buffer_ptr[1])<< 8)+ byte1(buffer_ptr[1]) ); 
			close(fd);
			break;		
		default:
			fprintf(stderr, "emmcparm: invalid option - default\n");
			exit(1);
		}
//	i++;
	}
}
