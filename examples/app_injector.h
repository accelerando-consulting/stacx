//
// This class implements the display logic for an injection molding thermostat
//
#pragma once

#include "abstract_app.h"
#include "trait_pid.h"
#include "trait_polynomial.h"

//RTC_DATA_ATTR int saved_something = 0;

#define HEATER_COUNT 4

class InjectorAppLeaf : public AbstractAppLeaf, public Pid, public Polynomial
{
protected: // configuration preferences
  float temperature_setpoint=0; // this is the sole "public" input
  int max_setpoint = 300;
  int cycle_length = 2000;

protected: // ephemeral state
  // these are measurements of state
  bool use_pid=true;
  int rawTemp;
  float temperature;
  bool heaters[HEATER_COUNT];
  unsigned long cycle_start[HEATER_COUNT];

  // this is an output the PID loop 
  int power = 0;
  double raw_power = 0;

#if USE_OLED
  OledLeaf *screen;
#endif
  
public:
  InjectorAppLeaf(String name, String target)
    : AbstractAppLeaf(name,target) {
    LEAF_ENTER(L_INFO);
    this->target=target;
    this->setpoint = temperature_setpoint;
    // default variables or constructor argument processing goes here
    LEAF_LEAVE;
  }

  virtual void setup(void) {
    AbstractAppLeaf::setup();
    LEAF_ENTER(L_INFO);
#if USE_OLED
    screen = (OledLeaf *)get_tap("screen");
#endif
    for (int h=0; h<HEATER_COUNT; h++) {
      cycle_start[h] = cycle_length*h/HEATER_COUNT;
      LEAF_NOTICE("Heater %d cycle_start=%lu", h+1, cycle_start[h]);
    }

    max_setpoint = getIntPref("injector_m", max_setpoint);

    float initial_t = getFloatPref("injector_t", 0);
    if (initial_t > max_setpoint) {
      initial_t = max_setpoint;
    }
    setpoint = temperature_setpoint = initial_t;
    LEAF_NOTICE("Initial temperature setpoint %.1fC", temperature_setpoint);

    cycle_length = getIntPref("injector_c", cycle_length);
    LEAF_NOTICE("Initial heater cycle length %.3f seconds", (float)cycle_length/1000);

    //
    // Load the polynomial coefficients for the cubic interpolation of thermistor ADC -> Celsius
    //
    double co_3 = getFloatPref("injector_co3", -5.842e-9);
    double co_2 = getFloatPref("injector_co2", 4.293e-5);
    double co_1 = getFloatPref("injector_co1", -0.1393);
    double co_0 = getFloatPref("injector_co0", 306.3);
    LEAF_NOTICE("Initial thermistor conversion coefficients y=%gx^3 + %gx^2 + %gx + %g",
		co_3, co_2, co_1, co_0);
    polynomial_setup(3, co_3, co_2, co_1, co_0);
    
    //
    // Load the PID constants for the temperature regulation
    //
    float initial_p = getFloatPref("injector_p", 7);
    float initial_i = getFloatPref("injector_i", 2);
    float initial_d = getFloatPref("injector_d", 1);
    LEAF_NOTICE("Initial PID constants p=%.1f i=%.1f d=%.1f", initial_p, initial_i, initial_d);
    pid_setup(initial_p, initial_i, initial_d);
    pid->SetOutputLimits(0,100);

    LEAF_LEAVE;
  }

  virtual void start(void) 
  {
    LEAF_ENTER(L_NOTICE);
    int h;
    
    for (h=0; h<HEATER_COUNT; h++) {
      heaters[h] = false;
      LEAF_NOTICE("Set heater #%d to OFF initially", h+1);
      String target = String("heater")+(h+1);
      message(target, "cmd/off");
      message(target, "set/do_status", "0");
    }
    LEAF_LEAVE;
  }
  
  void display_update() 
  {
    static char cmd[128];
    char when[16];
    time_t now = time(NULL);

#if USE_OLED
    LEAF_DEBUG("display_update");
    if (!screen) return;
    
    message(screen, "cmd/clear", "");
   
    snprintf(cmd, sizeof(cmd), "{\"font\":16,\"align\":\"center\",\"row\": 0,\"column\": 64,\"text\":\"Pwr %d%% %c %c %c %c\"}", power, (heaters[0]?'1':'-'), (heaters[1]?'2':'-'), (heaters[2]?'3':'-'), (heaters[3]?'4':'-'));
    message(screen, "cmd/draw", cmd);

    snprintf(cmd, sizeof(cmd), "{\"font\":24,\"align\":\"center\",\"row\": 20,\"column\": 64,\"text\":\"%.1fC\"}", temperature);
    message(screen, "cmd/draw", cmd);

    snprintf(cmd, sizeof(cmd), "{\"font\":16,\"align\":\"center\",\"row\": 48,\"column\": 64,\"text\":\"Target %dC\"}", (int)temperature_setpoint);
    message(screen, "cmd/draw", cmd);

#endif
  }

  void status_pub() 
  {
    static unsigned long last_status = 0;
    unsigned long now = millis();
    if (now < (last_status + 1000)) {
      // rate limit status updates
      return;
    }
    last_status = now;
    char buf[40];

    mqtt_publish("status/setpoint", String((int)temperature_setpoint));
    mqtt_publish("status/power", String((int)power));
    mqtt_publish("status/temperature", String((int)temperature));

    snprintf(buf, sizeof(buf), "[ %.2f, %.2f, %.2f ]", (float)pid->GetKp(), (float)pid->GetKi(), (float)pid->GetKd());
    mqtt_publish("status/pid", buf);

    snprintf(buf, sizeof(buf), "[ %s, %s, %s, %s ]", truth(heaters[0]), truth(heaters[1]), truth(heaters[2]), truth(heaters[3]));
    mqtt_publish("status/heaters", buf);
  }
  

  bool mqtt_receive(String type, String name, String topic, String payload)
  {
    LEAF_ENTER(L_DEBUG);
    bool handled = Leaf::mqtt_receive(type, name, topic, payload);

    LEAF_INFO("RECV %s %s %s %s", type.c_str(), name.c_str(), topic.c_str(), payload.c_str());

    WHEN("set/heater", {
	float v = payload.toFloat();
	if (v > max_setpoint) v=max_setpoint;
	this->temperature_setpoint = this->setpoint = v;

	this->use_pid=true;
      heater_compute();
      })
    ELSEWHENFROM("thermocouple", "status/temperature", {
	float new_temp = payload.toFloat();;
	if (abs(new_temp-temperature)>0.1) {
	  LEAF_INFO("Thermocouple reading is %.1fC", new_temp);
	  temperature = new_temp;
	}
	heater_compute();
      })
    ELSEWHENFROM("ntc", "status/raw", {
	rawTemp=payload.toInt();
	double new_temp = polynomial_compute(rawTemp);
	if (abs(new_temp-temperature)>0.1) {
	  LEAF_INFO("Raw NTC reading %d converted to temperature %.1fC", rawTemp, new_temp);
	  temperature = new_temp;
	  heater_compute();
	}
      })
    ELSEWHENFROM("heater1", "status/light", {
	heaters[0]=(payload=="lit");
	display_update();
      })
    ELSEWHENFROM("heater2", "status/light", {
	heaters[1]=(payload=="lit");
	display_update();
      })
    ELSEWHENFROM("heater3", "status/light", {
	heaters[2]=(payload=="lit");
	display_update();
      })
    ELSEWHENFROM("heater4", "status/light", {
	heaters[3]=(payload=="lit");
	display_update();
      })
    ELSEWHEN("set/power", {
	power = payload.toInt()%100;
	this->use_pid=false;
	heater_compute();
      })
    else {
      LEAF_DEBUG("app did not consume type=%s name=%s topic=%s payload=%s", type.c_str(), name.c_str(), topic.c_str(), payload.c_str());
    }

    LEAF_LEAVE;
    RETURN(handled);
  }

  void heater_compute() 
  {
    LEAF_ENTER(L_DEBUG);
    if (!this->use_pid) {
      LEAF_INFO("Manual power level at %d, skip PID computation", power);
      LEAF_VOID_RETURN;
    }
    
    double new_power = raw_power;
    if (setpoint <= 50) {
      LEAF_DEBUG("Setpoint effectively zero, heaters should all be off");
      new_power = 0;
    }
    else {
      if (this->pid_compute(temperature, &new_power)) {
	LEAF_INFO("heater compute temp=%.1f setpoint=%.1f => output %.1f", setpoint, temperature, new_power);
      }
    }

    // Only update if there is a significant change
    if (abs(new_power-raw_power)>=1) {
      //LEAF_NOTICE("Adjust power level %d=>%d to move current temp %.1fC toward setpoint %.1fC", power, (int)new_power, temperature, setpoint);
      // save the PID output so we can detect significant changes
      raw_power = new_power;
      
      // clip the effective power to [0,100]
      if (new_power > 100) new_power=100;
      if (new_power < 0) new_power=0;
      power = new_power;

      heater_update();
      display_update();
      status_pub();
    }
    LEAF_LEAVE;
    
  }

  void heater_update() 
  {
    //LEAF_ENTER(L_DEBUG);
    // Given our desired power level P, each heater should be on for P% of a 10 second cycle,
    // however heaters should be run out of phase so that, eg  when power level is below
    // 25%, only one heater would ever be on at a time
    bool change = false;
    for (int h=0; h<HEATER_COUNT; h++) {
      int heater_num = h+1;
      unsigned long now = millis();
      if (now < cycle_start[h]) {
	// it is not time to consider this heater's state yet
	//LEAF_NOTICE("Current time (%lu) is before the cycle start time (%lu) of heater %d", now, cycle_start[h], heater_num);
	continue;
      }
      unsigned long cycle_pos = now - cycle_start[h];
      while (cycle_pos >= cycle_length) {
	// It is time to advance to the next cycle for this heater
	//LEAF_NOTICE("Outdated cycle status for heater %d at time %lu: start=%lu pos=%lu", h+1, now, cycle_start[h], cycle_pos);
	cycle_start[h] += cycle_length;	
	cycle_pos -= cycle_length;
	//LEAF_NOTICE("Corrected phase position for heater %d to start=%lu pos=%lu", h+1, cycle_start[h], cycle_pos);
      }
      unsigned long cycle_cutoff = floor(cycle_length * power / 100.0);

      if (!heaters[h] && (cycle_pos < cycle_cutoff)) {
	// heater H is supposed to be on, make it so
	LEAF_DEBUG("Heater %d power=%d cycle_start=%lu cycle_pos=%lu cycle_cutoff=%lu => ON", heater_num, power, cycle_start[h], cycle_pos, cycle_cutoff);
	LEAF_INFO("Turn heater %d ON (at power=%d)", heater_num, power);
	heaters[h] = true;
	message(String("heater")+heater_num, "cmd/on");
	change=true;
      } else if (heaters[h] && (cycle_pos >= cycle_cutoff)) {
	// heater H is supposed to be off, make it so
	LEAF_DEBUG("Heater %d power=%d cycle_start=%d cycle_pos=%d cycle_cutoff=%d => OFF", heater_num, power, cycle_start[h], cycle_pos, cycle_cutoff);
	LEAF_INFO("Turn heater %d OFF (at power=%d)", heater_num, power);
	heaters[h] = false;
	message(String("heater")+heater_num, "cmd/off");
	change=true;
	//
	// There are no more events to consider for this cycle, we could do nothing
	// until next cycle.  BUT if the power level changes, we should
	// react immediately, so do not advance the cycle start here.
	//
	// could be: cycle_start[h] += cycle_length;
      }
    }
    if (change) {
      display_update();
      status_pub();
    }
    //LEAF_LEAVE;
  }
  
  virtual void loop(void)
  {
    Leaf::loop();
    static unsigned long last_heater_update = 0;
    static unsigned long last_status_update = 0;
    unsigned long now = millis();

    if (now >= (last_heater_update+ (cycle_length/100)) ) {
      // check heaters 100x per cycle
      last_heater_update = now;
      heater_compute();
    }
    else {
      heater_update();
    }

    if (now >= (last_status_update+10000)) {
      LEAF_NOTICE("setpoint=%d power=%d temp=%d", (int)temperature_setpoint, (int)power, (int)temperature);
      last_status_update=now;
      status_pub();
      display_update();
    }
      
    
  }
 

};

// local Variables:
// mode: C++
// c-basic-offset: 2
// End:
