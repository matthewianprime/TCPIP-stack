// object.cpp
// Base class for CProtocol_L3,CProtocol,CClient
// Implements message queue and handler routines

#include "router.h"
#include "pppd.h"
#include "utils.h"

CObj::CObj(){}

CObj::~CObj()
// Cancel all callbacks before deletion
{
router->untimeout(this,MSG_ALL);
}

void CObj::timeout(int32 lParam,int32 time)
// Schedule a callback to OnMessage function.
// time in ms
{
struct tagMESSAGE message;
message.msg = MSG_TIMER;				// Type of mesage -TIMER
message.lParam = lParam;				// Timer message parameter
message.wParam = 0;
message.time = time;
message.pObj = this;
router->timeout(&message);
}

bool CObj::untimeout(int32 messg, CObj* pObj)
// Cancel all callback timers with matching lParam (MSG_ constant) to this or a specified object
{
if(messg == 0)
	messg = MSG_ALL;

if(pObj == NULL)	
	return router->untimeout(this,messg);

return router->untimeout(pObj,messg);
}

void CObj::PostMessage(CObj* pObj,int16 messg,int32 lParam,int32 wParam,int16 zParam,int32 time)
// Send a message to another class, after a specified time lapse
{

tagMESSAGE message;
message.msg = messg;
message.lParam = lParam;				// Type of mesage
message.wParam = wParam;				// Message parameter
message.zParam = zParam;				// Message parameter
message.time = time;
message.pObj = pObj;
router->timeout(&message);
}

void CObj::PeekMessage(int32,int32){}
void CObj::OnMessage(int16,int32,int32,int16){}

