// utils.h.
// string functions, replaces stdio.h


#include <stdarg.h>	// For sprintf argument list
class CClient;
#define FLASH_SECTOR_SIZE	0x7fff
//#define messageBase (((unsigned long)(&GBL_pmrun_messageHighWord_address))<<16)
#define PAD_ZERO	0
#define PAD_SPACE	1

#ifndef int16
typedef unsigned int int16;
typedef unsigned long int32;
#endif

int min(int16 a,int16 b);
int max(int16 a,int16 b);

// Real time 
void delay_ms(int time);	// like Sleep()
int32 getTickCount();		

void ATFactoryReset(int HW_IF, int profile=0);

// String manipulation
void sprintf(unsigned char *dest ,const char *src,...);
void sprintg(char *pStr);
void makelower(unsigned char *str, int len=0xffff);
void makeupper(unsigned char *str);
void strip_parity(unsigned char *pdata,int charstoparse);

int16 hex_2int(unsigned char* hexstr); 
void printformatstring(unsigned char* dest,int destlen,const char* src,int srclen,bool hexformat=false,bool strip_parity=true);
//void printformatstring(unsigned char* dest,const char* src,bool hexformat=false,int destlen=0,bool strip_parity=true);
bool strcomp(unsigned char* str1,unsigned char* str2);
unsigned char* TrimLeft(unsigned char* CurrentLine,int len);
unsigned char* TrimRight(unsigned char* CurrentLine);
bool isgood_file_extn(const char* filename_extn, char **extn);

// Char conversion
unsigned char* itoa32(unsigned char *dest,int32 param,bool modifier_padding,int16 modifier_fieldwidth);
unsigned char* itoa(unsigned char *dest,int16,bool bPadding = PAD_SPACE,int16 fieldwidth = 0);
unsigned char* itoh(unsigned char *dest,int16,bool modifier_uppercase=true,int16 fieldwidth = 2);
unsigned char* itoh(unsigned char *dest,int32,bool modifier_uppercase=true,int16 fieldwidth = 8);
unsigned char* mac_2ascii(int16 *mac_addr);
int16* ascii_2mac(unsigned char* mac_str);
unsigned char* ip_2ascii(int32 ip_addr,int pad=16);
int32 ascii_2ip(unsigned char* ip_str);
int32 ascii_2ip_nodots(unsigned char* ip_str);
unsigned char* baudrate_2ascii(int baudrate);
int16 baudrate_2uS(int baudrate);
void UartAutobaudEnable(int HW_IF,bool enable,bool change_config=true);

// Formatted displays
unsigned char* HW_2ascii(int IF_NUM);
unsigned char* wan_if_2ascii(unsigned char* buf);
unsigned char* hardware_if_2ascii(unsigned char* buf);
unsigned char* owner_2ascii(int owner);
int arpcache_2ascii(unsigned char *buf,int start_index=0,int destlen=256);
unsigned char* version_2ascii(unsigned char *dest);
unsigned char* rtable_2ascii(unsigned char *buf);
unsigned char* phase_2ascii(unsigned char *str,int phase);
unsigned char* npmode_2ascii(int mode);
unsigned char* linestate_2ascii(int linestate);
unsigned char* uart_2ascii(unsigned char* buf,unsigned int HW_IF);
unsigned char* flowcontrol_2ascii(int flowcontrol);
unsigned char* dataformat_2ascii(int dataformat);
//unsigned char* filetype_2ascii(int FILETYPE);
unsigned char* tcpstate_2ascii(int tcpstate);
void objects_2ascii(unsigned char* buf);
void tracemodule_2ascii(unsigned char* buf);
void tracelevel_2ascii(unsigned char* buf);

// Uart switching functions
bool IsHardwareInterfaceAvailable(int HW_CLASS);
int GetHardwareInterface(int OWNER_IF_NUM,CClient *pOwner,int HW_CLASS);
int ReleaseHardwareInterface(int IF_NUM,int owner);
int GetHardwareOwner(CClient*);
void UpdateUart(int HW_IF,int Online = false);

// Trace functions
void OutputDebugString(const int16,const int16,const char*,bool endline = true);
void OutputDebugString(const int16,const int16,unsigned char*,bool endline = true);
void trace_out(unsigned char* OutputString,unsigned int len,bool endline=true);
void hexformatstring(unsigned char* src,unsigned int count,char* dest,int destlen);
void hexdump(unsigned char* p,unsigned int len,int HW_IF=0xffff);
int serialout(const char *OutputString,int HW_IF,unsigned int len = 0xffff);

// System memory calculator
int16 freemem();

#define STATE_NORMAL	0
#define STATE_PERCENT	1
#define STATE_BACKSLASH	2


