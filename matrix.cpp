// matrix.cpp
#include "router.h"
#include "pppd.h"
#include "matrix.h"
#include "tcp.h"
#include "string_flash.h"
#include "C:\DataDir\Adsp21xx\seb\VC5410A\pmRun\schedule.h"
#include "C:\DataDir\Adsp21xx\seb\VC5410A\pmRun\hardware.h"

//#define SEB
#ifdef SEB
#include "envSEB.h"
#else
//#include "envECTA.h"
#endif

#ifdef SEB

int Cmatrix::get_input_uarts(unsigned int IF_NUM)
// Copy data from a uart buffer to its owner application
{
CClient *pClient = (CClient*)hw->uart[IF_NUM].pClient;
unsigned int owner = hw->uart[IF_NUM].owner;

unsigned char *pdata = &packet_buf[TCP_DATA];
unsigned int bytesread = 0;

// Read bytes from uart interface "IF_NUM"

if(IF_NUM == HW_TRACE)
	{
	// Trace buffer is a special case, it is one way only.
	while(hw->uart[IF_NUM].uart_buffer_tx_in != hw->uart[IF_NUM].uart_buffer_tx_out)
		{
		pdata[bytesread++] = hw->uart[IF_NUM].uart_buffer_tx[hw->uart[IF_NUM].uart_buffer_tx_out];
		++hw->uart[IF_NUM].uart_buffer_tx_out;
		if (hw->uart[IF_NUM].uart_buffer_tx_out == UART_BUFLEN)
			hw->uart[IF_NUM].uart_buffer_tx_out=0;

// TODO don't exceed TCP window size (negotiated) !!
		if(bytesread == TCP_MAX_WINDOW)
			goto read_buffer_full;
		}
	}
else
	{
	// DTE port.
	// Forward data when inter character timeout expires or when buffer 80% full 
	if((hw->uart[IF_NUM].flag_interCharTimeoutRx_expire == 1) ||
		( hw->uartInputQueue(IF_NUM) > (200/*UART_BUFLEN*/) ))
		{
		while (hw->uartInputChar(IF_NUM,(unsigned int*)&pdata[bytesread++]));
		--bytesread;
		hw->uart[IF_NUM].flag_interCharTimeoutRx_expire=0;
		}
	}

read_buffer_full:

		
if( !pClient)
	return 0;


// Check and signal DTR status change
if(hw->uart[IF_NUM].flag_delta_dtr)
	{
	hw->uart[IF_NUM].flag_delta_dtr = false;
		pClient->OnTransport(TRANSPORT_DTEPORT,DELTA_DTR | (hw->uart[IF_NUM].flag_dtr? DTR_STATE : 0));
	}

if( !bytesread)
	return 0;
		
if(owner >= NUM_UARTS)
	{
	// Send DTE port data to a software object pointed to by pClient
	pClient->serial_receive(pdata,&bytesread);

	// Send clients' response back to DTE port
	while(bytesread--)
			hw->uartOutputChar(IF_NUM,*(pdata++));
	}

return 0;
}

#else	// SEB not defined

bool Cmatrix::get_input_uarts(unsigned int IF_NUM)
// Swap data between sw object and hardware interface buffers
// Poll input buffers for data. If there is data, pass to the owner object.
// Copy owner object reply data to hardware buffer

// nb If the owner is a hw interface, data is copied when it is received at the interface in hardware.cpp

// returns idle : true - no data processed,false - data processed
// Exception:stdout
{
unsigned char *pdata = uart_buf;
unsigned int i = 0;

CTcpClient *pClient = hardware_if[IF_NUM].pClient;
unsigned int owner = hardware_if[IF_NUM].owner;

if((owner <= OWNER_SW_OBJECT) ||
	(owner > OWNER_SHELL))
	// Either - no other HW or SW connected to this port. Return, discarding the data
	// Or - DTE HW is connected to this port. Handled in hardware.cpp IRQ routine
	// Or - DTE in use by PPP. PPP will poll the port for data
	// Also - don't want trace data in this mode
	return _IDLE;

// The uart is owned by a software object......

// Read in buffered bytes one at a time.
while((i < UART_BUFLEN) && ( uart_buffer_tx_out_ptr[IF_NUM] != uart_buffer_tx_in_ptr[IF_NUM] ))
	{
// ECTA hardware
	uart_buf[i++] = fardm_read(uart_buffer_tx[IF_NUM],uart_buffer_tx_out_ptr[IF_NUM]);
	uart_buffer_tx_out_ptr[IF_NUM]=circular_buffer_mask(uart_buffer_mask,uart_buffer_tx_out_ptr[IF_NUM]+1);
	};
uart_buf[i] = NULL;		// Null terminate for debug display

if(( i == 0 ) || ( pClient == NULL ))
	// No data buffered in uart, or param error
	return _BUSY;

// Owner is a sw object
switch(owner)
	{
	case OWNER_SHELL:
		// DTE talking to the console (may respond with 'i' bytes)
		pClient->receive(&pdata,&i);
		// Send the owners response to the serial port (TA/DTE/X21 port)
		while(i--)
			{
#ifdef SEB
// ECTA hardware
			uart_buffer_rx[IF_NUM][uart_buffer_rx_in_ptr[IF_NUM]]=*(pdata++);
			uart_buffer_rx_in_ptr[IF_NUM]=circular_buffer_mask(uart_buffer_mask,uart_buffer_rx_in_ptr[IF_NUM]+1);	
#else
			fardm_write(uart_buffer_rx[IF_NUM],uart_buffer_rx_in_ptr[IF_NUM],*(pdata++));
			uart_buffer_rx_in_ptr[IF_NUM]=circular_buffer_mask(uart_buffer_mask,uart_buffer_rx_in_ptr[IF_NUM]+1);
#endif
			};
	break;

	case OWNER_TCPPORT:
		// DTE (A TA) talking to a Telnet object
		// Or stdout to trace client
		pClient->serial_receive(pdata,&i);
		// Send the response to the serial port (TA/DTE/X21 port)		
		while(i--)
			{
#ifdef SEB
// ECTA hardware
			uart_buffer_rx[IF_NUM][uart_buffer_rx_in_ptr[IF_NUM]]=*(pdata++);
			uart_buffer_rx_in_ptr[IF_NUM]=circular_buffer_mask(uart_buffer_mask,uart_buffer_rx_in_ptr[IF_NUM]+1);	
#else
			fardm_write(uart_buffer_rx[IF_NUM],uart_buffer_rx_in_ptr[IF_NUM],*(pdata++));
			uart_buffer_rx_in_ptr[IF_NUM]=circular_buffer_mask(uart_buffer_mask,uart_buffer_rx_in_ptr[IF_NUM]+1);
#endif
			};
	break;

	case OWNER_UDPPORT:
	break;
	}
	
return _BUSY;
}

#endif


