// routerstrings.H

static const char szSoftwareVersion[]=		"2.09";				// SEB sw version
static const char szSoftwareVersion2[]=		"Version 2.09";		// SEB sw version

static const char szBuildInfo[]=			"\r\nBuild information\r\nProduct:\t%s\r\nCode version:\t%s %s\r\nCPU speed:\t%s\r\nUART speeds:\t%s\r\nOption:\t\t%s %s %s %s %s";

// Strings to help display build information
#ifdef LO_POWER
static const char szCPUSpeed[]=				"Low";
#else
static const char szCPUSpeed[]=				"High";
#endif

#ifdef BAUD_1200
static const char szUARTSpeed[]=			"1200-38400";
#else
static const char szUARTSpeed[]=			"4800-115200";
#endif

#ifdef TFTP
static const char szTFTPoption[]=			"TFTP";
#else
static const char szTFTPoption[]=			"";
#endif

#ifdef DHCP
static const char szDHCPoption[]=			"DHCP";
#else
static const char szDHCPoption[]=			"";
#endif

#ifdef PAD
static const char szPADoption[]=			"PAD";
#else
static const char szPADoption[]=			"";
#endif

#ifdef SMTP
static const char szSMTPoption[]=			"SMTP";
static const char szSMTPBanner[]=			"220 SMTP Service ready\r\n";
static const char szPOP3Banner[]=			"+OK POP Service ready\r\n";

static const char szSMTPMailFrom[]=			"MAIL FROM:";
static const char szSMTPRcptTo[]=			"RCPT TO:";
static const char szSMTPreset[]=			"RSET";
static const char szSMTPhelo[]=				"HELO";
static const char szSMTPok[]=				" OK\r\n";
static const char szSMTPdata[]=				"DATA";
static const char szSMTPquit[]=				"QUIT";
static const char szSMTPnoop[]=				"NOOP";
static const char szSMTPnouser[]=			" No such user here";
static const char szSMTPmsgstartend[]=		"\r\n.\r\n";
static const char szSMTPrcptto[]=			"RCPT TO:";
static const char szSMTPmsgid[]=			"Message-ID:";
static const char szSMTPwelcome[]=			" welcome here\r\n";
static const char szSMTPsendmail[]=			" send mail, end with <cr><lf>.<cr><lf>\r\n";
static const char szSMTPclose[]=			"221 SMTP SERVICE CLOSED\r\n";
static const char szSMTP220[]=				"220";
static const char szSMTP250[]=				"250";
static const char szSMTP550[]=				"550";
static const char szSMTP251[]=				"251";
static const char szSMTP450[]=				"450";
static const char szSMTP451[]=				"451";
static const char szSMTP551[]=				"551";
static const char szSMTP552[]=				"552";
static const char szSMTP553[]=				"553";
static const char szSMTP354[]=				"354";
static const char szSMTP554[]=				"554";

#else
static const char szSMTPoption[]=			"";
#endif

#ifdef POP3
static const char szPOP3option[]=			"POP3";
#else
static const char szPOP3option[]=			"";
#endif

#ifdef RELEASE
static const char szDebugorRelease[]=		"Release";
#else
static const char szDebugorRelease[]=		"Debug";
#endif

#ifdef SEB
	#ifdef ASL
	static const char szDefaultBanner[]=	"\r\nASLH Ltd p/n ASLH305\n\r\nType help for a list of commands\r\n\r\n>";
	static const char szIndex[]=			"<b><h1>ASLH Ltd<p>LAN Modem p/n ASLH305 %s<br>\r\nSelect an item to configure</b></h1>\r\n";
	static const char szTitle[]=			"<HEAD><TITLE>Digital SP LAN Modem</TITLE></HEAD>\r";
	static const char szATI[]=		 		"\r\nASLH Ltd p/n ASLH305%s\r\n";
	static const char szRoutersName[]=		"LANmodem";
	#else
	static const char szDefaultBanner[]=	"\r\nDigital SP SEB\n\r\nType help for a list of commands\r\n\r\n>";
	static const char szIndex[]=			"<b><h1>Digital SP LAN Modem %s<p>Select an item to configure</b></h1>\r\n";
	static const char szTitle[]=			"<HEAD><TITLE>Digital SP LAN Modem</TITLE></HEAD>\r";
	static const char szATI[]=		 		"\r\nDigital SP LAN Modem Version %s\r\n";
	static const char szRoutersName[]=		"SEB";
	#endif	

#else
static const char szTitle[]=				"<HEAD><TITLE>Digital SP LAN Modem</TITLE></HEAD>\r";
static const char szDefaultBanner[]=		"\r\nDigital SP CreditGate-e\n\r\nType help for a list of commands\r\n\r\n>";
static const char szIndex[]=				"<b><h1>Digital SP CreditGate-e<p>Select an item to configure</b></h1>\r\n";
static const char szRoutersName[]=			"Router";
#endif

#ifdef TFTP
static const char szTFTP[]=					"TFTP";
static const char szTFTPDisabled[]=			"TFTP is not enabled";
static const char szTFTPread[]=				"TFTP read %s.%s mode= %s";
static const char szTFTPwrite[]=			"TFTP write %s.%s mode= %s";
static const char szTFTPFileRejected[]=		"TFTP. File %s rejected";
static const char szTFTPFileNameError[]=	"TFTP. File must be in 8.3 format";
static const char szTFTPFileExists[]=		"Duplicate filename exists on server";
static const char szTFTPReceivingUpdate[]=	"TFTP receiving software update";
static const char szTFTPReceivingBootfile[]="TFTP receiving boot file";
static const char szTFTPReceivingProfile[]=	"TFTP receiving profile %s.%s";
static const char szTFTPFileNotFound[]=		"File not found";
static const char szTFTPRequireBinary[]=	"This file must be transferred in binary mode";
static const char szTFTPMailUnsupported[]=	"TFTP mail is not supported";
static const char szTFTPDiskFull[]=			"Disk full";
static const char szTFTPnetascii[]=			"netascii";
static const char szTFTPoctet[]=			"octet";
static const char szTFTPmail[]=				"mail";
static const char szBootFile[]=				"pmboot2";
static const char szRunFile[]=				"pmrun2";
static const char szBin[]=					"bin";
#endif

static const char szShellAT[]=				"AT";
//static const char szShellSilent[]=			"silent";
static const char szShellTelnet[]=			"Telnet";
static const char szShellPAD[]=				"PAD";

// Auto IP
static const char szSEB[] =					"SEB";
static const char szAuto_ip[] =				"Auto IP";
static const char szDSP_DISCOVER[]=			"DSP DISCOVER";
static const char szDSP_SETPARAMS[]=		"DSP SETPARAMS";

// File system
static const char szDefaultSector =			'C';
static const char szBackupSector =			'D';
static const char szDirectory[]=			"\r\n\r\nDirectory of sector %s\r\n\r\nfile\t\tbytes\r\n";
static const char szDirList[]=				"%s.%s\t%u\t\r\n";
static const char szDirListEnd[]=			"\t%u file(s)\t%u bytes\r\n\t\t\t%u Bytes free";
static const char szFileNotFound[]=			"\r\nFile not found";
static const char szFileDeleted[]=			"\r\n1 file deleted";
static const char szFormatting[]=			"Formatting sector %s";
static const char szDiskErr[]=				"Disk sector %s fail";
static const char szFileOpened[]=			"opened";
static const char szFileCreated[]=			"created";
static const char szFileFragmented[]=			"fragmented";
static const char szFileOpenCreate[]=		"FILE %s:\\\\%s.%s %s ( %u bytes )";
static const char szFileSectorUnknown[]=	"\r\nUnknown sector %s";
static const char szFileSectorBad[]=		"\r\nSector %s invalid or not formatted";
static const char szProfileNotExist[]=		"\r\nSpecified boot profile =>%s<= does not exist. Using default profile\r\n";
static const char szProfileColdStart[]=		"\r\nCold start\r\n";
static const char szFFSDiskFull[]=			"FFS Disk full during write. Remain=%u";
static const char szFileBadFormat[]=		"Bad filename format %s";
static const char szFileOpenNotFound[]=		"FILE open error. %s:\\\\%s.%s not found";
static const char szFileCreateExist[]=		"FILE create error. %s:\\\\%s.%s already exists";
static const char szFileCloseNoChange[]=	"FILE %s:\\\\%s.%s closed (not changed)";
static const char szFileClose[]=			"FILE %s:\\\\%s.%s closed (%u bytes fragment %u)";		
static const char szFileCloseAppendErr[]=	"FILE %s:\\\\%s.%s append error (deleting)";
static const char szFILETYPE_DEFAULT[]=		"prf";
static const char szFILETYPE_PROFILE[]=		"prf";
static const char szFILETYPE_SYSTEM[]=		"sys";
static const char szFILETYPE_TEXT[]=		"txt";

static const char szProfileDCU[]=			"DCU";
static const char szProfilePRN[]=			"Printer";
static const char szProfilePRI[]=			"PRI";
static const char szProfileEASE[]=			"Ease";
static const char szProfileElster[]=		"ELSTER";
static const char szProfileGalaxy[]=		"Galaxy";
static const char szProfileADPRO[]=			"ADPRO";
static const char szProfilePAD[]=			"PAD";
static const char szProfile2217[]=			"Server";
static const char szProfileFactory[]=		"factory";
static const char szProfileDefault[]=		"default";
static const char szProfileBoot[]=			"boot";
static const char szSystem[]=				"system";
static const char szProfile[]=				"profile";
static const char szTxt[]=					"txt";
//
static const char szOFF[]=					"Off";
static const char szON[]=					"On";
static const char szONhex[]=				"On (hex)";
static const char szNA[]=					"N/A";
static const char szEnabled[]=				"Enabled";
static const char szDisabled[]=				"Disabled";

static const char szSIGNATURE[]=			"SIGNATURE";
static const char szSET_BAUDRATE[]=			"SET_BAUDRATE %s";
static const char szSET_DATASIZE[]=			"SET_DATASIZE %u";
static const char szSET_PARITY[]=			"SET_PARITY %u";
static const char szSET_STOPSIZE[]=			"SET_STOPSIZE %u";
static const char szSET_CONTROL[]=			"SET_CONTROL %u";
static const char szNOTIFY_LINESTATE[]=		"NOTIFY_LINESTATE %u";
static const char szNOTIFY_MODEMSTATE[]=	"NOTIFY_MODEMSTATE %u";
static const char szFLOWCONTROL_SUSPEND[]=	"FLOWCONTROL_SUSPEND";
static const char szFLOWCONTROL_RESUME[]=	"FLOWCONTROL_RESUME";
static const char szSET_LINESTATE_MASK[]=	"SET_LINESTATE_MASK 0x%x";
static const char szSET_MODEMSTATE_MASK[]=	"SET_MODEMSTATE_MASK 0x%x";
static const char szPURGE_DATA[]=			"PURGE_DATA";

static const char szFLOW_REQUEST[]=			"FLOW_REQUEST";
static const char szFLOW_NONE[]=			"FLOW_NONE";
static const char szFLOW_XON[]=				"FLOW_XON";
static const char szFLOW_RTS[]=				"FLOW_RTS";
static const char szDTR_ON[]=				"DTR_ON";
static const char szDTR_OFF[]=				"DTR_OFF";
static const char szRTS_ON[]=				"RTS_ON";
static const char szRTS_OFF[]=				"RTS_OFF";

static const char szBaudrate[]=				"Baud rate";
static const char szAutobaud[]=				"Auto";
static const char sz115200[]=				"115200";
static const char sz57600[]=				"57600";
static const char sz38400[]=				"38400";
static const char sz19200[]=				"19200";
static const char sz9600[]=					"9600";
static const char sz4800[]=					"4800";
static const char sz2400[]=					"2400";
static const char sz1200[]=					"1200";
static const char szAuto[]=					"Auto";
static const char szDTR[]=					"DTR";
static const char szdtr[]=					"dtr";
static const char szRTS[]=					"RTS";
static const char szrts[]=					"rts";
static const char szDCD[]=					"DCD";
static const char szdcd[]=					"dcd";
static const char szCTS[]=					"CTS";
static const char szcts[]=					"cts";

static const char szFlowControl[]=			"Flow Control";
static const char szNOFLOW[]=				"Off";
static const char szXONXOFF[]=				"XON/XOFF";
static const char szRTSCTS[]=				"RTS/CTS";
static const char szRTSCTS_ONLINE[]=		"CTS Online";

static const char sz8N[]=					"8 None";
static const char sz7E[]=					"7 Even";
static const char sz7O[]=					"7 Odd";
static const char sz7M[]=					"7 Mark";
static const char sz7S[]=					"7 Space";

static const char szTraceShowLevel[]=		"\r\nTrace level %s\r\n";
//static const char szTraceAll[]=				"\r\nTracing all events\r\n";
static const char szTraceIP[]=				"\r\nIP tracing %s\r\n";
static const char szTraceTCP[]=				"\r\nTCP tracing %s\r\n";
static const char szTraceUDP[]=				"\r\nUDP tracing %s\r\n";
static const char szTraceArp[]=				"\r\nARP tracing %s\r\n";
static const char szTraceSerial[]=			"\r\nSerial tracing ";
static const char szTraceHelp[]=			"\r\n\r\nTrace commands:\r\nP\tPause output\r\n1-5\tSeverity filter (5=trace all)\r\nI\tToggle IP tracing\r\nT\tToggle TCP tracing\r\nS\tToggle Serial tracing\r\nA\tToggle Arp tracing\r\nC\tClear screen\r\n\r\n";
static const char szTraceLevel[]=			"\r\nTrace level %u selected";
static const char szTraceBanner[]=			"\r\nTrace port\r\nType ? for a list of commands\r\n\r\n";
static const char szTraceBufferOverflow[]=	"\r\n*Trace data lost*\r\n";

static const char szPaused[]=				"\r\nTrace paused\r\n";
static const char szClrScr[]=				"\r\n\r\n\r\n\r\n\r\n\r\n\r\n\r\n\r\n\r\n\r\n\r\n\r\n\r\n\r\n\r\n\r\n\r\n\r\n\r\n\r\n\r\n\r\n\r\n\r\n";
static const char szFreeMem[]=				"\r\n%u Bytes free\r\n";

static const char szCmdhelp[]=				"help";
static const char szCmdset[]=				"set";
static const char szCmdshow[]=				"show";
static const char szCmdbye[]=				"bye";
static const char szCmdprompt[]=			"prompt";
static const char szCmdecho[]=				"echo";
static const char szCmdping[]=				"ping";
static const char szCmdroute[]=				"route";
static const char szCmdarp[]=				"arp";
static const char szCmdrestart[]=			"restart";
static const char szCmdtrace[]=				"trace";
static const char szCmdmodem[]=				"modem";
static const char szCmdsave[]=				"save";
static const char szCmddiscard[]=			"discard";
static const char szCmdfactory[]=			"factory";
static const char szCmdFreeMem[]=			"freemem";
static const char szCmdDir[]=				"dir";
static const char szCmdFormat[]=			"format";
static const char szCmdClean[]=				"clean";
static const char szCmdCopy[]=				"copy";
static const char szCmdType[]=				"type";
static const char szCmdDel[]=				"del";
static const char szCmdVersion[]=			"ver";

static const char szParamhelp[]=			"help";
static const char szParamconfig[]=			"config";
static const char szParammask[]=			"mask";
static const char szParamuser[]=			"user";
static const char szParampass[]=			"pass";
static const char szParamdial[]=			"dial";
static const char szParamon[]=				"on";
static const char szParamoff[]=				"off";
static const char szParamport[]=			"port";
static const char szParammodule[]=			"module";
static const char szParammodules[]=			"modules";
static const char szParamcallbar[]=			"callbar";
static const char szParamname[]=			"name";
static const char szParamprint[]=			"print";
static const char szParamadd[]=				"add";
static const char szParamdelete[]=			"delete";
static const char szParamlevel[]=			"level";
static const char szParamlan[]=				"lan";
static const char szParamppp[]=				"ppp";
static const char szParamarp[]=				"arp";
static const char szParamlcp[]=				"lcp";
static const char szParamDte[]=				"dte";
static const char szParamip[]=				"ip";
static const char szParammac[]=				"mac";
static const char szParamGateway[]=			"gateway";
static const char szParammodem[]=			"modem";
static const char szParamtcp[]=				"tcp";
static const char szParamauth[]=			"auth";
static const char szParamhtml[]=			"html";
static const char szParamsystem[]=			"system";
static const char szParamall[]=				"all";
static const char szParamout[]=				"out";
static const char szParamRemote[]=			"remote";
static const char szParamLocal[]=			"local";

#ifdef SEB
static const char szHelpSet[]=
"\r\nset username\r\n\
set password\r\n\
set ip aa.bb.cc.dd\tSet IP address aa.bb.cc.dd\r\n\
set mask aa.bb.cc.dd\tSet network mask to aa.bb.cc.dd\r\n\
set gateway aa.bb.cc.dd\tSet default gateway to aa.bb.cc.dd\r\n\
set profile\t\tSet startup profile name\r\n\
\r\nNb Type save then restart to activate set commands\r\n";

static const char szHelpShow[]=
"\r\nshow dte 0\tShow serial interface settings\r\n\
show lan\tShow LAN interface settings\r\n\
show profile\tShow startup profile name\r\n\
show callbar\tShow call barring settings\r\n\
show modules\tShow software modules\r\n\
show module n\tShow module n parameters\r\n";

#else
static const char szHelpSet[]=				"\r\nset <param> <value>\r\nchange ip configuration\r\nip | mask | gateway | user | pass| profile | dialnumber\r\n";
static const char szHelpShow[]=				"\r\nshow <param>\r\nlan | username | password | dialnumber | dte n | port local n | port remote n n\r\n";
static const char szHelpTrace[]=			"\r\ntrace\r\nconfigure debug trace\r\ntrace <out><text>|<level 0-5>|off|all|lan|ppp|arp|lcp|auth|tcp|ip|modem|system|html|dhcp|udp\r\n";
#endif

static const char szHelp[]=					"\r\nSupported commands:\r\n";
static const char szHelpEnd[]=				"\r\nType help <command> for more\r\n";
static const char szHelpTrace[]=			"\r\ntrace <0-5>\t\tSet trace level (5=all messages)\r\ntrace off|all|lan|ppp|arp|tcp|ip|modem|system|html|udp\r\ntrace out [text]\tSend text to the trace port\r\n";
static const char szHelpPrompt[]=			"\r\nprompt <new prompt text>\r\n";
static const char szHelpPing[]=				"\r\nping <ip-addr>\r\n";
static const char szHelpEcho[]=				"\r\necho <on | off>\r\n";
static const char szHelpArp[]=				"\r\narp print\t\t\tShow arp cache\r\narp delete <ip-addr>\t\tRemove cache entry\r\narp add <ip-addr><mac-addr>\tAdd cache entry\r\n";
static const char szHelpSave[]=				"\r\nsave system settings to the startup profile\r\n";
static const char szHelpRestart[]=			"\r\nsystem restart\r\n";
static const char szHelpDiscard[]=			"\r\ndiscard configuration changes\r\n";
static const char szHelpFactory[]=			"\r\nrestore factory default settings\r\n";
static const char szHelpBye[]=				"\r\nbye\tclose telnet sesion\r\n";
static const char szHelpRoute[]=			"\r\nroute <param>\r\nprint | delete<ip-addr> | add<ip-addr><netmask><gateway>\r\n";
static const char szHelpPort[]=				"\r\nset port remote|local n: configure port number for TCP modem conection\r\n";
static const char szHelpDir[]=				"\r\ndir [sector]\tDirectory of files on flash disk\r\n";
static const char szHelpType[]=				"\r\ntype filename\tDisplay file contents of file\r\n";
static const char szHelpDel[]=				"\r\ndel filename\tDelete file\r\n";
static const char szHelpFormat[]=			"\r\nformat [sector]\tFormat flash disk sector\r\n";
static const char szHelpClean[]=			"\r\nclean\t\tRemove erased files from flash disk\r\n";
static const char szHelpCopy[]=				"\r\ncopy [src file][dest file]\tCopy file\r\n";
static const char szHelpFreeMem[]=			"\r\nfreemem\tShow free memory\r\n";
static const char szHelpVersion[]=			"\r\nver\tShow software version information\r\n";

static const char szRouterSysmem[]=			"Sysmem address: first=0x%04x last=0x%04x \r\n\r\n";
static const char szFactorySettings[]=		"\r\n** FACTORY SETTINGS INSTALLED **\r\n";
static const char szEnterMAC[]=				"\r\nPlease configure and save mac address\r\n\r\n";
static const char szEnterIP[]=				"\r\nPlease configure and save ip address\r\n\r\n";

static const char szSavingSystem[]=			"Saving system configuration";
static const char szConfigChanged[]=		"<b><h1>Configuration changes have not been saved</b></h1>";
static const char szDeletingProtocol[]=		"Deleting %s";
static const char szDeletingClient[]=		" and client %s";

static const char szConnectSpeedIP[]=		"TCP/IP";
static const char szConnectSpeed10M[]=		"10000000\r\n";
static const char szABORTED[]=				"\r\nABORTED\r\n";
static const char szNO_DIALTONE[]=			"\r\nNO DIALTONE\r\n";
static const char szNO_CARRIER[]=			"\r\nNO CARRIER\r\n";
static const char szOK[]=					"\r\nOK\r\n";
static const char szERROR[]=				"\r\nERROR\r\n";
static const char szCONNECT[]=				"\r\nCONNECT%s";
static const char szRING[]=					"\r\nRING\r\n";
static const char sz0[]=					"\r\n0\r\n";
static const char sz1[]=					"\r\n1\r\n";
static const char sz2[]=					"\r\n2\r\n";
static const char sz3[]=					"\r\n3\r\n";
static const char sz4[]=					"\r\n4\r\n";
static const char sz6[]=					"\r\n6\r\n";
static const char szSTAR_C1[]=				"\r\n\r\n!I\tIP addr\t\t\t%a\r\n!N\tNetmask\t\t\t%a\r\n!G\tGateway\t\t\t%a\r\n\tSubnet\t\t\t%a\r\n\tMac addr\t\t%m\r\n\r\n!L\tLocal TCP port\t\t%u\r\n!P\tRemote TCP port\t\t%u\r\n\tIdle disconnect time\t%lu (secs)\r\n";
static const char szSTAR_C3[]=				"\r\n%s\r\nFrames sent\t\t%u\r\nFrames received\t\t%u\r\nFrames discarded\t%u\r\nReceiver overruns\t%u\r\n";
static const char szSTAR_C[]=				"\r\nCHANNEL\t%s\tSTATUS\t%s\r\n\r\nV  AT CMD RESPONSES\t%u\t&C DCD OPT\t%u\r\n&D DTR OPT\t\t%u\tQ  QUIET\t%u\r\nS0 RINGS TO ANSWER\t%u\t&B AUTOBAUD\t%s\r\nF  BITS/PARITY\t\t%s\t&K FLOW CONTROL\t%s\r\nE  CMD ECHO\t\t%u\r\n";
static const char szModemDialing[]=			"MODEM dialing ip %0a port %u";
static const char szModemBadNumber[]=		"MODEM bad dial number =>%s<=";
static const char szModemDTR[]=				"MODEM DTR changed to %s";
static const char szModemRTS[]=				"MODEM RTS changed to %s";
static const char szModem_rfc2217[]=		"COM port control negotiated (RFC-2217)";
static const char szRemoteSends2217[]=		"Remote sends rfc2217";
static const char szRemoteAccpts2217[]=		"Remote accepts rfc2217";
static const char szModem2217unknown[]=		"Unknown rfc2217 option";
static const char szModemLocalAccess[]=		"MODEM Local access enabled";
static const char szModemAutobaud[]=		"SET_BAUDRATE AUTO (460800)";
static const char szModemBaudRequest[]=		"REQ_BAUDRATE %s";

static const char szNull[]=			 		"\0";
static const char szDialing[]=				"Dialing";
static const char szAnswering[]=			"Answering";
static const char szOnline[]=				"Online";
static const char szOffline[]=				"Offline";
static const char szOnlineCommands[]=		"Online";
static const char szLinestateError[]=		"Error";

static const char szCmdPortnoTA[]=			"\r\nNo resource available\r\n";

static const char szDHCP[]=					"DHCP";
static const char szDHCPClientRequest[]=	"Using DHCP to discover ip address";
static const char szDHCPfail[]=				"DHCP fatal error - no server responding";
static const char szDHCPReleaseOffered[]=	"DHCP releasing offered ip=%0a";
static const char szDHCPleasetimeout[]=		"DHCP lease timeout ip=%0a";
static const char szDHCPdiscover[]=			"DHCP received DISCOVER requesting ip=%0a offering %0a";
static const char szDHCPrequest[]=			"DHCP received REQUEST requesting ip=%0a offering %0a";
static const char szDHCPdecline[]=			"DHCP received DECLINE from mac=%m";
static const char szDHCPrelease[]=			"DHCP received RELEASE from mac %m";
static const char szDHCPnopool[]=			"DHCP received DISCOVER no ip address to allocate (requested ip %0a)";
static const char szDHCPoffer[]=			"DHCP received offer ip %0a from server %0a";
static const char szDHCPaccept[]=			"DHCP accepted ip %0a from server %0a";
static const char szDHCPnetmask[]=			"DHCP netmask\t%0a";
static const char szDHCPgateway[]=			"DHCP gateway\t%0a";
static const char szDHCPlease_time[]=		"DHCP lease time\t%lu secs";
static const char szDHCPserver_ip[]=		"DHCP server ip\t%0a";
static const char szDHCPxid_nomatch[]=		"DHCP rcvd xid %lu expected %lu";
static const char szDHCPexpire[]=			"DHCP **Halting network interface** client lease expired";
static const char szDHCPrenewing[]=			"DHCP client renewing lease";
static const char szDHCPrenewingnoresp[]=	"DHCP client no response from server while renewing lease";
static const char szDHCPrequestingnoresp[]=	"DHCP client no response from server while requesting lease";
static const char szDHCPservernoresp[]=		"DHCP client unable to locate a server";
static const char szDHCPfirstIP[]=			"First pool address";
static const char szDHCPlastIP[]=			"Last pool address";
static const char szDHCPleasetime[]=		"Lease period in seconds";
static const char szDHCPenable[]=			"Enable DHCP server";
static const char szDHCPack[]=				"DHCP received ACK";
static const char szIpNotAssigned[]=		"IP address not assigned\r\n";

static const char convert_upper[] = 		{'0','1','2','3','4','5','6','7','8','9','A','B','C','D','E','F'};
static const char convert_lower[] = 		{'0','1','2','3','4','5','6','7','8','9','a','b','c','d','e','f'};
static const char szPingData[] = 			"abcdefghijklmnopqrstuvwabcdefghi";
static const char szRcvdNonLCP[]=			"get_input Received non-LCP packet when LCP not open.protocol = %X";
static const char szUnsupportedProtocol[]=	"get_input[]= Unsupported protocol(%04X).";
static const char szUnrecognisedProtocol[]=	"Unrecognised protocol 0x%04X";
static const char szIPselectedInterface[]=	"IP route selected interface %u";
static const char szRasClientIP[]=			"RAS client %0a";
static const char szIPFoundAddress[]=		"Found ip address %0a in routing table entry %u";
static const char szIPCallBar[]=			"Access barred";

static const char szPortDisplay[]=			"\r\nport\tstate\tip\t\tuser\tdial\r\n\r\n";
static const char szLANPortsDisplay[]=		"%s\t\t%0a\r\n";
static const char szISDNPortsDisplay[]=		"%s\t%s\t%0a\t\t%s\t%s\r\n";
static const char szTCPObjectDisplay[]=		"\r\n%s client %s\r\nRTT=%lu\r\nSRTT=%lu\r\nRTO=%lu\r\nBackoff=%lu\r\nBuffers=%u Retransmissions=%u\r\n";
static const char szUDPObjectDisplay[]=		"\r\n%s client %s\r\n";
static const char szObjectDisplay[]=		"\r\n%s client %s\r\n";
static const char szUartDisplay[]=			"\r\nUart\tDTE%u (owner %s)\r\nBaud rate\t%s bps\r\nInterface\t%s %s %s %s\r\nFlow control\t%s\r\nData format\t%s\r\nFraming errs\t%u\r\nInput queue\t%u\r\nOutput queue\t%u\r\n";
static const char szCallbarDisplay[]=		"Call barring permits access from devices in two ranges:\r\nRange 1 from ip %0a to ip %0a\r\nRange 2 from ip %0a to ip %0a\r\n\r\n";
static const char szFreememDisplay[]=		"\r\n\r\n%u bytes of memory free\r\n";
static const char szNoObject[]=				"\r\nNo object #%u exists\r\n";

static const char szRouteDisplay[]=			"\r\nip addr\t\tmask\t\tgateway\t\tinterface\r\n\r\n";
static const char szRTableDisplay[]=		"%0a\t%0a\t%0a\t\t%s\r\n";

static const char szConnectDisplay[]=		"%u Prot %s(%u) client %s. State %s/%s. Idle for %lus\r\n";
static const char szNone[]=					"none";

static const char szPinging[]=				"\r\n\r\nPinging %0a with 32 bytes of data\r\n\r\n";

static const char szEthName[]=				"en0";
static const char szEthNotConnected[]=		"Ethernet cable not connected";
static const char szEthNotConnected2[]=		"Connect cable and restart\r\n";

static const char szPPP_ShortFrame[]=		"PPP received short frame %u bytes on interface %s";
static const char szPPP_LongFrame[]=		"PPP received long frame %u bytes on interface %s";
static const char szPPP_Rcv[]=				"PPP received %u bytes on interface %s ";
static const char szPPP_Opening[]=			"PPP opening PPP%u on %s";
static const char szPPP_Closing[]=			"PPP closing PPP%u on %s";
static const char szPPP_tx[]=				"PPP sending %u bytes on interface %s";
static const char szPPP_rx[]=				"PPP received %u bytes";
static const char szPPP_np_pass[]=			"PPP setting PPP interface %u to pass ip";
static const char szPPP_np_down[]=			"PPP setting PPP interface %u down";
static const char szPPP_np_mode[]=			"PPP setting PPP interface %u protocol %X to mode %s";
static const char szPPP_buffer[]=			"Serial Rx BUFFER space. Need %u avail %u";
static const char szBadNPMODE[]=			"PPP discard frame - bad NPMODE";
static const char szQueueDiscard[]=			"PPP queue/discard frame";
static const char szNPmodeError[]=			"PPP NPmode error!";	
static const char szQUEUE[]=				"PPP QUEUE";
static const char szDISCARD[]=				"PPP DISCARD";
static const char szPASS[]=					"PPP PASS";
static const char szNONE[]=					"PPP mode NONE";

static const char szARPtxrequest[]=			"ARP request to %0a";
static const char szARPtxreply[]=			"ARP reply to %0a";
static const char szARPrxrequest[]=			"ARP request from %0a";
static const char szARPrxreply[]=			"ARP reply from %0a";
static const char szARPnotInCache[]=		"ARP %0a not in cache";
static const char szARPtimeout[]=			"ARP timeout removing cache entry %0a";
static const char szARPstale[]=				"ARP %0a stale";
static const char szARPnotResponding[]=		"ARP host %0a not responding";
static const char szARPremove[]=			"ARP removing cache entry %0a";
static const char szARPadd[]=				"ARP adding cache entry %0a mac %m";
static const char szARPfound[]=				"ARP found %0a in cache";
static const char szARPUsingGateway[]=		"ARP Using gateway for %0a";
static const char szARPUndeliverable[]=		"ARP No route to %0a";
static const char szARPCacheDisplay[]=		"\r\nInternet address\tPhysical address\tType\tAge(s)\r\n\r\n";
static const char szARPDisplay[]=			"%0a\t\t%m\t%s\t%lu %s\r\n";
static const char szARPcachefull[]=			"ARP cache full adding %0a";
static const char szARPDynamic[]=			"dynamic";
static const char szARPStatic[]=			"static";
static const char szARPStale[]=				"stale";

static const char szHWrelease[]=			"Releasing hardware %s from interface ";
static const char szHWselected[]=			" selected interface %s";
static const char szHWavailable[]=			"Hardware interface %s available";
static const char szHWunavailable[]=		"No hardware interface available";

static const char szLANtx[]=				"LAN sending %u bytes protocol=0x%04X%04X to ip %0a mac 0x%04X%04X%04X";
static const char szLANMovingMemory[]=		"LAN *MOVING MEMORY*";
static const char szLANDisconnected[]=		"\r\nLAN not connected\r\n";
static const char szLANConnected[]=			"\r\nLAN is connected\r\n";
static const char szLANRingOverflow[]=		"LAN Ring overflow!!\r\n";
static const char szLANdiscard[]=			"LAN discard. ISR=0x%X";
static const char szLanIdleReInit[]=		"Reinitialising idle LAN";
static const char szLanIdleReBoot[]=		"Rebooting - idle LAN\r\n";
static const char szLanIdleReBootAsk[]=		"Reboot on idle LAN";
static const char szNotCreated[]=			"IP No resources available to accept client (protocol %u)";
static const char szConnectLimit[]=			"IP Connection limit reached - client %0a rejected (protocol %u)";
static const char szERRCreating[]=			"IP ERROR creating L3 ICMP protocol - instance already exists";
static const char szIP_info1[]=				"IP route source=%0a dest=%0a";
static const char szIP_sendingPPP[]=		"IP routing to PPP if %u using %s";
static const char szRouteAdded[]=			"IP route #%u added. Dest ip= %0a";
static const char szCreatedProtocol[]=		"IP created %s protocol";
static const char szDiscardingFrame[]=		"IP discarding frame. Unsupported IP Version %u. source=%0a";
static const char szDiscardingFrame_len[]=	"IP discarding frame. Bad length. source=%0a dest=%0a";
static const char szDiscardingFrame_TTL[]=	"IP discarding frame. TTL expired. source=%0a dest=%0a ";
static const char szDiscardingFrame_destn[]="IP Discarding frame. Network unreachable. source=%0a dest=%0a";
static const char szRouteDeleted[]=			"%u Route(s) deleted";

static const char szLoadingDefaults[]=		"Loading default profile";
static const char szDiscardingProtocol[]=	"get_input discarding protocol";
static const char szCarrierLost[]=			"Carrier lost";
static const char szIncomingCall[]=			"Incoming call";
static const char szCallAnswered[]=			"Call answered";
static const char szNoMem[]=				"Out of memory in timeout!";
static const char szLowMem[]=				"Memory low (%u bytes)";
static const char szError[]=				"Error";
static const char szError2[]=				"Error %u";
static const char szInitRtable[]=			"Initialising routing table";
static const char szInitPPP[]=				"Initialising PPP";
static const char szGatewayNotFound[]=		"Default gateway %0a not responding\r\n";
static const char szDuplicate_IP[]=			"A duplicate IP address exists on the LAN!\r\n";

static const char szPromptDefault[]=		"\r\n>";
static const char szPromptUnsaved[]=		"\r\nunsaved>";
static const char szUsernamePrompt[]=		"Username>";
static const char szPasswordPrompt[]=		"Password>";
static const char szDialCmd[]=				"ATD";
static const char szBye[]=					"\r\nbye\r\n";

#ifdef PAD
// X28 PAD commands
static const char szPADcmdSET[]=			"SET";
static const char szPADcmdVER[]=			"VER";
static const char szPADcmdLANSETTINGS[]=	"LAN";
static const char szPADcmdARPCACHE[]=		"ARP";
static const char szPADcmdHelp1[]=			"HELP";
static const char szPADcmdHelp2[]=			"?";
static const char szPADcmdCALL[]=			"CALL";
static const char szPADcmdCLR[]=			"CLR";
static const char szPADcmdCON[]=			"CON";
static const char szPADcmdPROF[]=			"PROF";
static const char szPADcmdRESET[]=			"RESET";
static const char szPADcmdSTAT[]=			"STAT";
static const char szPADcmdPORT[]=			"PORT";
static const char szPADcmdDTE[]=			"DTE";
static const char szPADcmdrestart[]=		"ATY";
static const char szPADcmdPAR[]=			"PAR";
static const char szPADCmdFreemem[]=		"FREEMEM";
static const char szPADClrConf[]=			"\r\nCLR CONF\r\n";
static const char szPADClrDte[]=			"\r\nCLR DTE 000\r\n";
static const char szPADCom[]=				"\r\nCOM\r\n";
static const char szPADStat[]=				"\r\n%s\r\n";
static const char szPADError[]=				"\r\nERR";
static const char szPADprompt[]=			"\r\n*";
static const char szPADHelp[]=				"\r\nPAD comands\r\nset <option:value>\tcall <ip-address>\r\nhelp\t\t\tpar\r\nclr\t\t\tstat\r\n";


// X29 PAD commands
static const char szPADcmdRSET[]=			"RSET";
static const char szPADcmdRPAR[]=			"RPAR";
static const char szPAD18is[]=				"%i,%i,%i,%i,%i,%i,%i,%i,%i,%i,%i,%i,%i,%i,%i,%i,%i,%i";
static const char szPAR18is[]=				"\r\nPAR 1:%i 2:%i 3:%i 4:%i 5:%i 6:%i 7:%i 8:%i 9:%i 10:%i 11:%i 12:%i 13:%i 14:%i 15:%i 16:%i 17:%i 18:%i";
#endif // PAD

#ifdef WIN32
static const char szDialConfig[]=			"AT\\Q0\\N1&C1!D0!UM=3\x0d\x0a"	; //v120 RAW mode
//static const char szDialConfig[]=		 	"AT\\Q0\\N4&C1!D0!UM=3&d0&W\x0d\x0a" ; PPP mode
#else
// Ignore DTR
static const char szRouterConfig[]=			"<b><h1>Router configuration</b></h1>\r\n";
static const char szDialConfig[]=			"AT\\Q0\\N1&C1!D0!UM=3&d0\r\n"	; //v120 RAW mode
//static const char szDialConfig[]=		 	"AT\\Q0\\N4&C1!D0!UM=3&d0&W\x0d\x0a"	; PPP mode
#endif

static const char szSaveSetting[]=			"\r\nSettings will not take effect until saved";
static const char szAnswer[]=				"ATA\r\n";
static const char szEndline[]=				"\r\n";
static const char szTab[]=					"\t";
static const char sz2Tabs[]=				"\t\t";
static const char szCmdLen[]=				"\r\ncommand too long (%u chars)\r\n";
static const char szBadCommand[]=			"\r\nbad command %s\r\n";
static const char szUnsupportedParam[]=		"\r\nbad parameter\r\n";
static const char szMACerr[]=				"\r\nMAC address is not user configurable\r\n";

static const char szAdded[]=				"\r\nadded\r\n";
static const char szNotAdded[]=				"\r\nerror not added\r\n";
static const char szDeleted[]=				"\r\ndeleted\r\n";
static const char szNotDeleted[]=			"\r\nentry not found\r\n";

static const char szIcmp_send[]=			"ICMP sending frame type %d code %d";
static const char szIcmp_request[]=			"ICMP echo request from %0a";
static const char szIcmp_reply_id[]=		"ICMP reply id %u sequence 0x%x";
static const char szIcmp_reply[]=			"Reply from %0a: time=%lums\r\n";
static const char szIcmp_timeout[]=			"Request timed out\r\n";

// Longest protocol name = 9 chars !!!
static const char szIPCP[]=					"IPCP";
static const char szIP[]=					"IP";
static const char szLCP[]=					"LCP";
static const char szTCP[]=					"TCP";
static const char szUDP[]=					"UDP";
static const char szCommandPort[]=			"Console";
static const char szTelnet[]=				"Telnet";
static const char szSMTP[]=					"SMTP";
static const char szPOP3[]=					"POP3";
static const char szTrace[]=				"Trace";
static const char szHTTP[]=					"HTTP";
static const char szICMP[]=					"ICMP";
static const char szPAP[]=					"PAP";
static const char szCHAP[]=					"CHAP";
static const char szTCPModem[]=				"MODEM";
static const char szPAD[]=					"X28 PAD";
static const char szGalaxy[]=				"Galaxy";

static const char szTx_UDP[]=				"UDP Tx %u bytes to %0a";
static const char szUDPCreatedClient[]= 	"UDP created client %s (port %u)";
static const char szUDPFailCreateClient[]=	"UDP failed to create client (port %u)";
static const char szUDPfcserr[]=			"UDP FCS error src ip=%a our ip=%a . Received<0x%04X> Calculated<0x%04X>\r\n";
static const char szUDPbadframesize[]=		"UDP rcvd bad frame size";
static const char szUDPframetoobig[]=		"UDP frame too large";

static const char szTCPClientTimeout[]=		"\r\nInactive session - disconnecting\r\n";
static const char szTCPNoMemSerial[]=		"TCP insufficient serial buffer";
static const char szTCPNoMemClient[]=		"TCP Insufficient memory to create client port %u (%u bytes free)";
static const char szTCPNoMem[]=				"TCP Insufficient memory for retransmission buffer (%u bytes free)";
static const char szTCPmss[]=				"TCP MSS %u Window %u";
static const char szTCPrxlimit[]=			"TCP Ignoring overlength frame.  Receive window=%u bytes";
static const char szTCPresetHalfOpen[]=		"TCP Sent reset - half open connection";
static const char szTCPresetWindow[]=		"TCP Sent reset - frame out of window";
static const char szTCPIgnoringFrame[]=		"TCP rejecting frame in state %s";
static const char szTCPCreatedClient[]= 	"TCP created client %s (port %u)";
static const char szTCPFailCreateClient[]=	"TCP failed to create client (port %u)";
static const char szTCPNoBuffer[]=			"TCP No memory for received data (%u bytes received %u bytes free)";
static const char szTCPremote_sequence[]=	"TCP sequence error. Received %lu expected %lu";
static const char szTCPDelayedACK[]=		"TCP delaying ACK for %lums (%uuS / char)";
static const char sztcp_RSTabort[]=			"TCP RST received";
static const char sztcp_retransmit[]=		"TCP resend #%u %u bytes (MSS=%u seq=%lu ack=%lu)";
static const char sztcp_retransmit_limit[]= "TCP resend limit reached. Closing TCP";
static const char szTCPremote_resend[]=		"TCP remote resent %u bytes. (ack=%lu seq=%lu)";
static const char szTCPremote_resend2[]=	"TCP remote resent %lu bytes plus %u new bytes";
static const char szTCPdiscard[]=			"TCP discarded trace data";
static const char szTCPtruncate[]=			"TCP truncating data. Len=%u remote mss=%u";
static const char szTCPrx_keepalive[]=		"TCP received keepalive";
static const char szTCPtx_keepalive[]=		"TCP sent keepalive";
static const char sztcp_FIN[]=				"FIN ";
static const char sztcp_SYN[]=				"SYN ";
static const char sztcp_ACK[]=				"ACK ";
static const char sztcp_RST[]=				"RST ";
static const char sztcp_PSH[]=				"PSH ";
static const char szTx_TCP[]=				"TCP Tx %u bytes seq=%lu ack=%lu flags=";
static const char szRx_TCP[]=				"TCP Rx %u bytes seq=%lu ack=%lu flags=";
static const char szTCPFCSerr[]=			"TCP FCS error. Received<0x%04X> Calculated<0x%04X> (%u bytes)";
static const char szTCPnot_established[]=	"TCP error. Unable to send in state %s";
static const char szTCPbuffering[]=			"TCP error. Buffering %u bytes. Total saved=%u";
static const char szTCP_SYN_SENT[]=			"SYN SENT";
static const char szTCP_LISTEN[]=			"LISTEN";
static const char szTCP_SYN_RECEIVED[]=		"SYN RECEIVED";
static const char szTCP_DELAYED_ACK[]=		"DELAYED ACK";
static const char szTCP_ESTABLISHED[]=		"ESTABLISHED";
static const char szTCP_CLOSE_WAIT[]=		"CLOSE WAIT";
static const char szTCP_TIME_WAIT[]=		"TIME WAIT";
static const char szTCP_CLOSING_TCP[]=		"CLOSING";
static const char szTCP_LAST_ACK[]=			"LAST ACK";
static const char szTCP_FIN_WAIT1[]=		"FIN WAIT 1";
static const char szTCP_FIN_WAIT2[]=		"FIN WAIT 2";
static const char szTCPstateError[]=		"Error";
static const char szTCPseqerror[]=			"**TCP ERROR!**";
static const char szTCPframeerr[]=			"TCP error. Frame (%u bytes) exceeds remote window (%u bytes)";
static const char szTCPLocalPort[]=			"TCP/IP IN port number";
static const char szTCPremotePort[]=		"TCP/IP OUT port number";
static const char szTCPidletimer[]=			"Idle disconnect secs";
static const char szTCPidletimer2[]=		"TCP idle disconnect secs";
static const char szHTMLrfc2217[]=			"RFC2217 DTE port control";
static const char szATDroplist[]=			"Command interface";
static const char szDataFormat[]=			"Data bits/parity";
static const char szATDefaults[]=			"Default AT commands";

static const char szOwnerError[]=			"Owner error %u";
static const char szOwner_None[]=			"None";
static const char szOwner_DTE[]=			"DTE Error";
static const char szOwner_TA[]=				"TA Error";
static const char szOwner_TCP[]=			"TCP";
static const char szOwner_Console[]=		"Console";
static const char szOwner_UDP[]=			"UDP";
static const char szOwner_PPP[]=			"PPP";

static const char szTx_IP[]=				"IP Tx %u bytes dest %0a src %0a";
static const char szRx_IP[]=				"IP Rx %u bytes src %0a dest %0a";
static const char szTx_IP_no_HW_IF[]=		"IP no route to %0a";

static const char szClassError[]=			"HW_CLASS error %u";
static const char szPhaseError[]=			"phase error %u";
static const char szDTE_percentd[]=			"DTE%u";
static const char szTA_percentd[]=			"TA%u";
static const char szStdout[]=				"TRACE";
static const char szX21[]=					"X21";
static const char szLAN[]=					"LAN";

static const char szRouterName[]=			"Device name";
static const char szUsername[]=				"User name";
static const char szPassword[]=				"Password";
static const char szIFname[]=				"Interface name";
static const char szDialNumber[]=			"Dial number";
static const char szIdleDisconnectSeconds[]= "Idle disconnect secs";
static const char szEncryptPassword[]=		"Use encrypted password";
static const char szAdd[]=					"Add";
static const char szInsert[]=				"Insert";
static const char szDelete[]=				"Delete";
static const char szSave[]=					"Save";
static const char szLoad[]=					"Load";
static const char szNo[]=					"No.";
static const char szDemand[]=				"Dial on demand";

static const char szip_addr[]=				"IP Address";
static const char sznetmask[]=				"Subnet mask";
static const char szgateway[]=				"Gateway";
static const char szsubnet[]=				"Subnet";
static const char szConfirmPassword[]=		"Confirm password";
static const char szInterface[]=			"Interface";
static const char szAllowRAS[]=				"Allow dial-in clients";
static const char szPermitLANAccess[]=		"Permit access to LAN";
static const char szPermitTFTP[]=			"Enable TFTP server";
static const char szPermitFromIP[]=			"Permit access from ip";
static const char szPermitToIP[]=			"To ip";

static const char szSaveSelectedProfile[]=	"Save settings to this profile ";
static const char szMakeStartupProfile[]=	"Make this the startup profile";
static const char szSaveToNewProfile[]=		"Save DTE port settings to a new profile";
static const char szSaveToNewProfile2[]=	"Save settings to new profile";
static const char szProfileName[]=			"<enter profile name>";
static const char szStartupProfile[]=		"DTE port startup profile";
static const char szLANSettingChanged[]=	"<tr><td>\r\nThe system settings have changed (common to all profiles)</tr></td>";
static const char szDeleteThisProfile[]=	" Delete this profile                  ";
static const char szSaveLanSettings[]=		"   Save system settings      ";
static const char szDiscardLanSettings[]=	"  Discard system settings  ";

//HTML
static const char szUnBody[]=				"</body>";
static const char szUnForm[]=				"</form>";
static const char szP[]=					"<p>";
static const char szUnP[]=					"</p>\r";
static const char szUnHTML[]=				"</HTML>";
static const char szHTML[]=					"<HTML>";
static const char szOption[]=				"<option>";
static const char szOptionValue[]=			"<option value=\"%u\">";
static const char szOptionSelected[]=		"<option selected>";
static const char szOptionValueSelected[]=	"<option value=\"%u\" selected>";
static const char szUnOption[]=				"</option>";
static const char szUnOptionCr[]=			"</option>\r";
static const char szTextArea[]=				"<textarea>";
static const char szUnTextArea[]=			"</textarea>";
static const char szUnSelect[]=				"</select>";

static const char szhtml_NoOption[]=		"<option>None defined</option>";
static const char szChecked[]=				" checked";
static const char szcrlf_percent_s[]=		"\r\n%s\r\n";
static const char szpercent_u[]=			"%u";
static const char szpercent_u_crlf[]=		"\r\n%u\r\n";
static const char szpercent_u_OKcrlf[]=		"\r\n%u\r\nOK\r\n";
static const char szpercent_lu[]=			"%lu";

static const char szpercent_twox[]=			"%02X-";
static const char szpercentfour_brackets[]=	"<%04X>";
static const char szMac[]=					"\r\n%m\r\n";
static const char szInterfaceList[]=		"interface\tpassword\tdial\tdemand\tidle time\r\n";
static const char szCancel[]=				"Cancel";
static const char szOKButton[]=				"    OK    ";


static const char szBUTTON_PRESSED[]=	 	"HTTP Button %u pressed";
static const char szBadPassword[]=		 	"<b><h1>Passwords do not match</b></h1>";
static const char szWrongPassword[]=	 	"<b><h1>Incorrect username or password entered</b></h1>";
static const char szhtmlRouteNotAdded[]= 	"<b><h1>Error! Route not added</b></h1>";
static const char szhtmlRouteAdded[]=	 	"<b><h1>Route added</b></h1>";
static const char szhtmlRouteDeleted[]= 	"<b><h1>Route deleted</b></h1>";
static const char szInvalidForm[]=			"Invalid form field entry\r\n";
static const char szBUTTON[]=				"BUTTON";
static const char szPingBytes[]=			"[]= bytes=%u";
static const char szPingTime[]=				" time=%ums";
static const char szBody[]=					"<body bgcolor=\"#00FFFF\">";
static const char szForm[]=					"<form method=\"POST\">";
static const char szDropListStart[]=		"<tr><td>%s</td><td><select size=\"1\" name=\"T%u\">\r";
static const char szTableRowEnd[]=			"</td></tr>\r";
static const char szColumn[]=				"<td>";
static const char szColumnEmpty[]=			"<td></td>";
static const char szRow[]=					"<tr>";
static const char szRowEnd[]=				"</tr>";
static const char szRowEndCr[]=				"</tr>\r";

static const char szTable[]=				"<table>\r";
static const char szhtml_table[]=			"";
static const char szhtml_table_bool[]=		"<tr><td>%s</td><td><input type=\"checkbox\" name=\"T%u\"%s></td></tr>\r";
static const char szhtml_table_passwd[]=	"<tr><td>%s</td><td><input type=\"password\" name=\"T%u\" size=\"20\" value=\"%s\"></td></tr>\r";
static const char szhtml_table_string[]=	"<tr><td>%s</td><td><input type=\"text\" name=\"T%u\" size=\"20\" value=\"%s\"></td></tr>\r";
static const char szUnTable[]=				"</table>";

static const char szHTTPButton[]=			"<input type=\"submit\" value=\"%s\" name=\"%s%u\">";
static const char szHTTPField[]=			"T%u=";
static const char szHTTPFormParamErr[]=		"HTTP Form %s error";
static const char szHTTPFormBool[]=			"HTTP Form %s BOOL true";
static const char szHTTPFormIP[]=			"HTTP Form %s IP %s";
static const char szHTTPFormErrIP[]=		"HTTP Form %s IP failed. Data=>%s<=";
static const char szHTTPFormINT[]=			"HTTP Form %s INT %lu";
static const char szHTTPFormErrINT[]=		"HTTP Form %s INT failed. Data=>%s<=";
static const char szHTTPFormErrBool[]=		"HTTP Form %s BOOL false";
static const char szHTTPFormErrString[]=	"HTTP Form %s not found";
static const char szHTTPFormString[]=		"HTTP Form %s STRING =>%s<=";
static const char szHTTPFieldLength[]=		"HTTP form %s field exceeds maximum length (%u chars)";
static const char szHTTPMethodPost[]=		"POST";
static const char szHTTPMethodGet[]=		"GET";
static const char szHTTPMethodHead[]=		"HEAD";
static const char szHTTPMethodPut[]=		"PUT";
static const char szHTTPMethodDelete[]=		"DELETE";

static const char szSpace[]=			 	" ";

static const char szLANinterface[]=			"             LAN                ";
static const char szDemandInterfaces[]=		"     WAN interface   ";
static const char szRoutingTable[]=			"     Routing table     ";
static const char szDUN[]=					"Dial-up networking";
static const char szSecurityButton[]=		"           Security           ";
static const char szLoadSaveSettings[]=		"Load and Save settings  ";
static const char szCallBarButton[]=		"    Call screening     ";
static const char szDHCPButton[]=			"     DHCP server      ";
static const char szSerialButton[]=			"           DTE port         ";

static const char szSerialConfig[]=			"<b><h1>DTE port</b></h1>\r";
static const char szCallBar[]=				"<b><h1>Call barring</b></h1>\r";
static const char szLANConfig[]=			"<b><h1>LAN configuration</b></h1>\r";
static const char szDemandConfig[]=			"<b><h1>PPP WAN interfaces</b></h1>\r";
static const char szRoutingConfig[]=		"<b><h1>Routing table configuration</b></h1>\r";
static const char szDUNConfig[]=			"<b><h1>Dial up networking</b></h1>\r";
static const char szISDNConfig[]=			"<b><h1>ISDN1 configuration</b></h1>\r";
static const char szSecurity[]=				"<b><h1>Enter username and password</b></h1>\r";
static const char szLoadSave[]=				"<b><h1><p>Load and save settings</p></b></h1>\r";
static const char szDHCPtitle[]=			"<b><h1><p>DHCP server configuration</p></b></h1>\r";


#ifdef PPP
static const char szIPCP_badremoteIP[]=		"Could not determine remote IP address";
static const char szIPCP_badlocalIP[]=		"Could not determine local IP address";
static const char szIPCP_up[]=				"IPCP up";
static const char szIPCP_down[]=			"IPCP down";
static const char szIPCP_unauthorisedIP[]=	"Peer is not authorized to use remote address %0a";
static const char szIPCP_localIP[]=			"local IP address %0a";
static const char szIPCP_remoteIP[]=		"remote IP address %0a";
static const char szIPCP_badrej[]=			"IPCP received bad Reject! len=%u";
static const char szIPCP_CI1[]=				"cilong=%u val1=%u";
static const char szIPCP_CI2[]=				"cilong=%u val2=%u";
static const char szIPCP_CI3[]=				"cishort=%u val=%u";
static const char szIPCP_CI4[]=				"cimaxslotindex=%u maxslot=%u";
static const char szIPCP_CI5[]=				"ciflag=%u cflag=%u";
static const char szIPCP_DNS[]=				"IPCP received DNS%u Request ";
static const char szIPCP_WINS[]=			"IPCP received WINS%u Request ";
static const char szIPCP_CONF[]=			"IPCP returning Configure-%s";

static const char szPHASE_DEAD[]=			"down";
static const char szPHASE_INITIALIZE[]=		"init";
static const char szPHASE_ESTABLISH[]=		"dial";
static const char szPHASE_AUTHENTICATE[]=	"auth";
static const char szPHASE_CALLBACK[]=		"callback";
static const char szPHASE_NETWORK[]=		"up";
static const char szPHASE_TERMINATE[]=		"disc";
static const char szPHASE_HOLDOFF[]=		"holdoff";

static const char szPAP_noresp[]=			"PAP No response to authenticate-requests";
static const char szauth_peer_refused[]=	"peer refused to authenticate";
static const char szauth_noprotocols[]=		"No network protocols running";
static const char szauth_failed[]=			"Authentication failed\r\n";
static const char szauth_timer_expired[]=	"Connect time expired\r\n";
static const char szlcp_peer_not_responding[]=  "Peer not responding";

static const char szpercent_u_brackets[]=	"(%u)";
static const char szpercent_s_brackets[]=	"(%s)";

static const char szCmdat[]=				"at";
#endif
