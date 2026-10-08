// tcp.cpp
// Implements TCP reliable transmission protocol (rfc0793)
// One class instance is required per connection
// The TCP state machine diagram is from "inside TCP/IP" ISBN 0-13-474321-0

#include "router.h"
#include "pppd.h"
#include "protocols.h"
#include "tcp.h"
#include "commandport.h"
#include "http.h"
#include "tcpmodem.h"
#include "telnet.h"
#include "utils.h"
#include "C:\DataDir\Adsp21xx\seb\VC5410A\pmRun\schedule.h"
#include "C:\DataDir\Adsp21xx\seb\VC5410A\pmRun\hardware.h"

//--------------------------------------------------------------------------
void CTcp::OnMessage(int16 messg, int32 lParam,int32 wParam,int16 zParam)
{
int16 saved_tx_seq_num;

switch(messg)
	{
	case MSG_TIMER:
	state_machine_timeout();
	break;
	
	case MSG_KEEPALIVE:
	// Send an ACK frame with tx_seq_num 1 less than it should be
	// This forces the remote TCP to respond with an ACK
	saved_tx_seq_num = tx_seq_num;
	tx_seq_num--;
	output(NULL,0,ACK,0,HW_LAN);
	tx_seq_num = saved_tx_seq_num;
	PostMessage(this,MSG_KEEPALIVE,0,0,0,keepalive_ms );
	OutputDebugString(LOG_INFO,LOG_TCP,szTCPtx_keepalive);
	break;
	
	case MSG_RETRANSMISSION:
	rtx_timeout(lParam);
	break;
	}
}

//--------------------------------------------------------------------------
void CTcp::init(int local_port)
{
protocol =  IP_TCP;
name = szTCP;
state = LISTEN;				// TCP state machine initial state.

// TCP frame variables
tx_seq_num = 0;				// Starting tcp sequence number
tx_ack_num = 0;				// Starting acknowledge number
rx_ack_num = 0;
rx_seq_num = 0;
port_local = local_port;	// 0 means retrieve from first incoming frame.
port_remote = 0;			// Retrieved from first incoming frame.

// TCP Frame sizes
MRU = UART_BUFLEN;			// Our max frame size.
MSS = DEFAULT_TCP_SEGMENT_SIZE;		// No. of bytes remote can accept. Updated when a frame received
window = 0;					// Remotes Window size

ip_id = 0;

pClient = NULL;				// Ptr to upper layer protocol (CClient class eg CTCPModem, CHTTP, CPad)

// Object management variables
enabled_flag = true;		// Set to false when TCP session is to be deleted
autodelete = true;			// Flag permits system to delete this instance on idle
time = getTickCount();		// Data inactivity timer

// Retransmission (rtx) buffer
rtx_buf = NULL;				// Buffer created with the client instance. Sized to suit client type.
rtx_buf_size = 0;			// Amount of memory allocated to retransmission buffer. Can be 0.
rtx_buf_in = 0;				// Retransmission buffer in ptr
rtx_buf_out = 0;			// Retransmission buffer out ptr

// Round trip timers for retransmission logic
RTT_MS = 100;				// Default Round Trip Time (time to ACK a frame).
SRTT_MS = RTT_MS;			// Default smoothed round Trip Time
ShortestRTT_MS = 0;
BACKOFF_TIMEOUT_MS = 3000L;	// Retransmission timeout.DEFAULT_TCP_TIMEOUT. Long for GPRS connections;
highest_tx_seq_num = 0;

// TCP keepalive timer interval *Not tested*
keepalive_ms = 0L;			/// 0=Keepalive disabled, otherwise value is interval in ms.

// Statistics
resends = 0;				// # of times a frame is resent without acknowledgement
total_resends = 0;			// Statistic
}

//--------------------------------------------------------------------------
void CTcp::close()
// Purpose:
// Attempt clean shut down of the TCP connection through the TCP state machine
{
// Only send FIN if connected
if( (state < CLOSE_WAIT) && (state > LISTEN) )
	{
	if(resends)
		// Probably closing due to retransmission excess
		tx_seq_num = rx_ack_num;

	output(NULL,0,FIN|ACK,0,HW_LAN);
	state = FIN_WAIT1;
	
	untimeout(MSG_TIMER);
	timeout(0,4000);				// (re)set 4s no-response timeout
	}
else
	enabled_flag=false;				// Mark for instant deletion
}

//--------------------------------------------------------------------------
unsigned int CTcp::input(void *datagram,int len,int32 ip_src_addr,int32 ip_dest_addr,int unit)
// Purpose:
// Handle an incoming TCP frame.
// Implement the TCP state machine - implementation of rfc0793
// Create client object, and pass TCP data to and from it.
// Returns:
// True if frame handled
// False if the frame is not addressed to this client object, or is corrupt.
{
if(their_ip != ip_src_addr)
	return 0;

unsigned char *p = (unsigned char*)datagram;
int16 rcvd_port_remote,rcvd_port_local;

GETSHORT(rcvd_port_remote,p);
GETSHORT(rcvd_port_local,p);
	
if(state > LISTEN)
	{
	// This TCP class instance currently has an active peer connection.
	// Do the received datagram Source and Destination port 
	// numbers match those in this class ?
	if((port_remote != rcvd_port_remote) || (rcvd_port_local != port_local))
		// This frame is not from our TCP peer so
		// must be for another TCP instance.
		return 0;
	}

if(port_local == TRACE_PORT)
	// Disable trace output as this frame is received and any acknowledgement sent
	// else we get in a loop tracing our trace frames !
	traceout.suppress = true;

int32 bytes_acknowledged=0;					// Number of bytes remote acks with this frame					
int32 last_rx_ack_num = rx_ack_num;			// Bytes remote acknowledged prior to this frame. Used to free retransmission buffer
int32 resent_byte_count = 0;				// If remote resends a frame with some new/some old telnet data, this is the offset of the new data

unsigned char data_offset,*pOptions=NULL,flags;
int iRes;
int16 cksum, temp;
GETLONG(rx_seq_num,p);							// rx_seq_num : sequence # of first byte in this frame
GETLONG(rx_ack_num,p);							// rx_ack_num : next byte # expected, should be our next transmitted tx_seq_num
GETCHAR(data_offset,p);
GETCHAR(flags,p);
GETSHORT(window,p);								// Window : max chars remote will accept in next frame."TCP flow control"
GETSHORT(cksum,p);								// TCP frame checksum
GETSHORT(temp,p);								// "Urgent" flag. I'm going as fast as I can already.

int16 telnetbytes = 0;							// No. of telnet bytes in datagram.
unsigned char* telnet_data = NULL;				// Ptr to telnet data

unsigned int tcp_hdrlen = ((data_offset) >> 2);
telnetbytes = len - tcp_hdrlen;					// No of telnet bytes in this datagram.

if(telnetbytes > TCP_MAX_WINDOW)
	{
	// TODO compare with current receive buffer space instead ?
	// Frame too big error. Give up now.
	close();
	return 1;
	}

int16 num_option_bytes = tcp_hdrlen - TCP_HEADERLEN;
if(num_option_bytes > 0)
	{
	pOptions = p;								// Pointer to TCP options field. Use later
	GETSHORT(temp,pOptions);							// First option type.
	pOptions = p;								// Pointer to TCP options field. Use later
	}
// Checksum verification :
// Append incoming TCP datagram to pseudoheader
p = packet_buf + IP_HEADERLEN + TCP_PSEUDOHEADERLEN;
memmove(p,datagram,tcp_hdrlen + telnetbytes);

// Prepend a temporary pseudoheader to TCP frame (datagram).
// The Pseudoheader and TCP frame are created in packet_buf.
p = packet_buf + IP_HEADERLEN;
PUTLONG(lan[ACTIVE].ip_addr,p)					// Our IP addr LAN
PUTLONG(their_ip,p);							// Remote IP addr
PUTCHAR(0,p);									// Reserved
PUTCHAR(6,p);									// Protocol = IP
PUTSHORT(tcp_hdrlen + telnetbytes,p);			// Length. 

// Calculate checksum of received datagram.
int16 tmp_cksum  = CHECKSUM((packet_buf + IP_HEADERLEN),tcp_hdrlen + telnetbytes + TCP_PSEUDOHEADERLEN);
if(tmp_cksum != 0)
	{
	// TCP checksum error. 
	// Don't ACK the frame, ignore it and it will be resent
	sprintf(msg,szTCPFCSerr,cksum,tmp_cksum,tcp_hdrlen + telnetbytes + TCP_PSEUDOHEADERLEN);
	OutputDebugString(LOG_NOTICE,LOG_TCP,(const char*)msg);
//	hexdump((packet_buf + IP_HEADERLEN),48);
	return true;
	}

// The frame checksum is good.

// Recalculate max transmit length for next frame
// The TCP "window" field is the no. of bytes remote can accept, starting from rx_ack_num
int32 highest_seq_num;
highest_seq_num = rx_ack_num + (int32)window;	// Highest sequence number we can send
highest_seq_num -= tx_seq_num;					// Actual number of bytes we can send including those sent and not yet ack'd

if(flags & RST)
	{
	// Remote wants to terminate immediately. No response.
	OutputDebugString(LOG_INFO,LOG_TCP,sztcp_RSTabort);
	state = TIME_WAIT;
	enabled_flag=false;							// This instance and client will be deleted.
	return true;
	}

// Output trace info for this frame
if(LOG_TCP & traceout.module)
	{
	sprintf(msg,szRx_TCP,telnetbytes,(int32)rx_seq_num,(int32)rx_ack_num);
	if(flags & FIN ) strcat((char*)msg,sztcp_FIN);
	if(flags & SYN ) strcat((char*)msg,sztcp_SYN);
	if(flags & RST ) strcat((char*)msg,sztcp_RST);
	if(flags & PSH ) strcat((char*)msg,sztcp_PSH);
	if(flags & ACK ) strcat((char*)msg,sztcp_ACK);
	OutputDebugString(LOG_DEBUG,LOG_TCP,(const char*)msg);
	}

// Check that their rx_ack_num is OK. rfc0793 page33 figure 9
//	A new acknowledgment (called an "acceptable ack"), is one for which the inequality below holds:
//	SND.UNA < SEG.ACK =< SND.NXT
//	SEG.ACK = acknowledgment from the receiving TCP <rx_ack_num>
//	SND.UNA = oldest unacknowledged sequence number	<tx_seq_num - BytesInBuffer()>
//	SND.NXT = next sequence number to be sent		<tx_seq_num>
if(flags & ACK)
	{
	if(rx_ack_num > (highest_tx_seq_num+1))
		{
		// Incoming segment is outside window.
		// (ie Ack number acks something we have not sent)
		if(state >= ESTABLISHED)
			// Remote must close half open connection
			// Unless they did this on purpose to force our response for keep alive
			// rfc0793 page34 figure 10 and page36 para 3
			output(NULL,0,ACK,0,unit);
		else
			{
			// Half open connection discovered in non-synchronised state
			// rfc0793 page34 figure 10 and page36 para 2
			OutputDebugString(LOG_NOTICE,LOG_TCP,szTCPresetHalfOpen);
			port_local = rcvd_port_local;
			port_remote = rcvd_port_remote;
			tx_seq_num = rx_ack_num;
			output(NULL,0,RST,0,unit);
			enabled_flag=false;						// Delete this instance
			}
		return 1;
		}

	if(rtx_buf != NULL)
		if(rx_ack_num < (tx_seq_num - 1 - (int32)BytesInBuffer()))
		{
		// Incoming segment is outside window.
		// (ie Bad ack - too low)
		if(state >= ESTABLISHED)
			// Remote must close half open connection
			// rfc0793 page34 figure 10 and page36 para 3
			output(NULL,0,ACK,0,unit);
		else
			{
			// Half open connection discovered in non-synchronised state
			// rfc0793 page34 figure 10 and page36 para 2
			OutputDebugString(LOG_NOTICE,LOG_TCP,szTCPresetWindow);
			port_local = rcvd_port_local;
			port_remote = rcvd_port_remote;
			tx_seq_num = rx_ack_num;
			output(NULL,0,RST,0,unit);
			enabled_flag=false;						// Delete this instance
			}
		return 1;
		}
	}

// Check the data received in this frame is correctly sequenced	
if(telnetbytes)
	{
	// tx_ack_num is the sequence number of the next octet expected from the remote.
	// Expect to find rx_seq_num == tx_ack_num
	if(rx_seq_num > tx_ack_num)
		{
		// Their tx seq num is too high. We must have missed a previous frame.
		sprintf(msg,szTCPremote_sequence,(int32)rx_seq_num,(int32)tx_ack_num);
		OutputDebugString(LOG_NOTICE,LOG_TCP,(const char*)msg);
		// Ignore this frame. Remote will have to resend the missed frame
		return 1;
		}
	
	if(rx_seq_num < tx_ack_num)
		{
		// Remote is retransmitting, because either:
		// Our ACK got lost 
		// Our ACK was not sent. If remote was timed out from our ARP cache the ACK frame would have been discarded
		if((rx_seq_num + telnetbytes) <= tx_ack_num)
			{
			// We have already ACK'd ALL the bytes in this frame
			// Acknowledge the data and discard it.
			sprintf(msg,szTCPremote_resend,telnetbytes,(int32)tx_ack_num,(int32)rx_seq_num);
			OutputDebugString(LOG_INFO,LOG_TCP,(const char*)msg);
			output(NULL,0,ACK,0,unit);
			return 1;
			}

		// We have already ACK'd SOME of the bytes in this frame.
		// resent_byte_count is the number of bytes we already received.
		// Will use this as an offset to extract the new data only
		resent_byte_count = tx_ack_num - rx_seq_num;

		// The number of new bytes in this frame (ie not previously received)
		telnetbytes = telnetbytes - (int16) resent_byte_count;
			
		sprintf(msg,szTCPremote_resend2,resent_byte_count,telnetbytes);
		OutputDebugString(LOG_INFO,LOG_TCP,(const char*)msg);
		}

	// Acknowledge all received bytes in our response.
	tx_ack_num = rx_seq_num + telnetbytes + resent_byte_count;
	// Pointer to the new telnet data
	telnet_data = packet_buf + IP_HEADERLEN + TCP_PSEUDOHEADERLEN + tcp_hdrlen + (int16)resent_byte_count;

	if(telnetbytes==0)	 // Should always be true...
		OutputDebugString(LOG_ERR,LOG_TCP,szTCPseqerror);
	}
else
	{
	// No telnet bytes in this frame
	if((flags & ACK) && (rx_seq_num == (tx_ack_num-1)))
		{
		// An ACK frame with received sequence number 1 less than expected.
		// This may be the remote sending a TCP keepalive.
		// Send an ACK with the expected sequence number.
		OutputDebugString(LOG_INFO,LOG_TCP,szTCPrx_keepalive);
		output(NULL,0,ACK,0,unit);

		time = getTickCount();						// Refresh data inactivity timer
		return 1;
		}
	}

if((traceout.module & LOG_SERIAL)&&(telnetbytes))
	{
	sprintf(msg,"<TCP Rx %u> ",telnetbytes);
	OutputDebugString(LOG_INFO,LOG_SERIAL,(const char*)msg,false);
	printformatstring(msg,0,(char*)telnet_data,min(telnetbytes,20),traceout.hexmode,false);
	OutputDebugString(LOG_INFO,LOG_SERIAL,(const char*)msg);
	}

// Action depends on the TCP state machine			
topcase:
switch(state)
	{
	case LISTEN:
		// Save the port numbers for this connection
		port_local = rcvd_port_local;
		port_remote = rcvd_port_remote;
		tx_ack_num = rx_seq_num + 1 /*+ telnetbytes*/;		// There is an option to send data with the open request..

		// We are only interested in OPEN requests when listening
		if(flags != SYN)
			{
			// Active Side Causes Half-Open Connection Discovery
			// rfc0793 page35 figures 11 and 12
			sprintf(msg,szTCPIgnoringFrame,tcpstate_2ascii(state));
			OutputDebugString(LOG_NOTICE,LOG_TCP,(const char*)msg);

			// rfc0793 page36 para 1			
			if(flags & ACK)
				{
				tx_seq_num = rx_ack_num;
				output(NULL,0,RST,0,unit);
				}
			else
				{
				tx_seq_num = 0;
				tx_ack_num = rx_ack_num + telnetbytes;
				output(NULL,0,ACK|RST,0,unit);
				}

			enabled_flag=false;								// Delete this instance
			return 1;
			}

		// Received an OPEN request
		tx_seq_num = getTickCount();						// Random starting sequence number.
		highest_tx_seq_num = tx_seq_num;
		
		// Extract the TCP options.
		// The mss (remote max segment size) is the only parameter defined for the options field
		MSS = DEFAULT_TCP_SEGMENT_SIZE;

		// Get the remotes MSS if specified. Nb. Only works if it is the first option specified
		if((num_option_bytes > 0) && ( temp == 0x0204))
			{
			// MSS option is present in the frame (MSS option kind=2 length=4)
			// Want value of Maximum Segment size option
			// "temp" is the value of the first options field.
			GETSHORT( MSS,pOptions );					// Max bytes remote accepts in any frame
			sprintf(msg,szTCPmss,MSS,window);
			OutputDebugString(LOG_NOTICE,LOG_TCP,(const char*)msg);
			}

		// Create a higher layer CClient object based on the TCP port number the remote is using
		if(! CreateClient(rcvd_port_local))
			{
			// Couldn't create client, we don't support the port number or our client wouldn't accept connection.
			// Send FIN to close TCP
			// rfc0793 page 37.
			tx_seq_num = 0;
			output(NULL,0, ACK|RST ,0,HW_LAN);			
			enabled_flag=false;							// Delete this instance
			return 1;
			}

		// Inform the CClient object that a connection is requested
		if(0 == pClient->OnTransport(TRANSPORT_CALL,0))
			{
			// Client can't accept more serial data.
			// Refuse connection.
			OutputDebugString(LOG_WARNING,LOG_TCP,szTCPNoMemSerial);
			tx_seq_num = 0;
			output(NULL,0, ACK|RST ,0,HW_LAN);			
			enabled_flag=false;							// Delete this instance
			return 1;
			}	
		
		telnet_data = (unsigned char*)pClient->szBanner;
		telnetbytes = 0;

		if(telnet_data)
			// Send the welcome message, if any
			telnetbytes = strlen((const char*)telnet_data);
			
		// Default max chars we can rcv. ( Typically the size of the uart buffer )
		MRU = pClient->MRU;

		state = SYN_RECEIVED;

		// Accept the conection	- we respond with SYN and ACK flags set	
		output(telnet_data,telnetbytes, SYN | ACK,0,unit);

		// DEBUG TODO do this better ? This not done in output()
		rtx_buf_in = 0;				// Retransmission buffer in ptr
		rtx_buf_out = 0;			// Retransmission buffer out ptr
		retransmission_buffer_in(telnet_data,telnetbytes);		

		tx_seq_num++;
		break;

	case SYN_SENT:
		// We initiated a connection, this is the response
		if(!flags & SYN) 
			break;

		// Get the remote port number for this connection
		port_remote = rcvd_port_remote;
		tx_ack_num = rx_seq_num + 1 + telnetbytes;
		output(NULL,0, ACK,0,unit);
		state = ESTABLISHED;
				
		// Tell CClient object the call is connected.
		pClient->OnTransport(TRANSPORT_OPEN,0);
		break;
	
	case SYN_RECEIVED:
		// Have received SYN, ACK'd it.
		// Looking for ACK to go to state ESTABLISHED
		if(flags & SYN)
			{
			state = LISTEN;				// Repeat of initial SYN frame. Our reply must have got lost
			goto topcase;				// Call this function again to process
			}
		if(!(flags & ACK))				// Has remote ACK'd our SYN frame ?
			return true;				// No. Expected an ACK

		state = ESTABLISHED;
		
		// Initiate sending of TCP keepalives
		if(keepalive_ms)
			PostMessage(this,MSG_KEEPALIVE,0,0,0,keepalive_ms);
			
		// Tell client the call is connected.
		pClient->OnTransport(TRANSPORT_OPEN,0);
		break;
		
	case DELAYED_ACK:									// Our ACK was delayed for serial buffer to clear
		untimeout(MSG_TIMER);							// Frame in. Clear delayed ack timer
		state = ESTABLISHED;
				
	case ESTABLISHED:
	
		if(flags & ACK) 
			{
			time = getTickCount();						// Refresh data inactivity timer

			// Frame contains a valid ACK number.
			// If any new bytes are ACK'd then remove them from the retransmission buffer
			bytes_acknowledged = rx_ack_num - last_rx_ack_num;		// Newly ACK'd bytes
			if(bytes_acknowledged)
				{
				retransmission_buffer_free(bytes_acknowledged);
				resends=0;
				}
			}
			
		if((flags & FIN) && ( flags & ACK)) 
			{
			// Passive close
			// Remote wants to terminate.Send ACK
			// Frame may have included telnet data
			// Nb. I think the client cannot send data in response.
			if(telnetbytes)			
				 pClient->receive(&telnet_data,&telnetbytes,window);
			
			tx_ack_num++;
			output(NULL,0, ACK,0,unit);
			state = CLOSE_WAIT;					// Wait for CLOSE_WAIT state
			output(NULL,0, FIN|ACK,0,unit);
			state = LAST_ACK;					// "Wait for for remote ACK" state
			untimeout(MSG_TIMER);
			timeout(0,5000);					// Time to wait for LAST_ACK to  arrive
			break;
			}
			
		if(telnetbytes)
			{
			// Received telnet data
			if(telnetbytes > pClient->GetReceiveBuffer())
				{// Too much data for client. Remote should resend it later.
				sprintf(msg,szTCPrxlimit,pClient->GetReceiveBuffer());
				OutputDebugString(LOG_NOTICE,LOG_TCP,(const char*)msg);
				break;
				}

			// If the client wants to send data in response, the calling params point to it.
			iRes = pClient->receive(&telnet_data,&telnetbytes,window);
				
			if(0)//telnetbytes > MSS)
				{
				// Truncating send data. An error
				sprintf(msg,szTCPtruncate,telnetbytes, MSS);
				OutputDebugString(LOG_NOTICE,LOG_TCP,(const char*)msg);
				// TODO
				// Do not exceed remotes' Maximum Segment Size
				// **WARNING** next line simply discards data. 
				// Client should not send more than MSS bytes (done)
				telnetbytes = min(telnetbytes, MSS);
				}

			switch(iRes)
				{
				case TERMINATE:
				// Send response frame, with FIN to terminate TCP session now
				output(telnet_data,telnetbytes, FIN|ACK,0,unit);
				timeout(0,5000);
				state = FIN_WAIT1;
				break;

				case ACKNOWLEDGE:
				// Acknowledge the frame, including any client data in the frame.
				output(telnet_data,telnetbytes,ACK,0,unit);
				break;

				case NORESPONSE:
				// Don't send a response. 
				// This case used if response already sent by upper layer protocol.
				break;
				}
			}
		else if(pClient->quit)
			{
			// Clients' "close session" flag is set
			output(NULL,0, FIN|ACK,0,unit);
			untimeout(MSG_TIMER);
			timeout(0,5000);
			state = FIN_WAIT1;
			}
		
		break;

	case FIN_WAIT1:
		if(flags & FIN)
			{
			// Rcvd their FIN to our FIN.
			// "Simultaneous close"
			tx_seq_num++;
			tx_ack_num++;
			output(NULL,0, ACK,0,unit);
			state = CLOSING_TCP;
			}
		if(flags & ACK)
			// Rcvd ACK of FIN
			state = FIN_WAIT2;

		untimeout(MSG_TIMER);
		timeout(0,5000);		// 2.5Secs delay seen from their ACK|FIN to their ACK with Win98
		break;

	case FIN_WAIT2:
		if(flags & FIN)
			{
			// Rcvd their FIN|ACK
			tx_seq_num++;
			tx_ack_num++;
			output(NULL,0, ACK,0,unit);
			state = TIME_WAIT;
			untimeout(MSG_TIMER);
			enabled_flag = false;
			}
		break;

	case CLOSE_WAIT:
	case CLOSING_TCP:
	case LAST_ACK:
		// Waiting for ACK to our FIN
		if(flags & ACK)
			{
			// Got it. Session is closed
			state = TIME_WAIT;
			untimeout(MSG_TIMER);
			enabled_flag = false;
			}
		break;
		
	case TIME_WAIT:
		// Waiting for end session timeout to expire
		// Not expecting any frames here.
		enabled_flag = false;
		break;

	default:
		break;
	}

return true;
}


