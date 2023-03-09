#pragma once
#include "abstract_ip_modem.h"

//
//@*********************** class IpEportE10 *************************
//
// This class encapsulates an eport TTL-to-ethernet bridge (itself a capable
// ARM microcontroller)
//

class IpEportE10Leaf : public AbstractIpModemLeaf
{
public:
  IpEportE10Leaf(String name, String target, int uart,int rxpin, int txpin,  int baud=115200, uint32_t options=SERIAL_8N1, int8_t pwrpin=-1, int8_t keypin=-1, int8_t sleeppin=-1, bool run = true) : AbstractIpModemLeaf(name,target,uart,rxpin,txpin,baud,options,pwrpin,keypin,sleeppin,run) {
  }

protected:

};



// Local Variables:
// mode: C++
// c-basic-offset: 2
// End:
