//@**************************** class MPU6050Leaf ******************************
// 
// This class encapsulates the MPU6050 6-axis IMS sensor
//

#include <Wire.h>
#include "leaf_ims_abstract.h"

class MPU6050Leaf : public AbstractIMSLeaf, public WireNode
{
public:
  bool found;
  bool use_gyro = false;

  MPU6050Leaf(String name, byte i2c_address = 0x68) : AbstractIMSLeaf(name, 0) {
    LEAF_ENTER(L_INFO);
    found = false;
    changed = false;
    address = i2c_address;
    decimal_places = 1;
    this->sample_interval_ms = 500;
    this->report_interval_sec = 900;
    accel_x = accel_y = accel_z = 0;
    rawAccX = rawAccY = rawAccZ = rawGyroX = rawGyroY = rawGyroZ = 0;
    gyroXOffset = 259;
    gyroYOffset = 231;
    gyroZOffset = -17;
    angGyroX = angGyroY = angGyroZ = angAccX = angAccY = 0;

    LEAF_LEAVE;
  }

  virtual void setup();
  virtual bool poll();
  virtual void status_pub();

protected:
  static const byte reg_smplrt_div = 0x19;
  static const byte reg_config = 0x1a;
  static const byte reg_gyro_config = 0x1b;
  static const byte reg_accel_config = 0x1c;
  static const byte reg_pwr_mgmt_1 = 0x6b;

  static const byte reg_accel_xout = 0x3b;
  static const byte reg_gyro_xout = 0x43;

  static constexpr float ACCEL_TRANSFORMATION_NUMBER = 0.00006103515; // (1 / 16384) precalculated
  static constexpr float  GYRO_TRANSFORMATION_NUMBER = 0.01525878906; // (1 / 65.536) precalculated

  static constexpr float DEFAULT_ACCEL_COEFF = 0.02;
  static constexpr float DEFAULT_GYRO_COEFF =  0.98;
  
  static const int DISCARDED_MEASURES = 100;
  static const int CALIBRATION_MEASURES =5000;
  static const int CHECKING_MEASURES=50;
  static const int ACCEL_PREOFFSET_MAGIC_NUMBER=8;
  static const int GYRO_PREOFFSET_MAGIC_NUMBER=4;
  
  float gyroXOffset, gyroYOffset, gyroZOffset;

  // Raw accel and gyro data
  int16_t rawAccX, rawAccY, rawAccZ, rawGyroX, rawGyroY, rawGyroZ;

  // Readable accel and gyro data (but use abstract parent's accel_x/y/z instead!)
  float accX, accY, accZ, gyroX, gyroY, gyroZ;
  float delta_x = 0.16;
  float delta_y = 0.16;
  float delta_z = 0.16;
  float delta_ang = 3;
  
  // Integration interval stuff
  long intervalStart;
  float dt;

  // Angle data according to accel and gyro (separately)
  float angGyroX, angGyroY, angGyroZ, angAccX, angAccY;

  // Complememtary filter accel and gyro coefficients
  float filterAccelCoeff, filterGyroCoeff;

  void UpdateRawAccel ();
  void UpdateRawGyro ();

};

void MPU6050Leaf::setup(void) {
    AbstractIMSLeaf::setup();

    LEAF_ENTER(L_NOTICE);
    wire = &Wire;
    address = 0x68;

    // Probe the address
    Wire.beginTransmission(address);
    int error = Wire.endTransmission();
    if (error == 0)
    {
      LEAF_NOTICE("MPU6050 device responding at address 0x%02x", address);
      found=true;
    }
    else {
      LEAF_ALERT("MPU6050 device not responding at address 0x%02x", address);
      found=false;
      return;
    }

    // Setting attributes with default values
    filterAccelCoeff = DEFAULT_ACCEL_COEFF;
    filterGyroCoeff = DEFAULT_GYRO_COEFF;

    // Setting sample rate divider
    write_register(reg_smplrt_div, 0x00);

    // Setting frame synchronization and the digital low-pass filter
    write_register(reg_config, 0x00);

    // Setting gyro self-test and full scale range
    write_register(reg_gyro_config, 0x08);

    // Setting accelerometer self-test and full scale range
    write_register(reg_accel_config, 0x00);

    // Waking up MPU6050
    write_register(reg_pwr_mgmt_1, 0x01);

    if (use_gyro) {
      // Setting angles to zero
      tilt_x = 0;
      tilt_y = 0;
    }
    

    // Beginning integration timer
    intervalStart = millis();

    LEAF_LEAVE;
}

/*
 *	Raw accel data update method
 */
void MPU6050Leaf::UpdateRawAccel () {
  LEAF_ENTER(L_DEBUG);
  
  // Beginning transmission for MPU6050
  wire->beginTransmission(address);

  // Accessing accel data registers
  wire->write(reg_accel_xout);
  wire->endTransmission(false);

  // Requesting accel data
  wire->requestFrom(address, (byte)6); //,(int) true);

  // Storing raw accel data
  rawAccX = wire->read() << 8;
  rawAccX |= wire->read();

  rawAccY = wire->read() << 8;
  rawAccY |= wire->read();
  
  rawAccZ = wire->read() << 8;
  rawAccZ |= wire->read();

  LEAF_DEBUG("UpdateRawAccel rawAcc[]=[%d,%d,%d]", (int)rawAccX, (int)rawAccY, (int)rawAccZ);
  
  LEAF_LEAVE;
  
}

/*
 *	Raw gyro data update method
 */
void MPU6050Leaf::UpdateRawGyro () {
  LEAF_ENTER(L_DEBUG);
  
  // Beginning transmission for MPU6050
  wire->beginTransmission(address);
  
  // Accessing gyro data registers
  wire->write(reg_gyro_xout);
  wire->endTransmission(false);
  
  // Requesting gyro data
  wire->requestFrom(address, (byte)6); //, (int) true);
  
  // Storing raw gyro data
  rawGyroX = wire->read() << 8;
  rawGyroX |= wire->read();
  
  rawGyroY = wire->read() << 8;
  rawGyroY |= wire->read();
  
  rawGyroZ = wire->read() << 8;
  rawGyroZ |= wire->read();

  LEAF_DEBUG("UpdateRawGyro rawGyro[]=[%d,%d,%d]", (int)rawGyroX, (int)rawGyroY, (int)rawGyroZ);

  LEAF_LEAVE;
}

void MPU6050Leaf::poll() 
{
  //LEAF_ENTER(L_INFO);

  // Updating raw data before processing it
  UpdateRawAccel();

  // Computing readable accel/gyro data
  accX = (float)(rawAccX) * ACCEL_TRANSFORMATION_NUMBER;
  accY = (float)(rawAccY) * ACCEL_TRANSFORMATION_NUMBER;
  accZ = (float)(rawAccZ) * ACCEL_TRANSFORMATION_NUMBER;

  bool result = false;
  if (abs(accel_x - accX) > delta_x) {
    result = true;
    accel_x = accX;
  }
  if (abs(accel_y - accY) > delta_y) {
    result = true;
    accel_y = accY;
  }
  if (abs(accel_z - accZ) > delta_z) {
    result = true;
    accel_z = accZ;
  }
  
  LEAF_DEBUG("accel[]=[%4.f,%4.f,%.4f] (%s)",accX, accY, accZ, changed?"CHANGED":"no change");

  // Computing accel angles
  angAccX = wrap((atan2(accY, sqrt(accZ * accZ + accX * accX))) * RAD_TO_DEG);
  angAccY = wrap((-atan2(accX, sqrt(accZ * accZ + accY * accY))) * RAD_TO_DEG);
  LEAF_DEBUG("angAcc[]=[%4.f,%4.f]",angAccX, angAccY);
  
  if (use_gyro) {
    UpdateRawGyro();
    gyroX = (float)(rawGyroX - gyroXOffset) * GYRO_TRANSFORMATION_NUMBER;
    gyroY = (float)(rawGyroY - gyroYOffset) * GYRO_TRANSFORMATION_NUMBER;
    gyroZ = (float)(rawGyroZ - gyroZOffset) * GYRO_TRANSFORMATION_NUMBER;
    LEAF_DEBUG("gyro[]=[%.4f, %.4f,%.4f]", gyroX, gyroY, gyroZ);
    
    // Computing gyro angles
    dt = (millis() - intervalStart) * 0.001;
    angGyroX = wrap(angGyroX + gyroX * dt);
    angGyroY = wrap(angGyroY + gyroY * dt);
    angGyroZ = wrap(angGyroZ + gyroZ * dt);
    
    // Computing complementary filter angles
    float angX = angle_average(filterAccelCoeff, angAccX, filterGyroCoeff, angX + gyroX * dt);
    float angY = angle_average(filterAccelCoeff, angAccY, filterGyroCoeff, angY + gyroY * dt);
    float angZ = angGyroZ;
    LEAF_INFO("Angles = [%.4.2f, %.4.2f, %.4.2f]", angX, angY, angZ);

    // TODO: delta filtering for gyro mode
    if (isnan(tilt_x) || (abs(tilt_x-angX) > delta_ang)) {
      changed=true;
      tilt_x = angX;
    }
    if (isnan(tilt_y) || (abs(tilt_y-angY) > delta_ang)) {
      changed=true;
      tilt_y = angY;
    }
    
    // Reseting the integration timer
    intervalStart = millis();
  }
  else {

    if (isnan(tilt_x) || (abs(tilt_x-angAccX) > delta_ang)) {
      changed=true;
      tilt_x = angAccX;
    }
    if (isnan(tilt_y) || (abs(tilt_y-angAccY) > delta_ang)) {
      changed=true;
      tilt_y = angAccY;
    }
  }

  //LEAF_LEAVE;
  return result;
}

void MPU6050Leaf::status_pub() 
{
  if (!found) return;
  
  AbstractIMSLeaf::status_pub();
  char msg[64];
  /*
  snprintf(msg, sizeof(msg), "%.2f", angX);
  publish("xtilt", msg);
  snprintf(msg, sizeof(msg), "%.2f", angY);
  publish("ytilt", msg);
  */

  snprintf(msg, sizeof(msg), "[%.2f, %.2f]", angX, angY);
  mqtt_publish("status/orientation", msg);
}




// local Variables:
// mode: C++
// c-basic-offset: 2
// End:
