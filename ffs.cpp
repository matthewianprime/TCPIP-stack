// ffs.cpp
// Flash File System routines
// Also includes cold start default routines

#include "router.h"
#include "pppd.h"
#include "ffs.h"
#include "utils.h"
#include "lan.h"
#include "C:\DataDir\Adsp21xx\seb\VC5410A\pmRun\schedule.h"
#include "C:\DataDir\Adsp21xx\seb\VC5410A\pmRun\hardware.h"

#define SE_ERASE			2
#define PACKET_DATA_SIZE	128

extern "C" 
{
extern unsigned int FlsRead( unsigned long Faddr );
extern int FLASH_Write(unsigned long Faddr, unsigned *pData, unsigned Count);
extern bool isvalidsector(char sector);
int tectronix_decode(int testrun);
void flashControl_sectorerase( int mode , char sector );
}
extern int access_boot_sector;

int Cffs::update_sofware()
// Returns 0 if ok
{
int res;

			OutputDebugString(LOG_NOTICE,LOG_SYSTEM,"\r\n");
			ffsReset(1);
			if(open(MODE_OPENEXISTING,"pmboot2","bin"))
				{
	 			OutputDebugString(LOG_NOTICE,LOG_SYSTEM,"\r\nUpdated bootcode found:verifying integrity...");
				// Test run to verify integrity of pmboot2
	 			res=tectronix_decode(1);
				if(res==0)
					{
					// Erase boot programme and programme the new one
					access_boot_sector = true;
					if(!flash_erase('J'))
		 				{
		 				OutputDebugString(LOG_NOTICE,LOG_SYSTEM,"Sector J erase fail\r\n");
						return 1;
						}
	 				OutputDebugString(LOG_NOTICE,LOG_SYSTEM,"Sector J erased\r\n");

		 			OutputDebugString(LOG_NOTICE,LOG_SYSTEM,"OK\r\nLoading bootcode\r\n");
					ffsReset(1);
					open(MODE_OPENEXISTING,"pmboot2","bin");
					// Program flash
		 			res=tectronix_decode(0);
					if(res==0)
			 			OutputDebugString(LOG_NOTICE,LOG_SYSTEM,"\r\nBootcode upgraded\r\n");
					// Erase the downloaded copy of the upgrade
					flash_erase('C');
					}

				if(res!=0)
		 			OutputDebugString(LOG_NOTICE,LOG_SYSTEM,"FAIL\r\nCorrupt Bootcode deleted\r\n");
				};
				
access_boot_sector = false;
return res;
}

//--------------------------------------------------------------------------------	
bool Cffs::GetMac(int16* mac)
// Reads mac address from protected flash sector.
// Returns false if all 0xFFs (unprogrammed), otherwise true
{
/*
// Algorithm from V1.70 to 1.73
int16* temp = mac;
unsigned long address = 0x004f8000L;
// Algorithm from V1.70
*(temp) = FlsRead(address++) & 0xff;
*(temp++) += (FlsRead(address++))<< 8;

*(temp) = FlsRead(address++) & 0xff;
*(temp++) += (FlsRead(address++))<< 8;

*(temp) = FlsRead(address++) & 0xff;
*(temp++) += (FlsRead(address++))<< 8;

if( (mac[0] == 0xffff) && (mac[1] == 0xffff) && (mac[2] == 0xffff) )
	return false;

return true;
*/

// Algorithm up to and including V1.69 and from 1.74 onwards
int16* temp = mac;
unsigned long address = 0x004f8000L;

*(temp++) = FlsRead(address++);
*(temp++) = FlsRead(address++);
*(temp) = FlsRead(address);

if( (mac[0] == 0xffff) && (mac[1] == 0xffff) && (mac[2] == 0xffff) )
	return false;

return true;
}

//--------------------------------------------------------------------------------	
void Cffs::SetMac(int16* mac)
// Writes mac address to protected flash sector.
{
unsigned long address = 0x004f8000L;		// Sector K in flashcontrol.cpp
/*
// Algorithm from V1.70 to 1.73
int16 mac16[6];
mac16[0] = mac[0] & 0xff;
mac16[1] = (mac[0] >> 8 ) & 0xff;

mac16[2] = mac[1] & 0xff;
mac16[3] = (mac[1] >> 8 ) & 0xff;

mac16[4] = mac[2] & 0xff;
mac16[5] = (mac[2] >> 8 ) & 0xff;

FLASH_Write(address, mac16,6);
*/

// Algorithm up to and including V1.69 and from 1.74 onwards
FLASH_Write(address, mac,3);
}

//--------------------------------------------------------------------------------
void Cffs::init()
// Initialise flash file system
{
fs.sector = szDefaultSector;	// Currently active flash sector
fs.sector_NULL = NULL;

ffsReset();

if( !chkdsk(fs.sector))
	{
	format(fs.sector);
	chkdsk(fs.sector);
	}
}

//--------------------------------------------------------------------------------
void Cffs::ffsReset(bool discard_currentfile)
// Reset file system
// Call before enumfiles().
{
// file system global variables
fs.fileptr = 0;					// Current file pointer position
fs.sector_NULL=NULL;

// Last file enumerated
//enumerate_file.filename[0] = 0xff;	// Text file name with trailing NULL. If filename[0]==0xff (or 0x00) entry is empty (been deleted)
enumerate_file.ext[0] = NULL;			// Text file extenstion with trailing NULL
enumerate_file.dataptr = 0;				// Offest of first byte of file
enumerate_file.len = 0;					// Length of file
enumerate_file.fragment = 0;
enumerate_file.zero = 0xffff;

if(discard_currentfile)
	{
	// The open file
	// These details are saved when a new fragment is created
	current_file.filename[0] = 0xff;		// Text file name with trailing NULL. If filename[0]==0xff (or 0x00) entry is empty (been deleted)
	current_file.ext[0] = NULL;				// Text file extension with trailing NULL
	current_file.dataptr = 0;				// Offest of first byte of file
	current_file.len = 0;					// Length of file
	current_file.fragment = 0;
	current_file.zero = 0xffff;
	}
}

//--------------------------------------------------------------------------------
bool Cffs::enumfiles(bool return_erased_files)
// Purpose: enumerate files saved in flash
// Nb. Call ffsReset() before caling this func to reset enumeration.

// Returns:  
// True if a file found ( whether deleted or not - deleted filenames start with a NULL char)
// False if no more files
// File info is saved in global struct "enumerate_file"
// On exit global file pointer "fs.fileptr" points to last enumerated file
{
int last_dataptr;
int last_len;

set_fileptr:
if(fs.fileptr == 0)								// Skip to fat table
	// Starting a new enumeration
	fs.fileptr = sizeof(FLASH_HEADER);
else
	fs.fileptr += sizeof(FATentry);				// Point to next FAT table entry

last_dataptr = enumerate_file.dataptr;
last_len = enumerate_file.len;

Flash_load(fs.sector,fs.fileptr,(int16*)&enumerate_file,sizeof(FATentry));	// Get FAT table entry structure

if(enumerate_file.filename[0] == 0xffff)
	{
	// Empty unused location
//	OutputDebugString(LOG_NOTICE,LOG_SYSTEM,(const char*)"FILE Empty unused location");
	enumerate_file.dataptr = last_dataptr;
	enumerate_file.len = last_len;
	return false;
	}
	
if(enumerate_file.filename[0] == 0)
	{
	// Erased file
	
	if(return_erased_files)
		{
//		sprintf(msg,"FILE Empty location (erased file _%s)",&enumerate_file.filename[1]);
//		OutputDebugString(LOG_NOTICE,LOG_SYSTEM,(const char*)msg);
		return true;
		}
	else
		// Only want returns for unerased files
		goto set_fileptr;
	
	}
	
//OutputDebugString(LOG_NOTICE,LOG_SYSTEM,(const char*)msg);
return true;
}

//--------------------------------------------------------------------------------
bool Cffs::copy(const char *dest_fn,const char *dest_ext,const char *src_fn,const char *src_ext,char dest_sector)
// Copy a file.
// sector param is the destination sector for the file copy. Must be formatted !
// Source sector is fs.sector (ie the current sector )
{
if(!isvalidsector(dest_sector))
	return false;
if(! open(MODE_OPENEXISTING,src_fn,src_ext) )
	return false;
if(current_file.len > 2000)
	{
	// TODO File too big !! Should calculate space or something
	close(false);
	return false;
	}

// Copy source file data to packet_buf
int len = read((int16*)packet_buf,current_file.len);
close(false);

char sector_saved = fs.sector;
fs.sector = dest_sector;

// Create destination file
if(! open(MODE_CREATE,dest_fn,dest_ext) )
	{
	fs.sector = sector_saved;
	return false;
	}

// Write source file data to destination
write((int16*)packet_buf,len);
close(false);

fs.sector = sector_saved;
/// File pointer invalid for this sector
ffsReset();

return true;
}

//--------------------------------------------------------------------------------
bool Cffs::driveclean()
// Clean deleted files from flash drive:
// 1/ Delete upgrade files pmrun2.bin and pmboot2.bin
// 2/ Copy all files to alternate flash sector
// 3/ Erase sector-to-clean and format it
// 4/ Copy files from alternate to primary sector
// Only deletes system files if delete_any=true
{
// Are there any deleted files?
bool deleted_file_found = false;
del((unsigned char*)szRunFile,szBin,true);
del((unsigned char*)szBootFile,szBin,true);

ffsReset();

while(enumfiles())
	{
	if(enumerate_file.filename[0] == NULL)
		{
		// Found a deleted file
		deleted_file_found = true;
		break;
		}
	};
	
if(! deleted_file_found)
	{
	// Nothing to do
//	serialout((char*)"\r\nNo files to clean",HW_DTE0);
	return false;
	}

// Format backup sector
format(szBackupSector);

ffsReset();
char fn[FILENAME_LEN+1];
char ext[FILEEXTENSION_LEN+1];

// Copy files from primary to alternate sector
while(enumfiles(false))
	{
	strcpy(fn,enumerate_file.filename);
	strcpy(ext,enumerate_file.ext);
	
	copy(fn,ext,fn,ext,szBackupSector);	// Calls ffsReset
	
	// Delete the source file
	del((unsigned char*)fn,ext,true);
	
	// Reset enumeration
	ffsReset();
	};

// Format default sector
format(szDefaultSector);

if(0 == setsector(szBackupSector,false))		// Source file sector
 	return false;

//sprintf(msg,"\r\nCopying from sector %s",&fs.sector);
//serialout((char*)msg,HW_DTE0);

ffsReset();
// Copy files from alternate to primary sector
while(enumfiles(false))
	{
	strcpy(fn,enumerate_file.filename);
	strcpy(ext,enumerate_file.ext);
	
	copy(fn,ext,fn,ext,szDefaultSector);	// Calls ffsReset
	
	// Delete the source file
	del((unsigned char*)fn,ext,true);
	
	// Reset enumeration
	ffsReset();
	};

// Format backup sector to erase temporary files
format(szBackupSector);
setsector(szDefaultSector,false);		// Select primary file sector

return true;
}

//--------------------------------------------------------------------------------
bool Cffs::open(int mode, const char *filename,const char *ext)
// Open a file
// Returns true or false
// If successful, sets the file pointer to the beginning of the data ready for a read or write operation
{
if((strlen(filename) > FILENAME_LEN) || (strlen(ext) > FILEEXTENSION_LEN))
	{// Filename or extension is too long
//	sprintf(msg,"FILE Filename extension not in 8.3 format ( %s.%s = %u.%u )",filename,ext,strlen(filename),strlen(ext));
//	serialout((const char*)msg,HW_DTE0);
	return false;
	}
	
if(ext == NULL)
	ext=szFILETYPE_PROFILE;
	
bool file_exists=false;

// See if the file already exists
ffsReset();										// Reset file system pointers
while(enumfiles(false))
	{
	if( 0 == strcmp(enumerate_file.filename,filename))	// Returns 0 if names match
		{
		if( 0 == strcmp(enumerate_file.ext,ext))	// Returns 0 if names match
			{
			file_exists=true;
			goto foundfile;
			}
		}
	};

foundfile:
switch(mode)
	{
	case MODE_OPENEXISTING:

	if(!file_exists)
		{// File not found
		sprintf(msg,szFileOpenNotFound,&fs.sector,filename,ext);
		OutputDebugString(LOG_NOTICE,LOG_SYSTEM,(const char*)msg);
		return false;
		}
			
	// File exists. Copy details to working area
	memcpy(&current_file,&enumerate_file,sizeof(FATentry));
	break;
	
	case MODE_CREATE:

	if(file_exists)
		{// File exists so cannot create
		sprintf(msg,szFileCreateExist,&fs.sector,filename,ext);
		OutputDebugString(LOG_NOTICE,LOG_SYSTEM,(const char*)msg);
		return false;
		}
	
	// File does not exist. 
	// Copy new file details to working area
	strcpy(current_file.filename,filename);
	strcpy(current_file.ext,ext);
	current_file.len = 0;									// write() increments this var.
	current_file.zero = 0;
	if(fs.fileptr == sizeof(FLASH_HEADER))
		current_file.dataptr = FILE_DATA_AREA_START;		// This is the first file in the file system
	else
		current_file.dataptr = enumerate_file.dataptr + enumerate_file.len;	// First free location follows last file enumerated

	break;

	case MODE_OPENORCREATE:
	break;
	}

// Set global file pointer for write operation
fs.fileptr = current_file.dataptr;

sprintf(msg,szFileOpenCreate,&fs.sector,filename,ext,(file_exists)? szFileOpened:szFileCreated,current_file.len);
OutputDebugString(LOG_NOTICE,LOG_SYSTEM,(const char*)msg);
return true;
}

//--------------------------------------------------------------------------------
int Cffs::read(int16* data,int16 len,int mode)
// Reads max of: len, bytes remaining in file.
// Returns no of bytes read
// mode param is either MODE_CHAR or MODE_INT16
// MODE_INT16 reads 16 bit data from flash into data array
// MODE_CHAR reads 16 bit data from flash into character array.Char array must be twice the size of the file
{
int int16sremaining = fs.fileptr - current_file.dataptr;	// bytes already read from file
int16sremaining = current_file.len - int16sremaining;		// bytes remaining to read
int int16s_toread;

/*
if(bytestoread < len)
	{
	sprintf(msg,"FFS EOF encountered bytestoread=%u bytesremaining=%u len=%u\r\n",bytestoread,bytesremaining,len);
	OutputDebugString(LOG_NOTICE,LOG_SYSTEM,(const char*)msg);
	}
*/
if(mode == MODE_INT16)
	{
	int16s_toread = min(len,int16sremaining);				// INT16s to read
	Flash_load(fs.sector,fs.fileptr,data,int16s_toread);	// Get filename structure
	fs.fileptr += int16s_toread;
	}
else if(mode == MODE_CHAR)
	{
	len>>=1;												// Convert byte length to length in int16s
	int16s_toread = min(len,int16sremaining);				// bytes to read
	Flash_load_bytearray(fs.sector,fs.fileptr,data,int16s_toread);	// Get filename structure
	fs.fileptr += int16s_toread;
	int16s_toread<<=1;									// Return number of bytes read (not int16s)
	}

return int16s_toread;
}

//--------------------------------------------------------------------------------
int Cffs::write(int16* data,int16 len,int mode)
// Write data to system file pointer location
// Returns no of int16s or bytes written
// MODE_CHAR Data is a byte array. len is in bytes. Pack and save as int16s
// MODE_int16 Data is an int16 array
{
//sprintf(msg,"FILE writing %u bytes to sector %s location %u",len,&fs.sector,fs.fileptr);
//OutputDebugString(LOG_NOTICE,LOG_SYSTEM,(const char*)msg);
int16	m;

// Do not exceed remaining flash sector space
// We always write at the beginning of free flash space
m = FLASH_SECTOR_SIZE - (fs.fileptr + len);	// Disk free space
if((len >= m) || ( m > FLASH_SECTOR_SIZE))
	{
	if(!createnewfragment())
		{// Failed to find additional space
		sprintf(msg,szFFSDiskFull,m);
		OutputDebugString(LOG_WARNING,LOG_SYSTEM,(const char*)msg);
		return 0;
		}
	}
	
// Write file entry
if(mode == MODE_INT16)
	{
	// Data saved as int16s
	Flash_save(fs.sector,fs.fileptr,data,len);
	fs.fileptr += len;
	current_file.byteswritten += len;
	}
else if(mode == MODE_CHAR)
	{
	// Data is a byte array. len is in bytes. Save as int16s
	// No erase option. "len" is in bytes
	// Chars are packed as int16s to save flash space
	len = Flash_save_bytearray(fs.sector,fs.fileptr,data,len);
	m = len/2;
	fs.fileptr += m;
	current_file.byteswritten += m;
	}

return len;
}

//--------------------------------------------------------------------------------
bool Cffs::createnewfragment()
// Added for ffs V2 sector spanning
// Creates a new file with same name as current file
// in the next flash sector.
{
current_file.len=0;

// Fragmented file fragment numbers start from 1.
// Non fragmented files have fragment number 0
if(current_file.fragment == 0)
	current_file.fragment = 1;
	
// Save current file details and close it
memcpy(&fragmented_file.filename[0],&current_file.filename[0],sizeof(FATentry));
close(false);

// Select next sector
fs.sector++;
if(!isvalidsector(fs.sector))
	{
	fs.sector--;
	return false;
	}
	
format(fs.sector);

// Create a new file of same name
if(!open(MODE_CREATE,fragmented_file.filename,fragmented_file.ext))
	return false;

current_file.fragment = fragmented_file.fragment+1;

//sprintf(msg,"FILE created fragment %u",current_file.fragment);
//OutputDebugString(LOG_NOTICE,LOG_SYSTEM,(const char*)msg);

bFragmented_File=true;
return true;
}

//--------------------------------------------------------------------------------
bool Cffs::close(bool fs_reset)
// If this is a new file, write its FAT table entry
// If this is a modified file, 
{
if(!current_file.byteswritten)
	{
	// File unchanged.
	sprintf(msg,szFileCloseNoChange,&fs.sector,current_file.filename,current_file.ext);
	OutputDebugString(LOG_NOTICE,LOG_SYSTEM,(const char*)msg);		
	if(fs_reset) ffsReset();										// Reset file system pointers
	return true;
	}

// The file has been written to or appended
	
// Write FAT table entry on close
if(current_file.len == 0)
	{
	// We just created this file then written some data
	// Now save its FAT table entry

	// Set the file pointer to the last entry of the FAT (don't use ffsReset() !)
	fs.fileptr = 0;
	while(enumfiles(false));												
	
	// Pointing at empty entry in FAT table now

	current_file.len = current_file.byteswritten;
	current_file.byteswritten = 0;
	Flash_save(fs.sector,fs.fileptr,(unsigned int*)&current_file,sizeof(FATentry));
	
	sprintf(msg,szFileClose,&fs.sector,current_file.filename,current_file.ext,current_file.len,current_file.fragment);
	OutputDebugString(LOG_NOTICE,LOG_SYSTEM,(const char*)msg);
	if(fs_reset) ffsReset();										// Reset file system pointers
	return true;
	}

// File has been appended.
// TODO We should delete the current FAT entry and create a new one
sprintf(msg,szFileCloseAppendErr,&fs.sector,current_file.filename,current_file.ext);
OutputDebugString(LOG_NOTICE,LOG_SYSTEM,(const char*)msg);
del((unsigned char*)current_file.filename,current_file.ext,true);
if(fs_reset) ffsReset();
return true;
}


//--------------------------------------------------------------------------------
bool Cffs::isvalid_file_extn(unsigned char* filename_extn, char **extn)
// Convert a text string to an 8.3 filename.extension
// Return false if the format is invalid
// Corrupts input string by changing '.' to a NULL if the format is valid.
{
char* pDot = strchr((char*)filename_extn,'.');
if(pDot == NULL)
	return false;
if((pDot - (char*)filename_extn) > FILENAME_LEN)
	// Filename too long
	return false;

if(strlen(pDot+1) > FILEEXTENSION_LEN)
	// File extension too long
	return false;

*pDot = NULL;					// NULL terminate filename
*extn = pDot+1;
return true;
}

//--------------------------------------------------------------------------------
bool Cffs::delfile(unsigned char* filename_extn,bool delete_system_files)
// Wrapper for Cffs::del()
// Accepts filename_extn paramater in format "filename.ext"
{
char *extn;
if(!isvalid_file_extn(filename_extn, &extn))
	return false;

return del(filename_extn,extn,delete_system_files);
}

//--------------------------------------------------------------------------------
bool Cffs::del(unsigned char* filename,const char* ext,bool delete_system_files)
// Mark a file as deleted by changing the first char of the filename to 0H
// Only deletes system files if delete_anyfile=true
{
// Find the file....
ffsReset();

while(enumfiles(false))
	{
	if(0 == strcmp(enumerate_file.filename,(char*)filename))	// Returns 0 if names match
		{
		if(0 == strcmp(enumerate_file.ext,ext))
			goto foundfile;
		}
	};

// File not found
//	sprintf(msg,"FILE delete %s.%s not found",filename,ext);
//	serialout((char*)msg,HW_DTE0);
//	OutputDebugString(LOG_NOTICE,LOG_SYSTEM,(const char*)msg);
return false;

foundfile:
// Add other protected file types here
if(!delete_system_files)
	{
	if(0 == strcmp(ext,szFILETYPE_SYSTEM))
		{
//		sprintf(msg,"FILE not deleting system file %s",filename);
//		OutputDebugString(LOG_NOTICE,LOG_SYSTEM,(const char*)msg);
		return false;
		}
	}
		
//sprintf(msg,"FILE deleting %s",filename);
//OutputDebugString(LOG_NOTICE,LOG_SYSTEM,(const char*)msg);

// Overwrite the first byte of the filename with a NULL
int16 data = NULL;
Flash_save(fs.sector,fs.fileptr,&data,1);	

return true;
}

//--------------------------------------------------------------------------------
bool Cffs::chkdsk(char sector)
// Flash disk integrity check.
{
if(sector == 0)
	sector = fs.sector;

int16 iVal;
Flash_load(sector,0,&iVal,1);
if(iVal!= 0x5a5a)						// Is this Flash sector formatted ?
	{
	// Not formatted
	sprintf(msg,szDiskErr,&fs.sector);
	OutputDebugString(LOG_NOTICE,LOG_SYSTEM,(const char*)msg);
	return false;
	}

//sprintf(msg,"Disk check ok val=0x%X sector=%s",iVal,&fs.sector);
//OutputDebugString(LOG_NOTICE,LOG_SYSTEM,(const char*)msg);
return true;
}

//--------------------------------------------------------------------------------
void Cffs::format(char sector)
// Format a flash chip sector with the FLASH_HEADER
// 1. Erase sector
// 2. Write FLASH_HEADER to sector
{
if(sector == 0)
	sector = fs.sector;

char str_sect[2];
str_sect[0]=sector;
str_sect[1]=NULL;
	
sprintf(msg,szFormatting,str_sect);
OutputDebugString(LOG_NOTICE,LOG_SYSTEM,(const char*)msg);

FLASH_HEADER hdr;
hdr.id = 0x5a5a;
hdr.headerlen = sizeof(FLASH_HEADER);
hdr.version = 2;		// FFS V2 supports spanning flash sectors 5/2006

// Sector should be all 1's now, except the fat table.
// Erase sector
flash_erase(sector);
// write FLASH_HEADER sector
int i=Flash_save(sector,0,(unsigned int*)&hdr,sizeof(FLASH_HEADER));
}

//--------------------------------------------------------------------------------
char Cffs::setsector(char sector,bool reformat)
// Changes drive sector (fs.sector) to "sector" . Returns old sector value
// If the specified sector is not formatted, then this func formats it
{
if(! isvalidsector( sector ))
	return NULL;

if(!reformat)
	{
	if( ! chkdsk(sector))
		return NULL;			
	}
else
	{
	if( !( chkdsk(sector)))
		format(sector);

	if( ! chkdsk(sector))
		return NULL;			
	}

// The specified sector exists and is formatted
// Change sector
fs.sector = sector;
fs.sector_NULL = NULL;
return fs.sector;
}

//-------------------------------------------------------------------
bool Cffs::create_bootfile(const char* filename)
// File "boot.sys" specifies the default settings filename.
// This routine changes the default boot file by:
// 1. Check the "filename" exists
// 2. Delete existing file "boot"
// 3. Create a new file "boot" containing the text name of the new settings file
{
// 1. Check the profile "filename.prf" exists
if( !ffs->open(MODE_OPENEXISTING,filename,szFILETYPE_PROFILE) )
	return false;
close(filename);

// 2. Delete existing file "boot" if it exists
del((unsigned char*)szProfileBoot,szFILETYPE_TEXT,true);

// 3. Create a new file "boot" containing the text name of the new settings file
open(MODE_CREATE,szProfileBoot,szFILETYPE_TEXT);
write((int16*)filename,strlen(filename));
close();
return true;
}

//-------------------------------------------------------------------
int Cffs::save_to_flash(int mode)
// Save system settings to currently open file
// Returns : 0 fail; 1 success
// Wrapper for void FlsWrite( unsigned long Faddr, unsigned int Data )
{
unsigned int len;
unsigned int* destptr;

OutputDebugString(LOG_NOTICE,LOG_SYSTEM,szSavingSystem);

if(mode & SAVE_RESTORE_AT)
	{
	len = sizeof(modem_config);
	destptr = (unsigned int*)&modem_config.echoflag;
	write(destptr,len);
	}

/*
if(mode & SAVE_RESTORE_MAIL)
	{
	len = sizeof(tagmail_config);
	destptr = (unsigned int*)&tagmail_config.echoflag;
	write(destptr,len);
	}
*/
	
if(mode & SAVE_RESTORE_LAN)
	{
	// Ip address , Password, Call barring
	// lan[UNSAVED].profile is only used here.
	len = sizeof(lanport);
	if(lan[UNSAVED].dhcp_server != 0)						// Do not save ip obtained by DHCP. Save 0.0.0.0 instead
		{
		lan[UNSAVED].dhcp_server = 0;
		lan[UNSAVED].dhcp_leasetime = 0;
		lan[UNSAVED].ip_addr = 0;
		}		
	destptr = (unsigned int*) &lan[UNSAVED];
	write(destptr,len);
	
	// Activate the following settings now
	// But the LAN settings are NOT activated (however they could be activated now if required)
	strcpy((char*)lan[ACTIVE].hostname,(char*)lan[UNSAVED].hostname);
	strcpy((char*)lan[ACTIVE].password,(char*)lan[UNSAVED].password);
	strcpy((char*)lan[ACTIVE].username,(char*)lan[UNSAVED].username);
	lan[ACTIVE].from_ip_addr1 = lan[UNSAVED].from_ip_addr1;
	lan[ACTIVE].to_ip_addr1 = lan[UNSAVED].to_ip_addr1;
	lan[ACTIVE].from_ip_addr2 = lan[UNSAVED].from_ip_addr2;
	lan[ACTIVE].to_ip_addr2 = lan[UNSAVED].to_ip_addr2;
	lan[ACTIVE].tcp_idletimer = lan[UNSAVED].tcp_idletimer;
	lan[ACTIVE].permitTFTP = lan[UNSAVED].permitTFTP;
	lan[ACTIVE].led_mode = lan[UNSAVED].led_mode;
	}

/*		
if(mode & SAVE_RESTORE_SYSTEM)
	{
	// DHCP pool
	len = sizeof(address_pool);
	destptr = (unsigned int*) &address_pool.first_ip_addr;
	write(destptr,len);
	}
*/
	
#ifndef SEB
// Static ARP cache entries added by the user...
unsigned int i;
for(i=system_arp_cache_entries; i < ARP_CACHELEN; i++)
	write(destptr,len);
#endif

return 1;
}

//-------------------------------------------------------------------
int Cffs::restore_from_flash(int mode)
// Restore system settings from the currently open file
// Returns : 0 fail; 1 success
{
unsigned int len;
unsigned int* destptr;

//OutputDebugString(LOG_NOTICE,LOG_SYSTEM,szRestoringSystem);

if(mode & SAVE_RESTORE_AT)
	{
	len = sizeof(modem_config);
	destptr = (unsigned int*)&modem_config.echoflag;
	read(destptr,len);
	UpdateUart(HW_DTE0);
	}

/*
if(mode & SAVE_RESTORE_MAIL)
	{
	len = sizeof(tagmail_config);
	destptr = (unsigned int*)&mail_config.echoflag;
	read(destptr,len);
	}
*/	
if(mode & SAVE_RESTORE_LAN)
	{
	// Ip address etc
	// Router config. Password, router name
	// Call barring
	len = sizeof(lanport);
	destptr = (unsigned int*) &lan[ACTIVE];			// Copied to active when ip address verified as unique		
	read(destptr,len);
	memcpy(&lan[UNSAVED],&lan[ACTIVE],sizeof(lanport));
	}

/*
if(mode & SAVE_RESTORE_SYSTEM)
	{
	// DHCP address pool
	len = sizeof(address_pool);
	destptr = (unsigned int*) &address_pool.first_ip_addr;		
	read(destptr,len);
	}
*/
	
#ifndef SEB
// Static ARP cache entries added by the user...
unsigned int i;
for(i=system_arp_cache_entries; i < ARP_CACHELEN; i++)
	read(destptr,len);
#endif
		
return 1;
}

//--------------------------------------------------------------------------
void restore_defaults()
// "Cold start" profile for data
// These parameters are used at startup if file "default.prf" is not found.
// File "defaults" is then written, containing these settings
{
// AT decoder defaults
// Init the modem_config structure
// nb Call UpdateUart to activate these settings
ATFactoryReset(HW_DTE0,PROFILE_AT);	
UpdateUart(HW_DTE0);

//OutputDebugString(LOG_NOTICE,LOG_SYSTEM,szLoadingDefaults);

// Set the hardware interface defaults
for(int IF_NUM=HW_DTE0; IF_NUM < NUM_UARTS; IF_NUM++)
	{
	// A port has a HW_CLASS and a unique IF_NUM
	if(IF_NUM < NUM_DTE_INTERFACES)
		hw->uart[IF_NUM].HW_CLASS = HW_DTE0;	// First ports are DTE
	else if(IF_NUM < NUM_TA_INTERFACES + NUM_DTE_INTERFACES)
		hw->uart[IF_NUM].HW_CLASS = HW_TA;		// Next ports are ISDN
	else if(IF_NUM < NUM_X21_INTERFACES + NUM_TA_INTERFACES + NUM_DTE_INTERFACES )
		hw->uart[IF_NUM].HW_CLASS = HW_TA;		// Next ports are X21
	else if(IF_NUM < NUM_X21_INTERFACES + NUM_TA_INTERFACES + NUM_DTE_INTERFACES + TRACE_INTERFACE)
		hw->uart[IF_NUM].HW_CLASS = HW_TRACE;	// Next port is trace		

	hw->uart[IF_NUM].pClient = NULL;			// If a software object, its "this" pointer
	hw->uart[IF_NUM].owner = OWNER_NONE;		// If < NUM_UARTS, an interface number, else an OWNER_ constant
			
	hw->uart[IF_NUM].demand = true;				// For ISDN link
	hw->uart[IF_NUM].inactivitytimer = 0;
	hw->uart[IF_NUM].maxinactivitytime = 0; 	// LINK_INACTIVITY_TIMEOUT;
	hw->uart[IF_NUM].maxestablishtime = LINK_CONNECT_TIME;

	hw->uart[IF_NUM].dialconfig=szDialConfig;
	hw->uart[IF_NUM].answerconfig=szAnswer;
	}

// LAN defaults. 
// These are copied into the active area when checking for duplicate ip address is complete.
memset(&lan[ACTIVE],0,sizeof(lanport));

strcpy((char*)lan[UNSAVED].hostname,(const char*)szRoutersName);
strcpy((char*)lan[UNSAVED].if_name,(const char*)szEthName);
lan[UNSAVED].ip_addr = DEFAULT_IP_ADDR;
lan[UNSAVED].netmask = DEFAULT_IP_NETMASK;
lan[UNSAVED].subnet = lan[UNSAVED].ip_addr & lan[UNSAVED].netmask;
lan[UNSAVED].gateway = 0;					
lan[UNSAVED].dhcp_leasetime = 0;
lan[UNSAVED].dhcp_server = 0;					// Used as a flag meaning "ip obtained by DHCP"
lan[UNSAVED].password[0]=NULL;
lan[UNSAVED].username[0]=NULL;
lan[UNSAVED].from_ip_addr1 = 0;
lan[UNSAVED].to_ip_addr1 = 0xffffffffL;
lan[UNSAVED].from_ip_addr2 = 0;
lan[UNSAVED].to_ip_addr2 = 0xffffffffL;
lan[UNSAVED].tcp_idletimer = TCP_IDLE_TIMEOUT_MSECS;	// Delete inactive TCP sessions timer
lan[UNSAVED].permitTFTP = 0;
lan[UNSAVED].idle_reset = 1;
lan[UNSAVED].led_mode=LEDS_ON;

// NB Byte order of the MAC address is reversed on the wire
mac_addr[0] = 0xffff;	
mac_addr[1] = 0xffff;
mac_addr[2] = 0xffff;

mail_config.smtp_local_port = 0;
mail_config.smtp_remote_port = 0;

dirty=false;

#ifndef SEB
// Initialise RAS address pool starting with lan address +1
for(int i=0; i < NUM_PPP_LINKS; i++)
	ip_pool[i].ip_addr = DEFAULT_IP_ADDR + i + 1;
#endif
}

//--------------------------------------------------------------------------
void ATFactoryReset(int HW_IF, int profile)
// Set AT decoder values to factory defaults
{
// Here are the defaults. If a profile number is specified, 
// the differences are applied in the following switch
modem_config.echoflag = true;
modem_config.verboseflag = true;
modem_config.quietflag = false;
modem_config.dcdflag = 1;
modem_config.dtrflag = 1;
modem_config.S0_answer_rings = 0;
modem_config.ATX = 0;
modem_config.autobaud_flag = 1;
modem_config.dtrdialnumber = 0L;
modem_config.S42_dialabort = 1;
modem_config.tcp_idletimer = TCP_MODEM_IDLE_TIMEOUT_MSECS;	// Delete inactive TCP sessions timer

#ifdef BAUD_1200
modem_config.baudrate_flag = BAUDOVERSAMPLERATE_SLOW38400;
#else
modem_config.baudrate_flag = BAUDOVERSAMPLERATE_115200;
#endif

modem_config.character_time_uS = baudrate_2uS(modem_config.baudrate_flag);
modem_config.parity_flag = FORMAT_8N;
modem_config.flowcontrol_flag = uartFlowControl_RTSCTS;
modem_config.S2_escape_char = (int)'+';		// Value of 0 disables online command mode access
modem_config.remote_port = MODEM_PORT;
modem_config.local_port = MODEM_PORT;
modem_config.shell = PROFILE_AT;
modem_config.com_enable_rfc2217 = false;
modem_config.at_cmd[0] = NULL;

switch(profile)
	{
	case PROFILE_AT:
	break;

#ifdef BAUD_1200

	case PROFILE_PAD:
	// X28 Paknet PAD emulation. 8N1 9600bps. For PRI EaseII software
	// Nb Ease2 does not want parity bit set, even if set to even parity.
	modem_config.autobaud_flag = 0;
	modem_config.baudrate_flag = BAUDOVERSAMPLERATE_SLOW9600;
	modem_config.character_time_uS = baudrate_2uS(modem_config.baudrate_flag);
	modem_config.parity_flag = FORMAT_8N;
	modem_config.shell = PROFILE_PAD;
	break;
	
	case PROFILE_DCU:
	// SPSL DCU footfall counter
	modem_config.dcdflag = 0;
	modem_config.dtrflag = 1;
	modem_config.S0_answer_rings = 1;
	modem_config.S2_escape_char = 0;
	modem_config.autobaud_flag = 0;
	modem_config.flowcontrol_flag = uartFlowControl_OFF;
	modem_config.com_enable_rfc2217 = false;
	modem_config.parity_flag = FORMAT_8N;
	modem_config.baudrate_flag = BAUDOVERSAMPLERATE_SLOW2400;
	modem_config.character_time_uS = baudrate_2uS(modem_config.baudrate_flag);
	modem_config.remote_port = 1998;
	modem_config.local_port = 1998;
	break;

	case PROFILE_PRINTER:
	// Generic printer. No responses, 8N1 19200bps
	modem_config.echoflag = false;
	modem_config.verboseflag = false;
	modem_config.quietflag = true;
	modem_config.dcdflag = 1;
	modem_config.dtrflag = 0;
	modem_config.S0_answer_rings = 1;
	modem_config.S2_escape_char = 0;
	modem_config.autobaud_flag = 0;
	modem_config.flowcontrol_flag = uartFlowControl_XONXOFF;
	modem_config.com_enable_rfc2217 = false;
	modem_config.parity_flag = FORMAT_8N;
	modem_config.baudrate_flag = BAUDOVERSAMPLERATE_SLOW19200;
	modem_config.character_time_uS = baudrate_2uS(modem_config.baudrate_flag);
	modem_config.remote_port = PRINTER_PORT;
	modem_config.local_port = PRINTER_PORT;
	break;
	
	case PROFILE_PRI:
	// PRI meter. 8N1 1200bps
	// Modified 4/7/2005 to lower idle timer value
	modem_config.echoflag = false;
	modem_config.verboseflag = true;
	modem_config.quietflag = true;
	modem_config.dcdflag = 1;
	modem_config.dtrflag = 0;
	modem_config.S0_answer_rings = 1;
	modem_config.S2_escape_char = 0;
	modem_config.autobaud_flag = 0;
	modem_config.flowcontrol_flag = uartFlowControl_OFF;
	modem_config.com_enable_rfc2217 = false;
	modem_config.parity_flag = FORMAT_8N;
	modem_config.baudrate_flag = BAUDOVERSAMPLERATE_SLOW1200;
	modem_config.character_time_uS = baudrate_2uS(modem_config.baudrate_flag);
	modem_config.tcp_idletimer = 90000;
	break;

	case PROFILE_EASE:
	// PRI EASE2 software. Modem mode (not PAD) 8N1 1200bps
	modem_config.echoflag = false;
	modem_config.verboseflag = false;
	modem_config.quietflag = false;
	modem_config.dcdflag = 1;
	modem_config.dtrflag = 2;
	modem_config.S0_answer_rings = 0;
	modem_config.S2_escape_char = 0;
	modem_config.autobaud_flag = 0;
	modem_config.flowcontrol_flag = uartFlowControl_OFF;
	modem_config.com_enable_rfc2217 = false;
	modem_config.parity_flag = FORMAT_8N;
	modem_config.baudrate_flag = BAUDOVERSAMPLERATE_SLOW9600;
	modem_config.character_time_uS = baudrate_2uS(modem_config.baudrate_flag);
	break;
	
	case PROFILE_ELSTER:
	// Elster 1700 meter. 7E1 1200bps
	modem_config.echoflag = false;
	modem_config.verboseflag = true;
	modem_config.quietflag = true;
	modem_config.ATX = 2;
	modem_config.dcdflag = 1;
	modem_config.dtrflag = 0;
	modem_config.S0_answer_rings = 1;
	modem_config.S2_escape_char = 0;		
	modem_config.autobaud_flag = 0;
	modem_config.com_enable_rfc2217 = false;
	modem_config.flowcontrol_flag = uartFlowControl_RTSCTS_ONLINE;
	// baudrate 1200
	modem_config.baudrate_flag = BAUDOVERSAMPLERATE_SLOW1200;
	modem_config.character_time_uS = baudrate_2uS(modem_config.baudrate_flag);
	// 7 Data, even parity
	modem_config.parity_flag = FORMAT_7E;
	break;

#ifdef GALAXY
	case PROFILE_GALAXY:
	modem_config.autobaud_flag = 0;
	modem_config.baudrate_flag = BAUDOVERSAMPLERATE_SLOW9600;
	modem_config.character_time_uS = baudrate_2uS(modem_config.baudrate_flag);
	modem_config.shell = PROFILE_GALAXY;
	break;
#endif

#else
	case PROFILE_PAD:
	// X28 Paknet PAD emulation. 7E1 9600bps. For PRI EaseII software
	modem_config.autobaud_flag = 0;
	modem_config.baudrate_flag = BAUDOVERSAMPLERATE_9600;
	modem_config.character_time_uS = baudrate_2uS(modem_config.baudrate_flag);
	modem_config.parity_flag = FORMAT_7E;
	modem_config.shell = PROFILE_PAD;
	break;
	
	case PROFILE_2217:
	// PRI meter. 8N1 1200bps
	modem_config.echoflag = false;
	modem_config.verboseflag = false;
	modem_config.quietflag = true;
	modem_config.dcdflag = 1;
	modem_config.dtrflag = 0;
	modem_config.S0_answer_rings = 1;
	modem_config.S2_escape_char = 0;
	modem_config.autobaud_flag = 0;
	modem_config.flowcontrol_flag = uartFlowControl_RTSCTS;
	modem_config.com_enable_rfc2217 = true;
	modem_config.parity_flag = FORMAT_8N;
	modem_config.baudrate_flag = BAUDOVERSAMPLERATE_115200;
	modem_config.character_time_uS = baudrate_2uS(modem_config.baudrate_flag);
	modem_config.remote_port = 2000;
	modem_config.local_port = 2000;
	break;

	case PROFILE_PRINTER:
	// Calumet Printer. 8N1 19200bps
	modem_config.echoflag = false;
	modem_config.verboseflag = false;
	modem_config.quietflag = true;
	modem_config.dcdflag = 1;
	modem_config.dtrflag = 0;
	modem_config.S0_answer_rings = 1;
	modem_config.S2_escape_char = 0;
	modem_config.autobaud_flag = 0;
	modem_config.flowcontrol_flag = uartFlowControl_XONXOFF;
	modem_config.com_enable_rfc2217 = false;
	modem_config.parity_flag = FORMAT_8N;
	modem_config.baudrate_flag = BAUDOVERSAMPLERATE_19200;
	modem_config.character_time_uS = baudrate_2uS(modem_config.baudrate_flag);
	modem_config.remote_port = PRINTER_PORT;
	modem_config.local_port = PRINTER_PORT;
	break;
	
	case PROFILE_ADPRO:
	// Location Specific Technologies serial camera.
	modem_config.S2_escape_char = 0;		// Value of 0 disables online command mode access
	modem_config.autobaud_flag = 0;
	modem_config.parity_flag = FORMAT_8N;
	modem_config.local_port = 3001;
	modem_config.remote_port = 7250;
	modem_config.tcp_idletimer = 20000;
	modem_config.flowcontrol_flag = uartFlowControl_RTSCTS;
	modem_config.baudrate_flag = BAUDOVERSAMPLERATE_115200;
	modem_config.character_time_uS = baudrate_2uS(modem_config.baudrate_flag);
	break;
#endif
	}
}

//--------------------------------------------------------------------------------
void Cffs::get_bootfile_name(char* pFn)
{
// Purpose:
// Return the name of the saved profile from which to load settings.
// If the file "boot.txt" exists:
//	It contains the default settings filename.
//	Check this file exists. If not, delete file "boot.sys".
// 
// If "boot.sys" or the file it specifies do not exist:
//		Use settings from file "default.prf"
//		If default does not exist, then create it
//
bool lan_config_loaded = false;

if(open(MODE_OPENEXISTING,szProfileBoot,szFILETYPE_TEXT))
	{
	// File "boot.txt" contains the default profile name if it exists
//	sprintf(msg,"\r\nFound file %s\r\n",szboot);
//	serialout((char*)msg,HW_DTE0);
	read((int16*)pFn,current_file.len);
	close();
	pFn=(char*)TrimRight((unsigned char*)pFn);
	
	// Check the specified profile file exists..
	if(open(MODE_OPENEXISTING,pFn,szFILETYPE_PROFILE))
		{
		// pFn points to the default profile name
		close();
		return;
		}
	else
		{
		// Specified profile does not exist ! error
		// Delete file Boot and fall thru to return default boot file
		sprintf(msg,szProfileNotExist,pFn);
		serialout((char*)msg,HW_DTE0);
		del((unsigned char*)szProfileBoot,szFILETYPE_TEXT,true);
		}
	}

// "boot.sys" does not exist
// Look for file "default.prf"
if(open(MODE_OPENEXISTING,szProfileDefault,szFILETYPE_PROFILE))
	{
	// "default.prf" exists
	close();
	// Will use settings from file "default.prf"
	strcpy(pFn,szProfileDefault);
	return;
	}

// File "default.prf" does not exist.
// ******* Either a cold start or default profile was deleted **********
// Create Factory and Default files
serialout(szProfileColdStart,HW_DTE0);

// Look for file "LAN"
if(open(MODE_OPENEXISTING,szLAN,szFILETYPE_SYSTEM))
	{
	// "LAN" exists. LAN settings already loaded in structure lanport
	close();
	// Recreate file "LAN" after formatting the flash
	lan_config_loaded = true;
	}

format(fs.sector);
// Default return value for "couldn't create file default"
*pFn=NULL;

// Create a new "default.prf"
if(open(MODE_CREATE,szProfileDefault,szFILETYPE_PROFILE))
	{
	// File default created ok
	save_to_flash(SAVE_RESTORE_ALL);
	close();
	// Will use settings from file "default"
	strcpy(pFn,szProfileDefault);
	}

// Regenerate the LAN file if it existed before reformatting flash
if(lan_config_loaded)
	{
	if(open(MODE_CREATE,szLAN,szFILETYPE_SYSTEM))
		{
		save_to_flash(SAVE_RESTORE_LAN);
		close();
		dirty = false;
		}
	}
					
#ifdef BAUD_1200
ATFactoryReset(HW_DTE0, PROFILE_PRI);
if(open(MODE_CREATE,szProfilePRI,szFILETYPE_PROFILE))
	{
	save_to_flash(SAVE_RESTORE_ALL);
	close();
	}	

ATFactoryReset(HW_DTE0, PROFILE_EASE);
if(open(MODE_CREATE,szProfileEASE,szFILETYPE_PROFILE))
	{
	save_to_flash(SAVE_RESTORE_ALL);
	close();
	}	

ATFactoryReset(HW_DTE0, PROFILE_ELSTER);
if(ffs->open(MODE_CREATE,szProfileElster,szFILETYPE_PROFILE))
	{
	save_to_flash(SAVE_RESTORE_ALL);
	close();
	}

ATFactoryReset(HW_DTE0, PROFILE_DCU);
if(open(MODE_CREATE,szProfileDCU,szFILETYPE_PROFILE))
	{
	save_to_flash(SAVE_RESTORE_ALL);
	close();
	}

#ifdef GALAXY
ATFactoryReset(HW_DTE0, PROFILE_GALAXY);
if(open(MODE_CREATE,szProfileGalaxy,szFILETYPE_PROFILE))
	{
	save_to_flash(SAVE_RESTORE_ALL);
	close();
	}	
#endif

#else
ATFactoryReset(HW_DTE0, PROFILE_ADPRO);
if(ffs->open(MODE_CREATE,szProfileADPRO,szFILETYPE_PROFILE))
	{
	save_to_flash(SAVE_RESTORE_ALL);
	close();
	}
#endif

// Profiles for all builds:
ATFactoryReset(HW_DTE0, PROFILE_PAD);
if(open(MODE_CREATE,szProfilePAD,szFILETYPE_PROFILE))
	{
	save_to_flash(SAVE_RESTORE_ALL);
	close();
	}

ATFactoryReset(HW_DTE0, PROFILE_2217);
if(open(MODE_CREATE,szProfile2217,szFILETYPE_PROFILE))
	{
	save_to_flash(SAVE_RESTORE_ALL);
	close();
	}

ATFactoryReset(HW_DTE0, PROFILE_PRINTER);
if(open(MODE_CREATE,szProfilePRN,szFILETYPE_PROFILE))
	{
	save_to_flash(SAVE_RESTORE_ALL);
	close();
	}

// Cold start UART settings
ATFactoryReset(HW_DTE0, PROFILE_AT);	

return;
}

// Program flash from Tectronix bin file in flash
int Cffs::tectronix_decode(int testrun)
{
	int offset;
	unsigned long address;
	int byteswritten;
	
	unsigned int byteWordState=0;
 	unsigned int fileState=0;
 	unsigned int blockState=0;
 	unsigned int operationComplete=0;
	unsigned int dataWord;
	unsigned int xpc,pc;
 	unsigned int blockSize=0;
 	unsigned int blockSizeCount=0;
 		
	byteswritten=0;
	offset=0;
  	while(offset<PACKET_DATA_SIZE)
  	{
		//
		if(operationComplete==1) 
			{
			return(0);
			}

		switch(byteWordState)
		{	
			case 0:
/*int Cffs::read(int16* data,int16 len,int mode)*/
			if(1 != read(&dataWord,1,MODE_INT16))
				// File read error
				return 1;
			byteWordState=1;
			break;
			
			case 1:
			byteWordState=0;
			//
			switch(fileState)
			{
				case 0:
				if((dataWord==0x08aa) || (dataWord==0x10aa))
				{
					fileState++;
					break;
				}
				else
				{
					return(2);//MCBSP_ERROR_HEADERERROR);	
				};
		
				case 7:
				switch(blockState)
				{
					case 0:
					blockState=1;
					blockSize=dataWord;
					blockSizeCount=0;
					if(blockSize==0) operationComplete=1;
					break;
					
					case 1:
					blockState=2;
					xpc=dataWord;
					break;
					
					case 2:
					blockState=3;
					pc=dataWord;
					break;

					case 3:
					//-------------------------
					//call flash chip routine
					// Programme or Verify   
					//-------------------------
					address=((unsigned long)xpc<<16)+(unsigned long)pc+(unsigned long)blockSizeCount;
					++blockSizeCount;
					if(blockSize==blockSizeCount) blockState=0;
					//

//					uartOutput( sprintf(uartOutputString,"FLASH_Write address %04x%04x byteswritten=%d\r\n",(int)(address>>16),(int)(address&0xffff),byteswritten) );
//					uartOutputEmpty();
					if(testrun==0)
						{
						if (FLASH_Write(address,&dataWord,1)<0) 
							{
							sprintf(msg,"FLASH_Write failed at address %04x%04x byteswritten=%d\r\n",(int)(address>>16),(int)(address&0xffff),byteswritten);
							OutputDebugString(LOG_NOTICE,LOG_SYSTEM,(const char*)msg);
							//hw->uartOutputEmpty();
							return(3);//MCBSP_ERROR_FLASHWRITE);
							}
						}

					byteswritten++;
					break;
				};
				break;
				
				default:
				fileState++;
				break;
			};
			break;
		};
		//
	};
	return(4);
}