//--------------------------------------------------------------------------
void CTcp::state_machine_timeout()
// A timer has expired.
{
switch(state)
	{
	case DELAYED_ACK:
		// Special case. Not a state in the TCP state machine.
		// The ACK was delayed as we had insufficient serial buffer to receive for the next frame
//		OutputDebugString(LOG_DEBUG,LOG_SYSTEM,(const char*)"(ACK-delayed)");
		output(NULL,0, ACK,0,HW_LAN);
		break;

	case LISTEN:
		break;

	case SYN_RECEIVED:
		// Waiting for an ACK to our Open acknowledge frame. Kill the connection
		break;

	case ESTABLISHED:
		break;

	case FIN_WAIT2:
		// Didnt get their FIN|ACK.
	case FIN_WAIT1:
		// Didnt get ACK to our FIN|ACK.
	case CLOSE_WAIT:
		// Transient state is never used
	case LAST_ACK:
		// Waited for their ACK to our FIN|ACK.
		// It never arrived
	case TIME_WAIT:
		// Should wait 4 minutes (2 * MSL) per rfc793
		// However, that would prevent additional connections.
		// TODO could delete buffers, then wait to conform to above spec.
		state = CLOSING_TCP;
		enabled_flag=false;
		break;
	
	case CLOSING_TCP:
		// This tcp client will be deleted in the system idle process
		enabled_flag=false;	 
		break;
		
	default:
		break;		
	}
}

//--------------------------------------------------------------------------
void CTcp::output(unsigned char* telnet_data, int telnetbytes, char code, int32 unused,int unit,bool retransmission)
// Send a TCP frame. Generated in packet_buf
// Params:
// telnet_data = ptr to telnet data
// telnetbytes = no of bytes to send
// code = TCP code field : ACK,NAK etc
// unit = destination interface
{
if((telnetbytes) && (state > SYN_RECEIVED))
	{
	// Track their receive window so we don't send too much data
	if(window >= telnetbytes)
		window -= telnetbytes;
	else 
		{
		sprintf(msg,szTCPframeerr,telnetbytes,window);
		OutputDebugString(LOG_WARNING,LOG_TCP,(const char*)msg);
		window = 0;
		}
		
	if((state != ESTABLISHED) && (state != DELAYED_ACK))
		{// Error. Can't send data when not established (see state definitions in tcp.h)
		sprintf(msg,szTCPnot_established,tcpstate_2ascii(state));
		OutputDebugString(LOG_NOTICE,LOG_TCP,(const char*)msg);
		return;
		}

	if(!retransmission)
		{
		// Add the TCP data to our retransmission buffer....
		// unless this is a retransmission in which case the data is already in the retransmission buffer
		retransmission_buffer_in(telnet_data,telnetbytes);	// Telnet data into retransmission buffer	
	
		if(resends)
			{
			// ERROR. New data arrived while we are retransmitting lost frames. 
			// Don't send a TCP frame as it will be out of sequence. The new data is buffered.
			sprintf(msg,szTCPbuffering,telnetbytes, rtx_buf_size - GetTransmitBuffer() - 1);
			OutputDebugString(LOG_NOTICE,LOG_TCP,(const char*)msg);
			tx_seq_num += telnetbytes;
			return;
			}
	
//		if(MSS >= telnetbytes)
//			MSS -= telnetbytes;				// Bytes remote client can accept. Updated when a frame rcvd.
//		else MSS = 1;						// Error ! Sending more than client can accept.
		}
	}

int32 our_window;
int OptionLen = 0;
if(code & SYN )
	OptionLen  = 8;									// Length in bytes of the option fields
	
char DataOffset = DATAOFFSET + ( OptionLen / 4);	// Header length, in Words * 0x10
DataOffset *= 0x10;

// Need to prepend headers to this frame as it passes down the stack.
// Calculate the header rooom needed
int TCP_OFFSET = IP_HEADERLEN;			// 20

if(unit == HW_LAN)
//				| LAN (14)		| IP (max=20) | TCP (20->28) |
	TCP_OFFSET += LAN_HDRLEN;
/*
else
//				| PPP (max=4)	| IP (max=20) | TCP (20->28) |
	TCP_OFFSET += PPP_HDRLEN + MP_HEADERLEN + 2;
*/
unsigned char* outp= &packet_buf[TCP_OFFSET];	// TCP header structure

// Generate a pseudoheader to be prepended to TCP frame.
// The pseudoheader is used only for checksum calculation then discarded
unsigned char* p = &packet_buf[TCP_OFFSET - TCP_PSEUDOHEADERLEN];
PUTLONG(their_ip,p);						// Remote IP addr
PUTLONG(lan[ACTIVE].ip_addr,p)				// Our IP addr LAN
PUTCHAR(0,p);								// Reserved
PUTCHAR(IP_TCP,p);							// Protocol = TCP
PUTSHORT((unsigned int)TCP_HEADERLEN + OptionLen + telnetbytes,p);	// Length of TCP datagram not including pseudoheader

// Generate TCP header following pseudo header. ( at &packet_buf[TCP_OFFSET] )
PUTSHORT(port_local,p);						// Our port no.
PUTSHORT(port_remote,p);					// Their port no.

PUTLONG(tx_seq_num,p);						// Our tx sequence number (seq num of first bute in frame)
PUTLONG(tx_ack_num,p);						// Acknowledge number
		
PUTCHAR(DataOffset,p);						// Header length. n words, n*4 bytes
if((telnetbytes > 0) && (state == ESTABLISHED))
	code |= PSH;							// PSH flag set if there is telnet data

PUTCHAR((char)code,p);						// Flags eg ACK, NAK

if(pClient)
	{
	our_window = pClient->GetReceiveBuffer();	// How many bytes CClient object will accept in the next frame
	our_window = min(MRU,our_window);
	}
else
	our_window = min(MRU,our_window);

PUTSHORT(our_window,p);						// max chars we will accept in next frame. Depends on CClient buffer space.
PUTSHORT(0,p);								// Checksum
PUTSHORT(0,p);								// Urgent position

int32 Option;
if(OptionLen == 8)							// Length, in bytes of the following 2 fields...
	{										// This is the "MSS" option.
	Option = 0x020405b4L;
	PUTLONG(Option,p);
	Option = 0x01010402L;
	PUTLONG(Option,p);
	}

if(telnetbytes)
	memmove(p,telnet_data,telnetbytes);		// Append telnet data to TCP header

int16 len = TCP_HEADERLEN + OptionLen + telnetbytes + TCP_PSEUDOHEADERLEN;
int16 cksum  = CHECKSUM(&packet_buf[TCP_OFFSET - TCP_PSEUDOHEADERLEN],len);

DECPTR(4 + OptionLen,p);					// Point at checksum
PUTSHORT(cksum,p);							// Stuff checksum in the frame

if(port_local == TRACE_PORT)
	// Disable trace output as this frame is sent or we trace our own trace o/p (bad)
	traceout.suppress = true;

if(LOG_TCP & traceout.module)
	{
	sprintf(msg,szTx_TCP,telnetbytes,(int32)tx_seq_num,(int32)tx_ack_num);
	if(code & FIN ) strcat((char*)msg,sztcp_FIN);
	if(code & SYN ) strcat((char*)msg,sztcp_SYN);
	if(code & RST ) strcat((char*)msg,sztcp_RST);
	if(code & PSH ) strcat((char*)msg,sztcp_PSH);
	if(code & ACK ) strcat((char*)msg,sztcp_ACK);
	OutputDebugString(LOG_DEBUG,LOG_TCP,(const char*)msg);
	}

// Send the frame using IP protocol
output_ip(ip_id++,IP_TCP,their_ip,outp,TCP_HEADERLEN + OptionLen + telnetbytes,unit);

// Remote is expected to ACK data frames within a given time so set a retransmission timer now.
// If remote ACKS this frame:-		retransmission_buffer_free deletes the timer MSG_RETRANSMISSION
// If remote does not ACK in time:-	retransmission timer will call func rtx_timeout()
// Nb If retransmissions are off this mechanism is used to detect remote frame loss
// and simply reset our transmission sequence number so the connection can continue. (although data is lost)
if((telnetbytes > 0) && (rtx_buf != NULL))
	{
	// Set boundaries on retransmission timer
	RTO_MS	= min(UBOUND_MS,max(LBOUND_MS, (SRTT_MS *3/2)));	// P504. Beta=1.5

	// Backoff timer with upper boundary. "2" is the reccommended backoff scale (P505)
	// Nb Exponential backoff delay.
	if(retransmission)
		BACKOFF_TIMEOUT_MS = min(BACKOFF_TIMEOUT_MS * 2, UBOUND_MS);
	else
		BACKOFF_TIMEOUT_MS = RTO_MS;

	// Set a retransmission timer for this frame
	PostMessage(this,MSG_RETRANSMISSION,tx_seq_num,getTickCount(),telnetbytes,(int32)BACKOFF_TIMEOUT_MS );
	}

tx_seq_num = tx_seq_num + telnetbytes;		// This should be the ack num of the next frame received	

// Save the highset sequence number transmitted.
if(!retransmission)
	{
	highest_tx_seq_num = tx_seq_num;
	resends = 0;
	}
	
// Cancel current keepalive and set a timer for a new one
if(keepalive_ms)
	{
	while(untimeout(MSG_KEEPALIVE));
	PostMessage(this,MSG_KEEPALIVE,0,0,0,keepalive_ms);
	}

// Restore tracing if disabled
traceout.suppress = false;
}

//--------------------------------------------------------------------------
void CTcp::rtx_timeout(int32 resend_seq_num)
// This is a frame retransmission timer, set when frame transmitted.
// If we are here, the timer has expired so the data in the frame has not been acknowledged.
// Resend the data from the retransmission buffer
// resend_seq_num is the seq_num of the LAST BYTE in the frame that initiated this rtx timer
{
if(resend_seq_num == 0) return;

int resend_count;
int temp;

/*
// Unused code : rtx_buf should never be NULL at this point
if(rtx_buf==NULL)
	{
	// We have no retransmission buffer so lost data cannot be resent.
	// Continue transmission (the data is lost)
	tx_seq_num = rx_ack_num;

	// Delete all outstanding retransmission timers for this object
	while(untimeout(MSG_RETRANSMISSION));

	return;
	}
*/

// If the last byte in this frame has not been ACK'd yet, then resend it.
if(rx_ack_num <= resend_seq_num)
	{
	// Sent data has not been acknowledged. 
	// Will resend from last acknowledged byte ( rx_ack_num )

	// Delete all outstanding retransmission timers for this object
	while(untimeout(MSG_RETRANSMISSION));
	
	// Number of bytes to resend : all from last valid acknowledgement
	// Either of these calculations should yield the same result....
	// 1/ The entire contents of the retransmission buffer
	// 2/ tx_seq_num - rx_ack_num bytes
	temp=resends;
	resends=0;
	resend_count = rtx_buf_size - GetTransmitBuffer() - 1;
	resends=temp;
	
	// Bump up stats
	resends++;
	total_resends++;
		
	if(resends >= TCP_RESEND_LIMIT)
		{
		// Close TCP
		sprintf(msg,sztcp_retransmit_limit);
		OutputDebugString(LOG_NOTICE,LOG_TCP,(const char*)msg);
		close();
		return;
		}
	else		
		{
		sprintf(msg,sztcp_retransmit,resends,resend_count,MSS,(int32)tx_seq_num,(int32)rx_ack_num);
		OutputDebugString(LOG_NOTICE,LOG_TCP,(const char*)msg);
		}
		
	// Roll back the tx sequence number to the last acknowledged byte
	tx_seq_num = rx_ack_num;
		
	// Get data for retransmission from buffer
	retransmission_buffer_out(&packet_buf[TCP_DATA_OFFSET],resend_count);
		
	// Resend frame. ( This sets a new retransmission timer )
	output(&packet_buf[TCP_DATA_OFFSET],resend_count,ACK,0,HW_LAN,true);
	}
}

//--------------------------------------------------------------------------
void CTcp::retransmission_buffer_free(int32 telnet_len)
// Received a frame acknowledging reception of "telnet_len" bytes
// Free acknowledged bytes from our retransmission buffer
// Delete retransmission timers
// Recalculate SRTT ( static round trip time)
{
int32 timesent;

// Delete all frame resend timers for newly acknowledged bytes
// If timesent > 0 the timer matching rx_ack_num was already deleted 
timesent = router->untimeout_rtx(this,rx_ack_num);

if(timesent)
	{
	// Recalculate RTT round trip time
	RTT_MS = getTickCount() - timesent;
	// Update ShortestRTT round trip time
	if(ShortestRTT_MS == 0)
		ShortestRTT_MS = RTT_MS;
	ShortestRTT_MS = min( ShortestRTT_MS, RTT_MS);
	// Update SRTT smoothed round trip time
	SRTT_MS = ( SRTT_MS/3 * 2 ) + ( RTT_MS/3 );

//	sprintf(msg,"RTT_MS=%lu SRTT_MS=%lu",RTT_MS,SRTT_MS);
//	OutputDebugString(LOG_DEBUG,LOG_SYSTEM,(const char*)msg);	
	}

// Free acknowledged bytes from our retransmission buffer
rtx_buf_out += telnet_len;
if(rtx_buf_out < rtx_buf_size)
	goto done;
	
// Out pointer wrap
rtx_buf_out = rtx_buf_out - rtx_buf_size;

done:
//sprintf(msg,"RTX buf freed %lu bytes remain %u rtx_buf_in=%u rtx_buf_out=%u",telnet_len,GetBuffer(),rtx_buf_in,rtx_buf_out);
//OutputDebugString(LOG_DEBUG,LOG_SYSTEM,(const char*)msg);	
return;
}


//--------------------------------------------------------------------------
int	CTcp::retransmission_buffer_in(unsigned char *telnet_data,int telnet_len)
// Copy Telnet data to circular retransmission buffer rtx_buf
// Buffer will wrap if too many  bytes supplied
// Exit: rtx_buf_in points at next location to put a byte into the buffer
{
if((!rtx_buf) || (telnet_len ==0))
	return 0;

unsigned char ch;
int  n = 0;

do
	{
	ch = telnet_data[n++];
	rtx_buf[rtx_buf_in++]=ch;

	if (rtx_buf_in >= rtx_buf_size) 
		rtx_buf_in = 0;
	}while( n < telnet_len);

//sprintf(msg,"RTX buf in %u bytes rtx_buf_in=%u rtx_buf_out=%u",telnet_len,rtx_buf_in,rtx_buf_out);
//OutputDebugString(LOG_DEBUG,LOG_SYSTEM,(const char*)msg);
return n;
}

//--------------------------------------------------------------------------
int	CTcp::retransmission_buffer_out(unsigned char *telnet_data,int telnet_len)
// Copy Telnet data from circular retransmission buffer to telnet_data for retransmission
// Buffer will wrap if too many  bytes requested
// Does not move out pointer. Only ACKs from remote can do this.
{
if(!rtx_buf)
	return 0;

int  n = 0;
int rtx_buf_out_tmp = rtx_buf_out;

do
	{
	telnet_data[n++] = rtx_buf[rtx_buf_out_tmp++];

	if (rtx_buf_out_tmp >= rtx_buf_size) 
		rtx_buf_out_tmp = 0;
	}while( n < telnet_len);

return n;
}

//--------------------------------------------------------------------------
int CTcp::BytesInBuffer()
// Return the number of bytes in the retransmission queue.
{
if(!rtx_buf)
	return 0;
	
if(rtx_buf_in  == rtx_buf_out )
	return 0;

else if(rtx_buf_in  > rtx_buf_out )
//			out				in			rtx_buf_size
//	|__________|_DDDDDDDDDDD_|______________|
	return (rtx_buf_in - rtx_buf_out) - 1;
	
// else, (rtx_buf_out  > rtx_buf_in )
//			in				out			rtx_buf_size
//	|_DDDDDDDD_|_____________|_DDDDDDDDDDDD_|

return rtx_buf_size - (rtx_buf_out - rtx_buf_in) - 1;
}

//--------------------------------------------------------------------------
int16 CTcp::GetTransmitBuffer()
// Return the number of bytes that can be transmitted.
// Depends on:
// 1. The space available in the retransmission buffer.
// 2. Remote window size, MSS
// Nb. Don't allow buffer to fill ! Always report 1 byte less than (buffer size - bytes in buffer).
// Why ? Either full or empty buffers would satisfy the above statement - indistinguishable
{
int16 iret;

if(!rtx_buf)
	// Retransmissions are off.
	// No need to check retransmit buffer for avail space
	return min(MSS,window);
	
if(resends)
	// We are waiting for the remote to respond to a resend.
	// Do not accept any more serial data
	return 0;

if(rtx_buf_in  == rtx_buf_out )
	iret = rtx_buf_size -1;
	
if(rtx_buf_in  > rtx_buf_out )
//			out				in			rtx_buf_size
//	|__________|_DDDDDDDDDDD_|______________|
	iret = ( rtx_buf_out + ( rtx_buf_size - rtx_buf_in)) - 1;	

// else, (rtx_buf_out  > rtx_buf_in )
//			in				out			rtx_buf_size
//	|_DDDDDDDD_|_____________|_DDDDDDDDDDDD_|

else iret = (rtx_buf_out - rtx_buf_in) - 1;

/*
if(rtx_buf)
	{
	if(iret > rtx_buf_size)
		{
		sprintf(msg,"RTX buf err. iret=%u in=%u out=%u size=%u",iret,rtx_buf_in,rtx_buf_out,rtx_buf_size);
		OutputDebugString(LOG_DEBUG,LOG_SYSTEM,(const char*)msg);	
		}
	}

if(iret > 0 )
	{
	sprintf(msg,"GetBuffer() = %u in=%u out=%u size=%u",iret,rtx_buf_in,rtx_buf_out,rtx_buf_size);
	OutputDebugString(LOG_DEBUG,LOG_SYSTEM,(const char*)msg);	
	}
*/
//sprintf(msg,"GetBuffer() MSS=%u iret=%u",MSS,iret);
//OutputDebugString(LOG_DEBUG,LOG_SYSTEM,(const char*)msg);	
//return min(MSS,iret);
return min(window,iret);
}

//--------------------------------------------------------------------------
CClient *CTcp::CreateClient(int16 portnumber)
// Create a CClient object to handle the connection data based on the TCP port number.
// Checks availavble memory before creating client class
// Called when TCP session opened.
{
CClient* TcpClient = NULL;
int HW_CLASS = HW_NONE;
bool bAutoDeleteClient = true;				// Client is automatically deleted when pClient->quit = true

if(portnumber == 0)							// Reserved
	return NULL;

int16 free_mem = freemem();
rtx_buf_size = UART_BUFLEN; 				// Default retransmission buffer size
if(free_mem < ( UART_BUFLEN + 100 ))		// Space for TCPmodem
	{
	sprintf(msg,szTCPNoMem,free_mem);
	OutputDebugString(LOG_INFO,LOG_SYSTEM,(const char*)msg);
	goto fail;
	}

if(pClient)
	{
	// There should not be a client, unless we receive 2 SYNS
	// If this happens is I delete and recreate the client.
	if(pClient->autodelete)
		{
		delete(pClient);
		pClient = NULL;
		}
	else
		{
		TcpClient = pClient;		// Client is the shell (I think)
		goto skipcreate;
		}
	}

// Create a client based on the TCP port number.
// The modem port number is user configurable and so could be the same as TELNET_PORT,
// If this is the case we create a modem client not a telnet client.
if( portnumber == modem_config.local_port )
	{
	// The console (shell) AT decoder will accept the call if it has no transport (ie it is idle)
	if(!shell->transport)
		{
		HW_CLASS = shell->HW_IF;
		if(free_mem > (rtx_buf_size + sizeof(shell)))
			TcpClient = shell;
		shell->quit = false;
		bAutoDeleteClient = false;				// Client is the shell. Don't delete it.
		}
	}

#ifdef SMTP
// TODO
else if( portnumber == mail_config.local_port )
	TcpClient = smtp;

#endif

else switch(portnumber)
	{
	case TRACE_PORT:
		rtx_buf_size = 0;					// No retransmission for Trace (saves RAM)
		if(free_mem > (rtx_buf_size + sizeof(CTelnet)))
			TcpClient = new CTelnet;
		HW_CLASS = HW_TRACE;					// Select trace port
		break;

	case TELNET_PORT:
		rtx_buf_size = 256;
		if(free_mem > (rtx_buf_size + sizeof(CCommandPort)))
			TcpClient = new CCommandPort;
		HW_CLASS = HW_NONE;					// Select no serial port (data from LAN)
		break;

	case HTTP_PORT:
		rtx_buf_size = 0;					// No retransmission for HTTP (saves RAM)
		if(free_mem > (rtx_buf_size + sizeof(Chttp)))
			TcpClient = new Chttp;
		break;

#ifndef SEB
	case TA_PORT:
		if(free_mem > (rtx_buf_size + sizeof(CTelnet)))
			TcpClient = new CTelnet;
		HW_CLASS = HW_ISDN;						// Automatically select serial port
		break;

	case TA_PORT1:
		if(free_mem > (rtx_buf_size + sizeof(CTelnet)))
			TcpClient = new CTelnet;
		HW_CLASS = HW_TA;						// Select serial port 1
		break;

	case TA_PORT2:
		if(free_mem > (rtx_buf_size + sizeof(CTelnet)))
			TcpClient = new CTelnet;
		HW_CLASS = HW_TA;						// Select serial port 2
		break;
		
	case MODEM_PORT1:
		if(free_mem > (rtx_buf_size + sizeof(CTCPModem)))
			TcpClient = new CTCPModem;
		HW_CLASS = HW_DTE0;						// Select serial port 1
		break;

	case MODEM_PORT2:
		if(free_mem > (rtx_buf_size + sizeof(CTCPModem)))
			TcpClient = new CTCPModem;
		HW_CLASS = HW_DTE1;						// Select serial port 2
		break;
#endif
	}


if(TcpClient == NULL)
	{
	// Not enough memory for client class
	sprintf(msg,szTCPNoMemClient,portnumber,free_mem);
	OutputDebugString(LOG_NOTICE,LOG_SYSTEM,(const char*)msg);
	pClient=NULL;
	goto fail;									// Create fail
	}

// Client instance created
pClient = TcpClient;
idletimer_ms = lan[ACTIVE].tcp_idletimer;		// Default TCP idle timeout value. Override in pClient->init() if required
pClient->init(HW_CLASS,this,bAutoDeleteClient);

skipcreate:

// Allocate a retransmission buffer..
// Retransmission buffer is deleted in tcp base class destructor ~CProtocol_L3
if(rtx_buf_size)
	{
	if(rtx_buf)
		// Already allocated !
		delete [] rtx_buf;
		
	rtx_buf = new unsigned char[rtx_buf_size];
	if( ! rtx_buf)
		{
		sprintf(msg,szTCPNoMem,free_mem,pClient);
		OutputDebugString(LOG_NOTICE,LOG_SYSTEM,(char*)msg);	
		goto fail;
		}
	}

if(pClient->quit)
	// Client refuses connection
	goto fail;

sprintf(msg,szTCPCreatedClient,TcpClient->name,portnumber);
OutputDebugString(LOG_INFO,LOG_SYSTEM,(const char*)msg);
return pClient;

fail:
sprintf(msg,szTCPFailCreateClient,portnumber);
OutputDebugString(LOG_NOTICE,LOG_SYSTEM,(const char*)msg);	
if(rtx_buf)
	{
	delete [] rtx_buf;
	rtx_buf_size = 0;	
	}
return pClient;
}

