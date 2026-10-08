// strings.cpp
// String manipulation routines

#include "object.h"
#include "router.h"
#include "pppd.h"
#include "utils.h"
#include "tcp.h"
#include "tcpmodem.h"
#include "ffs.h"
#include "C:\DataDir\Adsp21xx\seb\VC5410A\pmRun\schedule.h"
#include "C:\DataDir\Adsp21xx\seb\VC5410A\pmRun\hardware.h"

// Temporary working strings
unsigned char ipstr[MAXLEN2];
unsigned char macstr[MAXLEN1];

//-------------------------------------------------------------------
void printformatstring(unsigned char* dest,int destlen,const char* src,int srclen,bool hexformat,bool strip_parity)
// Convert src string to printable o/p dest string
// hexformat=true:
// All chars converted to hex
// hexformat=false:
// Nonprintable characters converted to hex eg cr -> <0A>
// Nonprintables are  < 20h(SPACE) and >7Dh(})

// Use destlen param if dest string is not of length TRACE_BUFLEN.
// Nb. ** Does not display parity bit **
{
int  out=0;
int in;
unsigned char ch;
if(destlen == 0 )
	destlen = TRACE_BUFLEN;	// Default expects "msg" to be specified as dest. This is its length

for(in=0; ((in < srclen) && (out < destlen)); in++)
	{
	ch = src[in];

	if(strip_parity)
		ch = ch & 0x7f;						// ** strip parity bit **

	if(hexformat)
		{
		// Display in hex
		itoh((unsigned char*)&dest[out],(int16)ch,true,2);
		out+=2;
		dest[out++]=' ';
		}
	else
		{
		if( (ch >= ' ') && (ch <= '}') )
			// Printable character or hex mode
			dest[out++]=ch;
		else
			{
			// Non printable character. Display hex value <hh>
			dest[out++]='<';
			itoh((unsigned char*)&dest[out],(int16)ch,true,2);
			out+=2;
			dest[out++]='>';
			}
		}
	};
	
dest[out]=NULL;
}

#pragma CODE_SECTION("sectionFastCode");
unsigned char* itoh(unsigned char *dest,int16 param,bool modifier_uppercase,int16 modifier_fieldwidth)
// Convert unsigned 16 bit integer to a Hex string
// Maxval = int16 = 0xffff
// modifier_fieldwidth = number of leading zeros to  display.eg 4 => "0001" 0 => "1"
// modifier_uppercase upper or lowercase hex chars (ie a-f or A-F)
{
int strpos=0;
char ch;
bool leadingdigit=false;						// True when a non-zero char encountered
int n;

const char* convert = convert_lower;
if(modifier_uppercase) convert = convert_upper;

// Make an array of the 4 nibbles to convert, (16 bits)
char data[4];
data[0] = param >> 12;
data[1] = (param >> 8) & 0x0f;
data[2] = (param >> 4) & 0x0f;
data[3] = param & 0x0f;

// Convert one nibble at a time
for(n=0; n < 4; n++)
	{
	ch = convert[ data[n] ];					// hi nibble

	if(ch != '0')
		{
		leadingdigit=true;						// most significant digit encountered
		dest[strpos++] = ch;
		}
	else
		{
		// Char is '0'. Only add to string if EITHER:
		// 1. We have previously encountered a leading non-zero digit
		// 2. modifier_fieldwidth defines a leading zero in this position
		if(leadingdigit || (modifier_fieldwidth >= (4-n)))
			dest[strpos++] = ch;
		}
	}

dest[strpos++] = NULL;
return dest;
}

//-------------------------------------------------------------------
unsigned char* itoh(unsigned char *dest,int32 param,bool modifier_uppercase,int16 modifier_fieldwidth)
// 32 bit version
{
unsigned char* p = dest;
// Least significant 16 bits
itoh(p,(int16)(param >> 16),modifier_uppercase,min(modifier_fieldwidth-4,4));
p += strlen((const char*)p);

// Most significant 16 bits
itoh(p,(int16)(param & 0xffff),modifier_uppercase,max(modifier_fieldwidth,0));
return dest;
}

//-------------------------------------------------------------------
int16 hex_2int(unsigned char* hexstr) 
// 2 Byte Hex char string to integer
// eg "FF" -> int 255
{
int ch_lo;
int ch_hi;
makeupper(hexstr);

ch_lo = hexstr[1];
ch_hi = hexstr[0];

if(ch_lo >= 'A')
	ch_lo = ch_lo - 'A' + 10;		// Char is 'A'-'F'. Convert to 10-15
else ch_lo -= '0';					// Char is '0'-'9'. Convert to 0-9

if(ch_hi >= 'A')
	ch_hi = ch_hi - 'A' + 10;		// Char is 'A'-'F'. Convert to 10-15
else ch_hi -= '0';					// Char is '0'-'9'. Convert to 0-9

return (ch_hi << 4) + ch_lo;;
}

//-------------------------------------------------------------------
#pragma CODE_SECTION("sectionFastCode");
int min(int16 a ,int16 b)
{return (a < b? a : b);}
int max(int16 a,int16 b)
{return (a > b? a : b);}

//-------------------------------------------------------------------
unsigned char* mac_2ascii(int16 *mac_addr)
// Convert mac address to colon separated hex format string
// mac address is stored as int16[3]
// Typical output : 00:c0:fd:23:21:a5
{
int16 byt16;
unsigned char tmpstr[10];tmpstr[0]=NULL;
macstr[0] = NULL;

for(int i = 0; i<3 ;i++)
	{
	byt16 = mac_addr[i];

	itoh(tmpstr,(int16)(byt16 & 0xff),true,2);
	strcat((char*)macstr,(const char*)tmpstr);
	strcat((char*)macstr,"-");

	byt16 >>= 8;

	itoh(tmpstr,(int16)(byt16 & 0xff),true,2);
	strcat((char*)macstr,(const char*)tmpstr);
	strcat((char*)macstr,"-");
	}
	
macstr[17] = NULL;
return macstr;
}

#pragma CODE_SECTION("sectionFastCode");
//-------------------------------------------------------------------
unsigned char* ip_2ascii(int32 ip_addr,int pad)
// Convert 32 bit integer to dotted decimal format string
// Resulting string is padded to 16 characters long.
// result in ipstr
// string is space padded to "pad" characters
{
int32 byt;
unsigned char dot = '.';
unsigned char *p = ipstr;
int n;

for(n=0; n <4 ;n++)
	{
	byt = ip_addr;
	byt >>= 24;
	byt &= 0xff;
	itoa(p,byt);
	p = (unsigned char*)strchr((const char*)p,0);		// Find null terminator
	*p++ =  dot;
	ip_addr <<= 8;
	}

p -= 1;

if( pad > 0)
	{
	// Want a string length of 16 chars for screen formatting
	n = p - ipstr; // same as strlen((const char*)ip_str);
	do
		{
		*p++ = ' ';
		n++;
		}while(n < 16);
	}

*p = NULL;
return ipstr;
}

#pragma CODE_SECTION("sectionFastCode");
//-------------------------------------------------------------------
void sprintf(unsigned char *dest ,const char *src,...)
// Added "%a" - display ip Address in dotted decimal format
// Added "%m" - display mac Address
{
unsigned char ch,fch;
int i=0;							// Points to current char position in "dest" string
bool modifier_uppercase = false;
int16  iParam;
int32 lParam;
char* pcParam;
bool modifier_padding = PAD_SPACE;
int modifier_fieldwidth = 0;
int modifier_long= false;

va_list ap;
va_start(ap, src);
int state = STATE_NORMAL;

while((ch=(unsigned char)(*src++)) != NULL ) //valid ascii char.
	{
	switch(ch)
		{
		case NULL:
			dest[i++] = NULL;
			break;

		case '\\':
			if( state == STATE_NORMAL)
				state = STATE_BACKSLASH;		// Backslash encountered
			else if (state == STATE_BACKSLASH)
				{
				dest[i++] = '\\';				// Double Backslash encountered
				state = STATE_NORMAL;
				}
			else if (state == STATE_PERCENT)
				state = STATE_NORMAL;			// %\ encountered.n Error
			break;
		
		case '%':
			if( state == STATE_NORMAL)
				state = STATE_PERCENT;			// % encountered
			else if (state == STATE_PERCENT)
				{
				dest[i++] = '%';				// %% encountered
				state = STATE_NORMAL;
				}
			else if (state == STATE_BACKSLASH)
				state = STATE_NORMAL;			// \& encountered.n Error
			break;
		
		default:
		// A non escape char read.
		// Action based on previous char
		switch(state)
		{
		case STATE_NORMAL:
			// A normal character
			modifier_uppercase = false;
			modifier_padding = PAD_SPACE;
			modifier_fieldwidth = 0;
			modifier_long = false;
			dest[i++] = ch;
			break;
		
		case STATE_BACKSLASH:
			// A character following "\"
			state = STATE_NORMAL;
			switch(ch)
				{
				case '\"':			// Quotes
				dest[i++] = '\"';
				break;
				case 'r':
				dest[i++] = '\x0d';
				break;
				case 'n':
				dest[i++] = '\x0a';
				break;
				case 't':
				dest[i++] = '\t';
				break;
				default:
				//Unrecognised escape sequence
				dest[i++] = '?';
				break;
				}
			break;
			
		case STATE_PERCENT:
			// Switch character following "%"
			switch(ch)
				{
				case 'a':					// %a ip address padded to fixed length
				// Nb. Use %0a to skip padding
				lParam = va_arg(ap, int32);
				ip_2ascii(lParam,modifier_padding);
				dest[i] = NULL;
				strcat((char*)dest,(const char*)ipstr);
				i = strlen((const char*)dest);
				state = STATE_NORMAL;
				break;		

				case 'c':					// Character
				iParam = va_arg(ap, int16);
				dest[i] = iParam;
				i = strlen((const char*)dest);
				state = STATE_NORMAL;
				break;
				
				case 'm':					// %a ip address padded to fixed length
				// Nb. Use %0a to skip padding
				iParam = va_arg(ap, int16);
				dest[i] = NULL;
				mac_2ascii((int16*)iParam);
				strcat((char*)dest,(const char*)macstr);
				i = strlen((const char*)dest);
				state = STATE_NORMAL;
				break;		

				case 'S':					// Formatted string ******NOT from flash*****
				case 's':					// **Unformatted** String ******from RAM*******
				//eg //sprintf(msg,szPcntS,"abcd");
				pcParam = va_arg(ap, char *);
				if(pcParam != NULL)
					{
					do
						{
						fch = *(pcParam++);
						dest[i++] = fch;
						}while(fch != NULL);

					i-=1;						// Don't want terminator char
					}
				state = STATE_NORMAL;
				break;

				case 'u':					// Unsigned decimal integer
				case 'd':					// Signed decimal integer as %i
				case 'i':					// Signed decimal integer as %d
				if(modifier_long)
					{
					lParam = va_arg(ap, int32);	// TI bug ? Returns 16 bits given 32 bit param
					itoa32((unsigned char*)&dest[i],lParam,modifier_padding,modifier_fieldwidth);	// Unsigned Decimal / integer
					}
				else
					{
					iParam = va_arg(ap, int16);
					itoa((unsigned char*)&dest[i],iParam,modifier_padding,modifier_fieldwidth);	// Unsigned Decimal / integer
					}
				i = strlen((const char*)dest);
				state = STATE_NORMAL;
				break;
			
				case 'X':					// Uppercase Hex
				modifier_uppercase = true;
				case 'x':
				if(modifier_long)
					{
					lParam = va_arg(ap, int32);
					itoh((unsigned char*)&dest[i],(int32)lParam,modifier_uppercase,modifier_fieldwidth);
					}
				else
					{
					iParam = va_arg(ap, int16);
					itoh((unsigned char*)&dest[i],iParam,modifier_uppercase,modifier_fieldwidth);
					}
				i = strlen((const char*)dest);				
				state = STATE_NORMAL;
				break;		

				case 'l':					// Specifies long ( 32-bit ) integer
				modifier_long = true;	
				break;
				
				case '0': //(ZERO)
				// Optional param %0 padding char is a '0'(ZERO)
				modifier_padding = PAD_ZERO;
				break;
				
				default:
				if((ch >= '1') && (ch <= '9'))
					{
					// Optional param
					// %1-9 Minimum field width specifier
					// NB max field width specifier is 9 !!			
					modifier_fieldwidth = ch - '0';
					break;
					}
				
				// Unsupported specifier following %, must be a % char
				dest[i++] = '%';
				dest[i++] = ch;
				state = STATE_NORMAL;
				break;		
				}
			}// end (state == STATE_PERCENT)

		}// end case default
	if(ch == NULL) break;
	}

va_end(ap);		   // Reset	arg-list
dest[i] = NULL;
return;
}	

//-------------------------------------------------------------------
void sprintg(char *pStr)
// Convert HTML formatted string to normal text
// 1. Finds text "%hh" in a char string,. "hh" is 2 hex bytes.
// 2. Converts '+' to ' '
// This is needed as web browsers convert reserved chars
// to multi-char sequences eg. = -> %3D. 
{
char *str = pStr;
char ch;
int len = 0;
unsigned char sprintgstr[3];

while((ch=(char)*str) != NULL ) // Valid ascii char.
	{
	if( ch == '+' )
		// Browsers send + instead of ' '
		*str = ' ';

	if( ch == '%' )
		{
		// Escape sequence format %HH. Convert to ASCII equivalent
		if( (*(str+1) != NULL) && (*(str+2) != NULL) )
			{
			sprintgstr[0]=(unsigned char)*(str+1);
			sprintgstr[1]=(unsigned char)*(str+2);
			sprintgstr[2]=NULL;
			ch = (char)hex_2int(sprintgstr);
			*str = ch;
			len = 1 + strlen(str+3);
			memcpy((str+1),(str+3),len);
			}
		else
			// Encountered %NULL or %HNULL
			return;
		}
	str++;
	}
}	

//-------------------------------------------------------------------
unsigned char* itoa32(unsigned char *dest,int32 param,bool modifier_padding,int16 modifier_fieldwidth)
// Convert 32 bit integer to ascii string
{
int32 temp;
int32 modnum = 1000000000;
int strpos=0;
for(int i; modnum>0 ;i++)
	{
	temp = param % modnum;	// temp is remainder
	param -= temp;			// Param is now a multiple of 10^n
	if(param > 0 )
		dest[strpos++] = (char) (param/modnum) + '0';
	else if(strpos)
		// Print zeros,but not leading zeros
		dest[strpos++] = '0';
		
	param = temp;			// loop again with the remainder
	modnum /= 10;
	}

if(strpos == 0)
	// Converted to 0 - but leading 0's are not printed.
	dest[strpos++] = '0';	
dest[strpos] = NULL;
return dest;
}

//-------------------------------------------------------------------
unsigned char* itoa(unsigned char *dest,int16 param,bool modifier_padding,int16 modifier_fieldwidth)
// Convert 16 bit integer to ascii string
// Maxval = int16 = 32000
{
int temp;
unsigned int modnum = 10000;
int strpos=0;
for(int i; modnum>0 ;i++)
	{
	temp = param % modnum;	// temp is remainder
	param -= temp;			// Param is now a multiple of 10^n
	if(param > 0 )
		dest[strpos++] = (char) (param/modnum) + '0';
	else if(strpos)
		// Print zeros,but not leading zeros
		dest[strpos++] = '0';
		
	param = temp;			// loop again with the remainder
	modnum /= 10;
	}

if(strpos == 0)
	// Converted to 0 - but leading 0's are not printed.
	dest[strpos++] = '0';	
dest[strpos] = NULL;
return dest;
}

//-------------------------------------------------------------------
void makelower(unsigned char *str, int len)
// Convert string to lowercase
{
if(len == 0xffff)
	len = strlen((const char*)str);
	
unsigned char *ch;
for(int n=0; n < len; n++)
	{
	ch = str + len;
	if((*ch <= 'Z' ) || (*ch  >= 'A'))
		*ch = *ch | 0x20;					// Convert to lower case
	};
}

//-------------------------------------------------------------------
void makeupper(unsigned char *str)
// Convert a string to uppercase
{
int len = strlen((const char*)str);
for(int i=0; i< len; i++)
	{
	if((str[i] >= 'a') && (str[i] <= 'z'))
		str[i] ^= 0x20;						// Convert to upper case
	}
}

//--------------------------------------------------------------------------
void strip_parity(unsigned char *pdata,int charstoparse)
{
int n;
for(n=0; n < charstoparse; n++)
	*pdata++ = *pdata & 0x7f;
}

//--------------------------------------------------------------------------
unsigned char* TrimLeft(unsigned char* CurrentLine,int keylen)
// Remove "keylen" chars from left of string
// Then remove whitespace (space,cr,lf,tab) characters from the left of the string.
// STRING MUST BE NULL TERMINATED !!
{
unsigned int len = (unsigned int)strlen((const char*)CurrentLine);
char ch;
int whitespace_chars = 0;
int pos=0;

// Trim out specific no of chars first
keylen = (keylen > len)? len : keylen;
if(keylen > 0)
	strcpy((char*)CurrentLine,(const char*)&CurrentLine[keylen]);

// Trim out leading whitespace
while(pos < len)
	{
	ch = CurrentLine[pos++];
	if((ch == ' '))
		whitespace_chars++;
	else if((ch == '\r'))
		whitespace_chars++;
	else if((ch == '\n'))
		whitespace_chars++;
	else if((ch == '\t'))
		whitespace_chars++;
	else pos=len;
	};

if(whitespace_chars > 0)
	strcpy((char*)CurrentLine,(const char*)&CurrentLine[whitespace_chars]);

return CurrentLine;
}

//--------------------------------------------------------------------------
unsigned char* TrimRight(unsigned char* CurrentLine)
// remove whitespace (space,cr,lf,tab) characters from the string.
// STRING MUST BE NULL TERMINATED !!
{
unsigned int len = (unsigned int)strlen((const char*)CurrentLine);
int pos=0;

// Trim out trailing whitespace
while(pos < len)
	{
	switch (CurrentLine[pos])
		{
		case NULL:
		// Error
		break;
		case ' ':
		case '\r':
		case '\n':
		case '\t':
		strcpy((char*)&CurrentLine[pos],(char*)&CurrentLine[pos+1]);
		len = len-1;
		break;

		default:
		pos++;
		}
	}
CurrentLine[pos]=NULL;
return CurrentLine;
}

//-------------------------------------------------------------------
void hexdump(unsigned char* p,unsigned int len,int HW_IF)
{
// Dump data in ascii hex representation to serial port or system log
for(int i=0; i< len; i++)
	{
	sprintf(msg,szpercentfour_brackets,p[i]);				//<%04X>
	if(HW_IF == 0xffff)
		OutputDebugString(LOG_NOTICE,LOG_SYSTEM,(const char*)msg,false);
	else
		serialout((char*)msg,HW_DTE0);
	};

OutputDebugString(LOG_NOTICE,LOG_SYSTEM,szSpace);			// Send cr lf
}

//-------------------------------------------------------------------
void hexformatstring(unsigned char* src,unsigned int count,char* dest,int destlen)
// Dump "count" data bytes in hex format to buffer "dest"
// Formatted to 16 hex bytes per line
{
int destptr=0;
int chars_this_line=0;
strcpy(dest,szEndline);
destptr=2;
int i;
	
for(i=0; i< count; i++)
	{
	dest[destptr++]='<';
	itoh((unsigned char*)&dest[destptr],src[i],true,4);	
	destptr += 4;
	dest[destptr++]='>';
	dest[destptr++]=' ';


	// Will next char exceed destination string length ?
	if(destptr >= (destlen-10))
		break;
		
	chars_this_line++;												// 1 more char converted
	if(chars_this_line >= 8)
		{
		// End of current display line
		chars_this_line = 0;
		dest[destptr++] = '\r';
		dest[destptr++] = '\n';
		}
	}

dest[destptr] = NULL;
}

//-------------------------------------------------------------------
unsigned char* version_2ascii(unsigned char *dest)
//Generate a text build version display
{
sprintf(dest,szBuildInfo,szRoutersName,szSoftwareVersion,szDebugorRelease,szCPUSpeed,szUARTSpeed,szTFTPoption,szDHCPoption,szSMTPoption,szPOP3option,szPADoption);
return dest;
}

//-------------------------------------------------------------------
int arpcache_2ascii(unsigned char *dest,int start_index,int destlen)
// Convert arp cache to a printable string
// Each line is approx 50 bytes
// Returns the index of the last entry in the string.
// Call multiple times if necessary
{
int n,i=0;
dest[0] = NULL;
int32 currenttime = getTickCount()/1000;	// 1ms units
int32 time_elapsed;

for(n=start_index; n < ARP_CACHELEN; n++ )
	{
	if(arp_cache[n].ip_addr != 0L)
		{
		time_elapsed =(int32)(currenttime - arp_cache[n].time);
		
		sprintf(&dest[i],szARPDisplay,arp_cache[n].ip_addr,
						arp_cache[n].mac_addr,
						(arp_cache[n].type & ARP_STATIC)? szARPStatic:szARPDynamic,
						time_elapsed,
						(arp_cache[n].type & ARP_STALE)? szARPStale:szSpace);

		i=strlen((const char*)dest);
	
		if(i > (destlen - 55))
			break;
		}
	}

return n+1;
}

int16 tmp_mac_addr[3];

//-------------------------------------------------------------------
int16* ascii_2mac(unsigned char* mac_str)
// MAC address string to integer array
// mac_str in format aa-bb-cc-dd-ee-ff
// returns : ptr if success, NULL if fail.
// Nb **mac_str is corrupted**
//           ip_addr_int points to result

// eg String "DF-F5-00-C0-C4-53" converts to:
// tmp_mac_addr[0] = 0xf5df;	
// tmp_mac_addr[1] = 0xc000;
// tmp_mac_addr[2] = 0x53c4;
{
int pos=0,i=0,n=0;
int16 temp;
int16 len=(int16)strlen((const char*)mac_str);
if(strlen((const char*)mac_str) != 17)
	return NULL;

// Convert to 4 null terminated strings	
mac_str[2] = NULL;		// Simplifies parsing
mac_str[5] = NULL;
mac_str[8] = NULL;
mac_str[11] = NULL;
mac_str[14] = NULL;

for(i=0;i < 3;i++)
	{
	tmp_mac_addr[n] = hex_2int(&mac_str[pos]);
	pos += 3;
		
	temp = hex_2int(&mac_str[pos]);
	temp = temp<<8;
	tmp_mac_addr[n] += temp;
	pos += 3;
	n++;
	}

return tmp_mac_addr;
}

//-------------------------------------------------------------------
int32 ascii_2ip_nodots(unsigned char* ip_str)
// Decimal format IP string to integer. 
// Used to convert IP address in "AT" dial command to ip addr. Some sw cannot send dots in the dial number
// Input: IP address in format aaabbbcccddd. No spaces or dots.
// Returns : ip address as int32 if success, -1 if fail.
{
int32 ip_addr=0;
int n=0, i;
char tmpstr[4];
char *endptr = &tmpstr[3];

int16 len=(int16)strlen((const char*)ip_str);
if(len != 12)
	// Bad string parameter
	return 0xFFFFFFFFL;
	
for(i=0;i < 4;i++)
	{
	// Copy 3 chars at a time to temp string
	tmpstr[0] = ip_str[n++];
	tmpstr[1] = ip_str[n++];
	tmpstr[2] = ip_str[n++];
	tmpstr[3] = NULL;
	
	ip_addr <<=8;
	ip_addr += strtoul((const char*)tmpstr,&endptr, 10);
	}

return ip_addr;
}

//-------------------------------------------------------------------
int32 ascii_2ip(unsigned char* ip_str)
// Dotted decimal format IP string to integer
// Returns : int32 ip address if success, -1 if fail.
{
int32 ip_addr=0;
int pos=0, i = 0, numdots = 0;
unsigned char* dot;
char endptr;
char *pendptr=&endptr;

int16 len=(int16)strlen((const char*)ip_str);
ip_str[len] = '.';		// Simplifies parsing

for(i=0;i < 4;i++)
	{
	dot = (unsigned char*)strchr((const char*)&ip_str[pos],'.');
	if(dot != NULL)
		{
		numdots++;
		*dot = NULL;
		ip_addr <<=8;
		
		ip_addr += strtoul((const char*)&ip_str[pos], &pendptr, 10);

		pos = 1 + dot - ip_str;

		if((pos >= len) & (i < 3))
			// Error. Bad address entered
			return 0xFFFFFFFFL;
		}
	}

if(numdots != 4)
	// Bad address format
	return 0xFFFFFFFFL;

return ip_addr;
}

#ifndef SEB

//-------------------------------------------------------------------
unsigned char* rtable_2ascii(unsigned char *dest)
// Convert routing table to a printable string
// TODO check strlen does not exceed buffer size !!
{
int i=0;

for(int n=0; n < RTABLE_SIZE; n++ )
	{
	if(rtable[n].dest_net != 0)
		{
		sprintf(&dest[i],szRTableDisplay,(int32)rtable[n].dest_net,(int32)rtable[n].dest_mask,rtable[n].gateway,HW_2ascii(rtable[n].if_num));
		i=strlen((const char*)dest);		
		}
	};

return dest;
}

//-------------------------------------------------------------------
unsigned char* wan_if_2ascii(unsigned char* buf)
{
buf[0]= NULL;

for(int i=0; i < NUM_UARTS; i++)
	{
	if(ppp_if[i].auth[0] != NULL)
		{
		strcat(buf,ppp_if[i].auth);
		strcatf(buf,sz2Tabs);
		strcat(buf,ppp_if[i].password);
		strcatf(buf,sz2Tabs);
		strcat(buf,ppp_if[i].dialnumber);
		strcatf(buf,szTab);
		if(ppp_if[i].demand)
			{
			//For demand if, display idle timeout
			strcat(buf,(unsigned char*)"Y");
			strcatf(buf,szTab);
			sprintf(msg,szpercent_i,ppp_if[i].maxinactivitytime/100);
			strcat(buf,msg);
			}
		else		
			strcat(buf,(unsigned char*)"N");

//		HW_2ascii(msg,hardware_if[i].if_unit);
		strcatf(buf,szEndline);
		//hardware_if[i].useChap = false;
		//hardware_if[i].dirty = false;				// Flags a config change
		}
	}

return buf;
}
#endif

//-------------------------------------------------------------------
unsigned char* uart_2ascii(unsigned char* buf,unsigned int HW_IF)
// Convert uart parameters to a printable string
{
if( HW_IF >= (NUM_UARTS-1) )
	{
	strcpy((char*)buf,szError);
	return buf;
	}

sprintf(buf,szUartDisplay,
			HW_IF,
			owner_2ascii(hw->uart[HW_IF].owner),
			baudrate_2ascii(HW_IF),			
			hw->uart[HW_IF].flag_dtr? szDTR:szdtr,
			hw->uart[HW_IF].flag_rts? szRTS:szrts,
			hw->uart[HW_IF].flag_cts? szCTS:szcts,
			hw->uart[HW_IF].flag_dcd? szDCD:szdcd,
			flowcontrol_2ascii(modem_config.flowcontrol_flag),
			dataformat_2ascii(hw->uart[HW_IF].uart_parity_rx),
			hw->uart[HW_IF].framing_error_count,
			hw->uartOutputQueue(HW_IF),
			hw->uartInputQueue(HW_IF));
return buf;
}


//-------------------------------------------------------------------
unsigned char* owner_2ascii(int owner)
// Accepts OWNER_ constant, or a HW_IF number
{
if(owner < NUM_UARTS)
	// A HW_IF number was supplied
	return HW_2ascii(owner);
	
switch(owner)
	{
	case OWNER_NONE:
		sprintf(ownerstr,szOwner_None);
		break;
	case OWNER_DTE:
		sprintf(ownerstr,szOwner_DTE);	// ERROR !!
		break;
	case OWNER_TA:
		sprintf(ownerstr,szOwner_TA);	// ERROR !!
		break;
	case OWNER_TCPPORT:
		sprintf(ownerstr,szOwner_TCP);
		break;	
	case OWNER_SHELL:
		sprintf(ownerstr,szOwner_Console);
		break;	
	case OWNER_UDPPORT:
		sprintf(ownerstr,szOwner_UDP);
		break;		
	case OWNER_PPP:
		sprintf(ownerstr,szOwner_PPP);
		break;		
	
	default:
		sprintf(ownerstr,szOwnerError,owner);
	}		
return ownerstr;
}

//-------------------------------------------------------------------
unsigned char* HW_2ascii(int IF_NUM)
// Convert an IF_NUM number to interface name
{
if(IF_NUM == HW_NONE)
	{
	strcpy((char*)ifstr,szNone);
	return ifstr;
	}		
if(IF_NUM < HW_TA)
	{
	sprintf(ifstr,szDTE_percentd,IF_NUM+1);
	return ifstr;
	}
if(IF_NUM < HW_TRACE)
	{
	sprintf(ifstr,szTA_percentd,IF_NUM+1-HW_TA);
	return ifstr;
	}
if(IF_NUM == HW_TRACE)
	{
	strcpy((char*)ifstr,szStdout);
	return ifstr;
	}
if(IF_NUM < HW_LAN)
	{
	strcpy((char*)ifstr,szX21);
	return ifstr;
	}
if(IF_NUM == HW_LAN)
	{
	strcpy((char*)ifstr,szLAN);
	return ifstr;
	}
	
sprintf(ifstr,szClassError,IF_NUM);
return ifstr;
}

#ifdef PPP
//-------------------------------------------------------------------
unsigned char* phase_2ascii(unsigned char *str,int phase)
{
switch(phase)
	{
	case PHASE_DEAD:
		strcpy((char*)str,szPHASE_DEAD);
		break;	
	case PHASE_INITIALIZE:
		strcpy((char*)str,szPHASE_INITIALIZE);
		break;	
	case PHASE_ESTABLISH:
		strcpy((char*)str,szPHASE_ESTABLISH);
		break;	
	case PHASE_AUTHENTICATE:
		strcpy((char*)str,szPHASE_AUTHENTICATE);
		break;	
	case PHASE_CALLBACK:
		strcpy((char*)str,szPHASE_CALLBACK);
		break;	
	case PHASE_NETWORK:
		strcpy((char*)str,szPHASE_NETWORK);
		break;	
	case PHASE_TERMINATE:
		strcpy((char*)str,szPHASE_TERMINATE);
		break;	
	case PHASE_HOLDOFF:
		strcpy((char*)str,szPHASE_HOLDOFF);
		break;	
	default:
		sprintf(str,szPhaseError,phase);
	}
return str;
}

//-------------------------------------------------------------------
unsigned char* npmode_2ascii(int mode)
// Used for PPP interface display
{
switch(mode)
	{
	case NPMODE_NONE:
		strcpy((char*)ipstr,szNONE);
		break;
	case NPMODE_QUEUE:		// TODO queue frame
		strcpy((char*)ipstr,szQUEUE);
		break;
	case NPMODE_DISCARD:
		strcpy((char*)ipstr,szDISCARD);
		break;
	case NPMODE_PASS:
		strcpy((char*)ipstr,szPASS);
		break;
	default:
		strcpy((char*)ipstr,szNPmodeError);
		break;
	}
return ipstr;
}
#endif // PPP

//-------------------------------------------------------------------
unsigned char* linestate_2ascii(int linestate)
// Used for modem interface display
{
switch(linestate)
	{
	case LINESTATE_DIAL_WAIT:
		strcpy((char*)ipstr,szDialing);
		break;
	case LINESTATE_ANSWER_WAIT:		// TODO queue frame
		strcpy((char*)ipstr,szAnswering);
		break;
	case LINESTATE_OFFLINE:
		strcpy((char*)ipstr,szOffline);
		break;
	case LINESTATE_ONLINE:
		strcpy((char*)ipstr,szOnline);
		break;
	case LINESTATE_ONLINE_CMDS:
		strcpy((char*)ipstr,szOnlineCommands);
		break;
	
	default:
		strcpy((char*)ipstr,szLinestateError);
		break;
	}
return ipstr;
}

//-------------------------------------------------------------------
unsigned char* tcpstate_2ascii(int tcpstate)
// Convert TCP state machine state to a string
{
switch(tcpstate)
	{
	case SYN_SENT:
		strcpy((char*)ipstr,szTCP_SYN_SENT);
		break;
	case LISTEN:
		strcpy((char*)ipstr,szTCP_LISTEN);
		break;
	case SYN_RECEIVED:
		strcpy((char*)ipstr,szTCP_SYN_RECEIVED);
		break;
	case DELAYED_ACK:
	// Special case for TCP flow control, not in TCP state machiine
		strcpy((char*)ipstr,szTCP_DELAYED_ACK);
		break;
	case ESTABLISHED:
	// End can send data states
		strcpy((char*)ipstr,szTCP_ESTABLISHED);
		break;
	case CLOSE_WAIT:
		strcpy((char*)ipstr,szTCP_CLOSE_WAIT);
		break;
	case CLOSING_TCP:
		strcpy((char*)ipstr,szTCP_CLOSING_TCP);
		break;
	case LAST_ACK:
		strcpy((char*)ipstr,szTCP_LAST_ACK);
		break;
	case TIME_WAIT:
		strcpy((char*)ipstr,szTCP_TIME_WAIT);
		break;
	case FIN_WAIT1:
		strcpy((char*)ipstr,szTCP_FIN_WAIT1);
		break;
	case FIN_WAIT2:
		strcpy((char*)ipstr,szTCP_FIN_WAIT2);
		break;
	case STATE_NA:
		strcpy((char*)ipstr,szNA);
		break;
	default:
		strcpy((char*)ipstr,szTCPstateError);
		break;
	}
return ipstr;
}


//-------------------------------------------------------------------
unsigned char* baudrate_2ascii(int HW_IF)
// Return baudrate as a printable string
{

int baudrate = hw->uart[HW_IF].baudOversampleRate;
bool autobaud = hw->uart[HW_IF].uart_autobaud_active;	// 0 means autobaud off
if(autobaud)
	return (unsigned char*)szAutobaud;

#ifdef BAUD_1200
switch(baudrate)
	{
	case BAUDOVERSAMPLERATE_SLOW38400:
		strcpy((char*)baudstr,sz38400);
		break;
	case BAUDOVERSAMPLERATE_SLOW19200:
		strcpy((char*)baudstr,sz19200);
		break;
	case BAUDOVERSAMPLERATE_SLOW9600:
		strcpy((char*)baudstr,sz9600);
		break;
	case BAUDOVERSAMPLERATE_SLOW4800:
		strcpy((char*)baudstr,sz4800);
		break;
	case BAUDOVERSAMPLERATE_SLOW2400:
		strcpy((char*)baudstr,sz2400);
		break;
	case BAUDOVERSAMPLERATE_SLOW1200:
		strcpy((char*)baudstr,sz1200);
		break;	
	default:
		strcpy((char*)baudstr,szError);
		break;
	}
#else
switch(baudrate)
	{
	case BAUDOVERSAMPLERATE_115200:
		strcpy((char*)baudstr,sz115200);
		break;
	case BAUDOVERSAMPLERATE_57600:		
		strcpy((char*)baudstr,sz57600);
		break;
	case BAUDOVERSAMPLERATE_38400:
		strcpy((char*)baudstr,sz38400);
		break;
	case BAUDOVERSAMPLERATE_19200:
		strcpy((char*)baudstr,sz19200);
		break;
	case BAUDOVERSAMPLERATE_9600:
		strcpy((char*)baudstr,sz9600);
		break;
	case BAUDOVERSAMPLERATE_4800:
		strcpy((char*)baudstr,sz4800);
		break;
	default:
		strcpy((char*)baudstr,szError);
		break;
	}
#endif
return baudstr;
}

//-------------------------------------------------------------------
int16 baudrate_2uS(int baudrate)
// Return character transmission time in uS (MicroSeconds) based on baudrate constant
{
#ifdef BAUD_1200
switch(baudrate)
	{
	case BAUDOVERSAMPLERATE_SLOW38400:
		return 260;
	case BAUDOVERSAMPLERATE_SLOW19200:
		return 520;
	case BAUDOVERSAMPLERATE_SLOW9600:
		return 1040;
	case BAUDOVERSAMPLERATE_SLOW4800:
		return 2080;
	case BAUDOVERSAMPLERATE_SLOW2400:
		return 4160;
	case BAUDOVERSAMPLERATE_SLOW1200:
		return 8320;
	default:
		return 0;
	}
#else
switch(baudrate)
	{
	case BAUDOVERSAMPLERATE_115200:
		return 86;
	case BAUDOVERSAMPLERATE_57600:		
		return 170;
	case BAUDOVERSAMPLERATE_38400:
		return 260;
	case BAUDOVERSAMPLERATE_19200:
		return 520;
	case BAUDOVERSAMPLERATE_9600:
		return 1040;
	case BAUDOVERSAMPLERATE_4800:
		return 2080;
	default:
		return 0;
	}
#endif
}

//-------------------------------------------------------------------
unsigned char* flowcontrol_2ascii(int flowcontrol)
//Convert flowcontrol constant to printable string
{
switch(flowcontrol)
	{
	case uartFlowControl_OFF:
		strcpy((char*)flowstr,szNOFLOW);
		break;
	case uartFlowControl_XONXOFF:
		strcpy((char*)flowstr,szXONXOFF);
		break;
	case uartFlowControl_RTSCTS:
		strcpy((char*)flowstr,szRTSCTS);
		break;
	case uartFlowControl_CTS:
		strcpy((char*)flowstr,szCTS);
		break;
	case uartFlowControl_RTSCTS_ONLINE:
		strcpy((char*)flowstr,szRTSCTS_ONLINE);
		break;
	default:
		strcpy((char*)flowstr,szError);
		break;
	}
return flowstr;
}

//-------------------------------------------------------------------
unsigned char* dataformat_2ascii(int dataformat)
// Convert RS232 data format to printable string
//Note: uart_parity  0: 8bit-data 1: 7bit-data,even 2: 7bit-data,odd	
{
switch(dataformat)
	{
	case FORMAT_8N:
	strcpy((char*)macstr,sz8N);
	break;
	case FORMAT_7E:
	strcpy((char*)macstr,sz7E);
	break;
	case FORMAT_7O:
	strcpy((char*)macstr,sz7O);
	break;
	default:
	sprintf(macstr,szError2,dataformat);
	break;
	}
return macstr;
}


//-------------------------------------------------------------------
void objects_2ascii(unsigned char* buf)
// Convert params from a CClient object toa printable string
// Used to display status ena/disabled and time idle
{
int32 currenttime = getTickCount();	// 1ms units
int32 idle_secs;
int16 n=2;
strcpy((char*)buf,szEndline);			

for(int i=0; i < MAXL3CLIENTS;i++)
	{
	if(ip_protocols[i] != NULL)
		{
		idle_secs =  currenttime - ip_protocols[i]->time;
		idle_secs = idle_secs / 1000; // >>1024 ??
		sprintf(&buf[n],szConnectDisplay,i+1,ip_protocols[i]->name,ip_protocols[i]->protocol,ip_protocols[i]->pClient? ip_protocols[i]->pClient->name:szNone,tcpstate_2ascii(ip_protocols[i]->state),ip_protocols[i]->enabled_flag? szEnabled:szDisabled,idle_secs);
		n = strlen((const char*)buf);
		}
	}
//n = freemem();
//strcat(buf)
}


/*
//-------------------------------------------------------------------
void tracelevel_2ascii(unsigned char* buf)
// Not fully implemented.
// traceout.level constants
//#define LOG_NONE			0			// No errors
//const int LOG_ERR			=1;			// Baaad errors
//const int LOG_WARNING		=2;
//const int LOG_NOTICE		=3;	
//const int LOG_INFO			=4;			// Minor things
//const int LOG_DEBUG			=5;			// Programmer stuff / all
//const int LOG_DEFAULT		=LOG_INFO;	// Default logging level
{
switch(traceout.level)
	{
	case LOG_ERR:
	break;
	case LOG_WARNING:
	break;
	case LOG_NOTICE:
	break;
	case LOG_INFO:
	break;
	case LOG_DEBUG:
	break;
	default:
	break;
	}
}

//-------------------------------------------------------------------
void tracemodule_2ascii(unsigned char* buf)
// Not fully implemented.
// traceout.module constants. LOG_DEBUG used for non-category stuff
{
switch(traceout.module)
	{
	case LOG_LAN:
	break;
	case LOG_PPP:
	break;
	case LOG_ARP:
	break;
	case LOG_LCP:
	break;
	case LOG_AUTH:
	break;
	case LOG_TCP:
	break;
	case LOG_IP:
	break;
	case LOG_SYSTEM:
	break;
	case LOG_HTML:
	break;
	case LOG_TCPMODEM:
	break;
	case LOG_ICMP:
	break;
	case LOG_DHCP:
	break;
	case LOG_UDP:
	break;
	case LOG_BUG:
	break;
	case LOG_ALL:
	break;
	case LOG_NONE:
	break;
	default:
	break;
	}
}
*/

