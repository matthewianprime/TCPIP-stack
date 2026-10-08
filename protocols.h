// protocols.h
// Base class definition used by LCP, PAP, CHAP etc
// Used to define protocol entry points
// Abstraction of the protent structure used in BSD

struct fsm;		// Forward declaration allowing use in CProtocol

class CProtocol : public CObj
{
public:
const char *name;			// text protocol name
int16 protocol;				// PPP protocol number

// Initialisation proc
virtual void init(int);
// Process a received packet 
virtual void input(int, unsigned char*,int);
// Process a received protocol-reject 
virtual void protrej(int);
// Lower layer has come up 
virtual void lowerup(int);
// Lower layer has gone down 
virtual void lowerdown(int);
// Open the protocol 
virtual void open (int unit);
// Close the protocol 
virtual void close (int unit, const char *reason);
// Process a received data packet 
virtual void datainput (int unit, unsigned char *pkt, int len);
int  enabled_flag;		// 0 if protocol is disabled 
// Check requested options, assign defaults 
virtual void check_options(void);
// Configure interface for demand-dial 
int demand_conf(int unit);
// Say whether to bring up link for this pkt 
virtual int active_pkt(unsigned char *pkt, int len);
virtual void resetci (fsm *);						// Reset our CI
virtual int  cilen (fsm *);							// Return length of our CI
virtual void addci (fsm *, unsigned char *, int *);	// Add our CI to pkt
virtual int  ackci (fsm *, unsigned char *, int);	// Peer ack'd our CI
virtual int  nakci (fsm *, unsigned char *, int);	// Peer nak'd our CI
virtual int  rejci (fsm *, unsigned char *, int);	// Peer rej'd our CI
virtual int  reqci (fsm *, unsigned char *, int *, int); // Rcv peer CI
virtual void up (fsm *);		// We're UP
virtual void down (fsm *);		// We're DOWN
virtual void starting (fsm *);	// We need lower layer up
virtual void finished (fsm *);	// We need lower layer down
virtual int  extcode (fsm *, int, int, unsigned char *, int);
virtual void rprotrej (fsm *, unsigned char *, int);
};
