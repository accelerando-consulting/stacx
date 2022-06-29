#pragma once
#include "abstract_ip_simcom.h"

//
//@*********************** class IpSimcomM2MLeaf *************************
//
// This class encapsulates a simcom nbiot/catm1 modem
//

class IpSimcomM2MLeaf : public AbstractIpSimcomLeaf
{
public:
  IpSimcomM2MLeaf(String name, String target, int uart,int rxpin, int txpin,  int baud=115200, uint32_t options=SERIAL_8N1, int8_t pwrpin=-1, int8_t keypin=-1, int8_t sleeppin=-1, bool run = true) : AbstractIpSimcomLeaf(name,target,uart,rxpin,txpin,baud,options,pwrpin,keypin,sleeppin,run) {
  }

protected:

};



// Local Variables:
// mode: C++
// c-basic-offset: 2
// End:
