#pragma once
#include "abstract_ip_simcom.h"

//
//@*********************** class IpSimcomSim7000 *************************
//
// This class encapsulates a simcom nbiot/catm1 modem
//

class IpSimcomSim7000 : public AbstractIpSimcomLeaf
{
public:
  IpSimcomSim7000(String name, String target, int uart,int rxpin, int txpin,  int baud=115200, uint32_t options=SERIAL_8N1, int8_t pwrpin=-1, int8_t keypin=-1, int8_t sleeppin=-1, bool run = true) : AbstractIpSimcomLeaf(name,target,uart,rxpin,txpin,baud,options,pwrpin,keypin,sleeppin,run) {
  }

protected:

};



// Local Variables:
// mode: C++
// c-basic-offset: 2
// End:
