// tftp.cpp
// Implementation of rfc783
// Trivial File Transfer Protocol. Up/download files to another computer.
// Intended for (but not limited to) loading and saving configuration profiles from flash drive.
// Limitations :
// No retransmission if an ACK is lost
// All RECEIVED (WRQ) files are stored in 8.3 format, and names converted to lowercase
// Read request (RRQ) pFilename is case sensitive

#ifdef TFTP

#include "router.h"
#include "pppd.h"
#include "protocols.h"
#include "tcp.h"
#include "udp.h"
#include "utils.h"
#include "C:\DataDir\Adsp21xx\seb\VC5410A\pmRun\schedule.h"
#include "C:\DataDir\Adsp21xx\seb\VC5410A\pmRun\hardware.h"
#include "tftp.h"
#include "ffs.h"

//--------------------------------------------------------------------------
int Ctftp::OnTransport(int message,int unused)
{
if(message== TRANSPORT_CLOSE)
	{
	// TCP connection closed.
	// Reboot if update received
	if(bBootSoftwareUpdate)
		{
		if( 0 != ffs->update_sofware())
			// Update failed. Boot code may be missing in the worst case
			return 0;
		}
		
	if(bSoftwareUpdate)
		watchDogTimerDisable=1;
	}
	
return 0;
}

//--------------------------------------------------------------------------
void Ctftp::init(int,CProtocol_L3* pUDP,bool bAutoDelete)
{
name = szTFTP;
outbuf = &packet_buf[UDP_DATA_OFFSET];
transport = pUDP;
autodelete = bAutoDelete;
bSendingFile = false;
quit = false;
mode = MODE_NETASCII;
bSoftwareUpdate = false;
bBootSoftwareUpdate = false;
pFilename[0]=NULL;
}

//------------------------------------------------------------------------
int Ctftp::receive(unsigned char **udp_data,unsigned int *len,unsigned int window)
{
int16 opcode,thisblocknumber;
unsigned char *p = (unsigned char*)*udp_data;
int errorcode = ERR_UNDEFINED;
char* pExtn=NULL;
const char* pError=szNull;				// Error text
int	resp = ACKNOWLEDGE;
int length = *len;

outbuf = &packet_buf[UDP_DATA_OFFSET];
*udp_data = outbuf;
int udpbytes = 0;

if(!lan[UNSAVED].permitTFTP)
	{// TFTP is disabled
	OutputDebugString(LOG_INFO,LOG_UDP,szTFTPDisabled);
	return TERMINATE;
	}
		
GETSHORT(opcode,p);
switch(opcode)
	{
	case TFTP_RRQ:
	// Read request
	// Change local port number (our TID)
	transport->port_local = (int16)getTickCount();
	
	// Extract the mode.. (binary, netascii, mail..)
	strcpy((char*)pMode,(char*)(p + strlen((char*)p) + 1));
	if(strstr((char*)pMode,szTFTPnetascii))
		mode = MODE_NETASCII;
	else if(strstr((char*)pMode,szTFTPoctet))
		mode = MODE_OCTET;
	else if(strstr((char*)pMode,szTFTPmail))
		mode = MODE_MAIL;
	
	// Extract the filename and extension..
	strcpy((char*)pFilename,(const char*)p);
	if(!ffs->isvalid_file_extn(pFilename, &pExtn))
		{
		// Filename & extension too long (must be 8.3)
		OutputDebugString(LOG_INFO,LOG_UDP,szTFTPFileNameError);
		errorcode = ERR_ILLEGAL_OPERATION;
		pError = szTFTPFileNameError;
		resp = NACKNOWLEDGE;
		break;
		}
			
	if(! ffs->open(MODE_OPENEXISTING,(char*)pFilename,pExtn))
		{
		errorcode = ERR_FILE_NOT_FOUND;
		pError = szTFTPFileNotFound;
		resp = NACKNOWLEDGE;
		break;
		}

	sprintf(msg,szTFTPread,pFilename,pExtn,pMode);	
	OutputDebugString(LOG_INFO,LOG_UDP,(const char*)msg);

	udpbytes = ffs->read((int16*)&outbuf[4],TFTP_MAX_BLOCKSIZE,mode);

	blocknumber = 1;
	if(udpbytes)
		{
		if(udpbytes < TFTP_MAX_BLOCKSIZE)
			ffs->close(true);
		else
			// More to send. Next block will be sent when an ACK is received
			bSendingFile = true;
		}
	else
		{
		// The file is empty!!. Send an empty frame to terminate
		udpbytes = TFTP_MAX_BLOCKSIZE + 1;		// Special value
		bSendingFile = false;
		ffs->close(true);
		}		
	break;

	case TFTP_WRQ:
	// Write request - remote sending us a file
	// Change local port number (our TID)
	transport->port_local = (int16)getTickCount();
	
	// Extract the mode.. (binary, netascii, mail..)
	strcpy((char*)pMode,(char*)(p + strlen((char*)p) + 1));
	if(strstr((char*)pMode,szTFTPnetascii))
		mode = MODE_NETASCII;
	else if(strstr((char*)pMode,szTFTPoctet))
		mode = MODE_OCTET;
	else if(strstr((char*)pMode,szTFTPmail))
		{
		//mode = MODE_MAIL;
		// I don't permit mail yet
		OutputDebugString(LOG_INFO,LOG_UDP,szTFTPMailUnsupported);
		errorcode = ERR_ILLEGAL_OPERATION;
		pError = szTFTPMailUnsupported;
		resp = NACKNOWLEDGE;
		break;
		}
	
	// Extract the filename and extension..
	makelower(pFilename);
	strcpy((char*)pFilename,(char*)p);
	if(!ffs->isvalid_file_extn(pFilename, &pExtn))
		{
		// Filename & extension too long (must be 8.3)
		OutputDebugString(LOG_INFO,LOG_UDP,szTFTPFileNameError);
		errorcode = ERR_ILLEGAL_OPERATION;
		pError = szTFTPFileNameError;
		resp = NACKNOWLEDGE;
		break;
		}

	if(0 == strcmp(pExtn,szBin))
		{
		// A software update file ?
		if((0 == strcmp((char*)pFilename,szRunFile)) ||
			(0 == strcmp((char*)pFilename,szBootFile)))
			{
			OutputDebugString(LOG_INFO,LOG_SYSTEM,szTFTPReceivingUpdate);
			bSoftwareUpdate = true;
			}
		if(0 == strcmp((char*)pFilename,szBootFile))
			bBootSoftwareUpdate = true;
		}

	ffs->setsector(szDefaultSector);

	if(0 == strcmp(pExtn,szFILETYPE_PROFILE))
		{
		// A profile file. 
		// Delete any existing profile of the same name
		sprintf(msg,szTFTPReceivingProfile,pFilename,pExtn);
		OutputDebugString(LOG_INFO,LOG_SYSTEM,(char*)msg);
		ffs->del(pFilename,pExtn);
		}
	
	if(0 == strcmp(pExtn,szFILETYPE_TEXT))
		{
		if(0 == strcmp((char*)pFilename,szProfileBoot))
		// A file specifying boot (default) profile. 
		// Delete any existing profile of the same name
			OutputDebugString(LOG_INFO,LOG_SYSTEM,szTFTPReceivingBootfile);
			ffs->del((unsigned char*)szProfileBoot,pExtn);
		}

	if(! ffs->open(MODE_CREATE,(char*)pFilename,pExtn))
		{
		// TODO does it exist, or is the disk full ?
		errorcode = ERR_FILE_EXISTS;	// or ERR_DISK_FULL
		pError = szTFTPFileExists;
		resp = NACKNOWLEDGE;
		break;
		}
	
	// Remember the filename	
	sprintf(msg,szTFTPwrite,pFilename,pExtn,pMode);	
	OutputDebugString(LOG_INFO,LOG_UDP,(const char*)msg);
	blocknumber = 0;
	break;

	case TFTP_DATA:
	// Got a block of data from remote. Save to flash
	sprintf(msg,"TFTP got block %u (%u)",blocknumber,length-4);
	OutputDebugString(LOG_INFO,LOG_UDP,(const char*)msg);
	
	GETSHORT(thisblocknumber,p);	
	if (thisblocknumber != blocknumber + 1)
		{
//		errorcode = 
		resp = NACKNOWLEDGE;
		break;
		}

	blocknumber = thisblocknumber;
	length -=4;

	if(length==0)
		break;
		

	if(mode==MODE_OCTET)
		// Binary. 2 chars saved as INT16
		length = ffs->write((int16*)p,length,MODE_CHAR);		// Returns bytes written
	else
		// Ascii file. Char saved as INT16
		ffs->write((int16*)p,length,MODE_INT16);		// Returns bytes written
	
	if(length == 0)
		{
		// Flash write error
		resp = NACKNOWLEDGE;
		errorcode = ERR_DISK_FULL;
		pError = szTFTPDiskFull;
		ffs->close(true);
		// Delete the file
		ffs->del(pFilename,ffs->current_file.ext);
		break;
		}
	else if(length < TFTP_MAX_BLOCKSIZE)
		{
		// Less than TFTP_MAX_BLOCKSIZE (512 bytes) of data signals the end of the transfer
		resp = TERMINATE;
		ffs->close(true);
		break;
		}
	else
		{
		// Flash write was OK
		// Delay the response as we just wrote flash
		// Get UDP to send ACK after 25 milliseconds
		resp=ACK_DELAYED;
		}
	break;
	
	case TFTP_ACK:
	// They ACK a block of data
	// TODO check the block number
	if(bSendingFile)
		{
		udpbytes = ffs->read((int16*)&outbuf[4],TFTP_MAX_BLOCKSIZE,mode);
		if(udpbytes)
			{
			if(udpbytes < TFTP_MAX_BLOCKSIZE)
				{
				// Last block of data for this file
				bSendingFile = false;
				ffs->close(true);
				}
			}
		else
			{
			// The file was a multiple of 512 bytes. Send an empty frame to terminate
			udpbytes = TFTP_MAX_BLOCKSIZE + 1;		// Special value
			ffs->close(true);
			bSendingFile = false;
			}
		}
	else
		// Acknowledgement for last block. Sending file finished
		resp = NORESPONSE;
		
	break;

	case TFTP_ERROR:
	if(bSendingFile)
		ffs->close(true);

	// Terminate with no response as per rfc783
	resp = NORESPONSE;
	break;
	}

// Make up a response frame at packet_buf[UDP_DATA_OFFSET]
p = outbuf;
	
switch(resp)
	{
	case ACKNOWLEDGE:
	if(udpbytes)
		{
		// Send data
		if(udpbytes == TFTP_MAX_BLOCKSIZE + 1)
			// Special case, empty data frame
			udpbytes = 0;

		PUTSHORT(TFTP_DATA,p);
		PUTSHORT(blocknumber,p);
		blocknumber++;
		}
	else
		{
		// ACK with no data
		PUTSHORT(TFTP_ACK,p);
		PUTSHORT(blocknumber,p);
		}

	*len = 4 + udpbytes;
	break;

	case NACKNOWLEDGE:
	PUTSHORT(TFTP_ERROR,p);
	PUTSHORT(errorcode,p);
	strcpy((char*)p,pError);
	*len = 5 + strlen(pError);
	break;
	
	case TERMINATE:
	// ACK last block and terminate
	PUTSHORT(TFTP_ACK,p);
	PUTSHORT(blocknumber,p);
	*len = 4;
	break;
	
	case NORESPONSE:
	*len = 0;
#ifdef DHCP
// AGH ! This was likking the unit.
// TODO why is it here anyway ?
// 	transport->enabled_flag = false;
#endif
	break;
	
	case ACK_DELAYED:
	*len = blocknumber;
	break;
	}
	
return resp;
}

#endif // TFTP - the whole file
