// ffs.h
// Flash File System routines
// Default values
extern int32 LAN_IP_ADDR;
extern int32 LAN_IP_NETMASK;

#define MODE_CREATE				0
#define MODE_OPENEXISTING		1
#define MODE_OPENORCREATE		2

// read() modes.
#define MODE_INT16				0			// Read 16 bit file into 16 bit arry
#define MODE_CHAR				1			// Read 16 bit file into 8 bit char arry (arry must be twice size of file)

#define FILENAME_LEN			8			// Max filename length
#define FILEEXTENSION_LEN		3			// Max file extension length

#define FILE_DATA_AREA_START	1000		// Data area follows FAT

// FAT table header format. Located at beginning of flash sector:
typedef struct tagFLASH_HEADER
{
int id;				// Always 0x5A5A used to verify "drive" is formatted;
int headerlen;		// FLASH_HEADER size
int version;		// FLASH_HEADER version
}FLASH_HEADER;

// FAT table entry format (1 entry per file, 20 chars per entry):
typedef struct tagFATentry
{
char filename[FILENAME_LEN+1];	// Text name with trailing NULL. If filename[0]==0xff (or 0x00) entry is empty (been deleted)
char ext[FILEEXTENSION_LEN+1];	// Text file extension with trailing NULL
int16 dataptr;					// Offest of first byte
int16 len;						// Length of this file / fragment
int16 byteswritten;				// Bytes written during file open / file modified flag
int16 fragment;					// Non zero if fragmented, contains the fragment number
int zero;
}FATentry;

// Global file system attributes
typedef struct tagFileSystem
{
int16 fileptr;						// Current file pointer position
char sector;						// Current flash sector
char sector_NULL;						// Null terminator so sector can be used as a string
}FileSystem;

void restore_defaults();

class Cffs
{
public:
void init();
bool open(int mode, const char *filename,const char *ext=NULL);
bool close(bool fs_reset = true);
int write(int16* data,int16 len,int mode=MODE_INT16);
int read(int16* data,int16 len,int mode=MODE_INT16);
bool copy(const char *dest_fn,const char *dest_ext,const char *src_fn,const char *src_ext,char dest_sector);
bool del(unsigned char* filename,const char* ext,bool delete_system_files=false);
bool delfile(unsigned char* filename_extn,bool delete_system_files=false);
int update_sofware();
int tectronix_decode(int testrun);

char setsector(char sector,bool format=false);
bool isvalid_file_extn(unsigned char* filename_extn, char **extn);

void format(char sector=0);
bool enumfiles(bool return_erased_files=true);
void ffsReset(bool discard_currentfile=true);
bool chkdsk(char sector=0);
bool erase(char sector);
bool driveclean();

void get_bootfile_name(char* pFn);
bool GetMac(int16* mac_addr);
void SetMac(int16* mac_addr);

bool create_bootfile(const char* filename);
int restore_from_flash(int mode);
int save_to_flash(int mode);

// These structures are stored in the FAT table and contain individual file details
FATentry current_file;
FATentry enumerate_file;
FileSystem fs;						// File pointer and current flash sector are saved in this structure

private:
FLASH_HEADER fls_hdr;				// Structure saved at start of flash drive indicates it is formatted
bool createnewfragment();
FATentry fragmented_file;
bool bFragmented_File;
};

