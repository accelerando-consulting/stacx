// esp32_cam sensor_t clashes with adafruit sensor, so need to namespace
// this library if also using sensor.
#include "JPEGDEC.h"

#include "esp_camera.h"
#include <driver/rtc_io.h>

// AI_THINKER ESP-32 cam pinout
#define PWDN_GPIO_NUM     32
#define RESET_GPIO_NUM    -1
#define XCLK_GPIO_NUM      0
#define SIOD_GPIO_NUM     26
#define SIOC_GPIO_NUM     27

#define Y9_GPIO_NUM       35
#define Y8_GPIO_NUM       34
#define Y7_GPIO_NUM       39
#define Y6_GPIO_NUM       36
#define Y5_GPIO_NUM       21
#define Y4_GPIO_NUM       19
#define Y3_GPIO_NUM       18
#define Y2_GPIO_NUM        5
#define VSYNC_GPIO_NUM    25
#define HREF_GPIO_NUM     23
#define PCLK_GPIO_NUM     22

#include "abstract_camera.h"

#ifdef __JPEGDEC__
JPEGDEC jpeg;
#endif


//
//@********************** class Esp32CamLEaf ***********************
//
// This class encapsulates an AI Thinker ESP-32 cam
//
/* Some useful info about sizes and formats:
 *
 *      Typedef enum {
 *          PIXFORMAT_RGB565,    // 0: 2BPP/RGB565
 *          PIXFORMAT_YUV422,    // 1: 2BPP/YUV422
 *          PIXFORMAT_GRAYSCALE, // 2: 1BPP/GRAYSCALE
 *          PIXFORMAT_JPEG,      // 3: JPEG/COMPRESSED
 *          PIXFORMAT_RGB888,    // 4: 3BPP/RGB888
 *          PIXFORMAT_RAW,       // 5: RAW
 *          PIXFORMAT_RGB444,    // 6: 3BP2P/RGB444
 *          PIXFORMAT_RGB555,    // 7: 3BP2P/RGB555
 *      } pixformat_t;
 *
 *      typedef enum {
 *          FRAMESIZE_QQVGA,    // 0: 160x120
 *          FRAMESIZE_QQVGA2,   // 1: 128x160
 *          FRAMESIZE_QCIF,     // 2: 176x144
 *          FRAMESIZE_HQVGA,    // 3: 240x176
 *          FRAMESIZE_QVGA,     // 4: 320x240
 *          FRAMESIZE_CIF,      // 5: 400x296
 *          FRAMESIZE_VGA,      // 6: 640x480
 *          FRAMESIZE_SVGA,     // 7: 800x600
 *          FRAMESIZE_XGA,      // 8: 1024x768
 *          FRAMESIZE_SXGA,     // 9: 1280x1024
 *          FRAMESIZE_UXGA,     //10: 1600x1200
 *          FRAMESIZE_QXGA,     //11: 2048*1536
 *          FRAMESIZE_INVALID   //12
 *      } framesize_t;
 *
 **/

static camera_config_t camera_config;

class Esp32CamLeaf : public AbstractCameraLeaf
{
protected:
  camera_config_t config;
  sensor_t *sensor = NULL;
  bool lazy_init = false;
  bool camera_ok = false;
  bool detection_enabled = false;
  bool is_enrolling = false;
  bool recognition_enabled = false;
  bool sample_enabled = false;
  unsigned long last_test = 0;
  int framesize = FRAMESIZE_QVGA;
  int pixformat = PIXFORMAT_JPEG;
  int jpeg_quality = 12;
  int test_interval_sec = 0;
  int pin_flash = -1;

  void flashOn();
  void flashOff();

public:
  Esp32CamLeaf(String name, String target, int flashPin, bool run=true) : AbstractCameraLeaf(name,target,run) {
    LEAF_ENTER(L_INFO);
    this->pin_flash = flashPin;
    LEAF_LEAVE;
  }

  virtual bool init(bool reset=false);
  virtual void setup();
  virtual void start();
  virtual void loop();
  virtual void pre_sleep(int duration=0);
  virtual void post_sleep();
  virtual bool mqtt_receive(String type, String name, String topic, String payload);
  
  virtual camera_fb_t *getImage(bool flash=false);
  virtual void returnImage(camera_fb_t *buf);
  virtual int getParameter(const char *param);
  virtual String getParameters();
  virtual bool setParameter(const char *param, int value);
  bool isReady() { return camera_ok; }
  bool isLazy() { return lazy_init; }
  void test(bool flash=false);
#ifdef __JPEGDEC__
  int draw(JPEGDRAW *img);
#endif

};

bool Esp32CamLeaf::init(bool reset)
{
  LEAF_ENTER(L_NOTICE);

  memset(&config, 0, sizeof(config));
  config.pin_pwdn = PWDN_GPIO_NUM;
  config.pin_reset = RESET_GPIO_NUM;
  config.pin_xclk = XCLK_GPIO_NUM;
  config.pin_sscb_sda = SIOD_GPIO_NUM;
  config.pin_sscb_scl = SIOC_GPIO_NUM;

  config.pin_d0 = Y2_GPIO_NUM;
  config.pin_d1 = Y3_GPIO_NUM;
  config.pin_d2 = Y4_GPIO_NUM;
  config.pin_d3 = Y5_GPIO_NUM;
  config.pin_d4 = Y6_GPIO_NUM;
  config.pin_d5 = Y7_GPIO_NUM;
  config.pin_d6 = Y8_GPIO_NUM;
  config.pin_d7 = Y9_GPIO_NUM;

  config.pin_vsync = VSYNC_GPIO_NUM;
  config.pin_href = HREF_GPIO_NUM;
  config.pin_pclk = PCLK_GPIO_NUM;

  //config.xclk_freq_hz = 10000000;
  config.xclk_freq_hz = 20000000;
  config.ledc_timer = LEDC_TIMER_0;
  config.ledc_channel = LEDC_CHANNEL_0;

  config.pixel_format = (pixformat_t)this->pixformat;
  config.frame_size = (framesize_t)this->framesize;
  config.jpeg_quality = this->jpeg_quality;
  config.fb_count = 1;
  LEAF_NOTICE("Camera pixformat %d, framesize %d quality %d", this->pixformat, this->framesize, this->jpeg_quality);

  if (wake_reason.startsWith("deepsleep")) {
    //LEAF_NOTICE("Unload any old i2c driver on wake from deep sleep");
    //int rc = i2c_driver_delete(1);
    //LEAF_NOTICE("I2C driver delete returned %d", rc);
    esp_camera_deinit();
  }

  
  if (reset) {
    // turn camera off for a moment
    if(PWDN_GPIO_NUM != -1){
      LEAF_ALERT("Cutting power to camera");
      pinMode(PWDN_GPIO_NUM, OUTPUT);
      digitalWrite(PWDN_GPIO_NUM, HIGH);
      delay(1500);
      LEAF_ALERT("Restore power to camera");
    }
  }

  // turn camera on
  if(PWDN_GPIO_NUM != -1){
    pinMode(PWDN_GPIO_NUM, OUTPUT);
    digitalWrite(PWDN_GPIO_NUM, LOW);
    delay(200);
  }

  if(psramFound()){
    LEAF_NOTICE("PSRAM present");
#if 1 // def BREAKS_CAMERA_AFTER_SLEEP
    LEAF_NOTICE("Reserving space in PSRAM for UXGA images");
    config.frame_size = FRAMESIZE_UXGA;
    config.jpeg_quality = 10;
    config.fb_count = 2;
    config.fb_location = CAMERA_FB_IN_PSRAM;
#endif
  }
  else {
    LEAF_ALERT("PSRAM not present, falling back to DRAM for framebuffer.");
    config.fb_location = CAMERA_FB_IN_DRAM;
    post_error(POST_ERROR_PSRAM, 0);
  }

  
  //LEAF_NOTICE("Testing pre-init esp_camera_sensor_get");
  //sensor = esp_camera_sensor_get();

  LEAF_NOTICE("Calling esp_camera_init");
  memcpy(&camera_config, &config, sizeof(config));
  esp_err_t err = esp_camera_init(&camera_config);
  if (err != ESP_OK) {
    LEAF_ALERT("Camera init failed with error 0x%x", err);
    LEAF_RETURN(false);
  }

  //LEAF_NOTICE("Waiting for camera to settle");
  //delay(500);

  
  LEAF_NOTICE("Testing esp_camera_sensor_get");
  sensor = esp_camera_sensor_get();
  if (!sensor) {
    LEAF_ALERT("Camera sensor read failed");
    post_error(POST_ERROR_CAMERA, 0);
    LEAF_RETURN(false);
  }
  LEAF_INFO("Camera type is %d", (int)sensor->id.PID);

#if 1 // def BREAKS_CAMERA_AFTER_SLEEP
  //drop down frame size for higher initial frame rate
  LEAF_NOTICE("Setting framesize");
  sensor->set_framesize(sensor, FRAMESIZE_QVGA);
  delay(500);
#endif
  
  LEAF_RETURN(true);
}

void Esp32CamLeaf::setup()
{
  AbstractCameraLeaf::setup();

  LEAF_ENTER(L_NOTICE);
  if (pin_flash > 0) {
    pinMode(pin_flash, OUTPUT);
    digitalWrite(pin_flash, LOW);
  }

  if (prefsLeaf) {
    getBoolPref("camera_lazy", &lazy_init, "Do not initialise camera until needed");
    getBoolPref("camera_sample", &sample_enabled, "take a sample photo at startup");
    getIntPref("camera_test_interval_sec", &test_interval_sec, "Take periodic test images");
    getIntPref("camera_framesize", &framesize, "Image size code (see esp-camera)");
    getIntPref("camera_pixformat", &pixformat, "Image pixel format code (see esp-camera)");
    getIntPref("camera_quality", &jpeg_quality, "JPEQ quality value (percent)");
  }

  if (lazy_init) {
    LEAF_NOTICE("Lazy init: camera will be initialised on demand only");
    camera_ok =false;
    return;
  }

  int retry = 1;
  int num_retry = 2;  // could be 4 to try a bit harder
  while (!camera_ok && (retry < 2)) {

    LEAF_NOTICE("Initialise camera (attempt %d)", retry);
    if (init((retry > 1))) {
      camera_ok = true;
    }
    else if (retry < 2) {
      ERROR("Camera error at retry %d.  Wait 500ms", retry);
      delay(500);
      ++retry;
    }
    else {
      // After a couple of retries, Bounce power on the camera
      ERROR("Camera error at retry %d, toggle power to camera", retry);
      pinMode(PWDN_GPIO_NUM, OUTPUT);
      digitalWrite(PWDN_GPIO_NUM, HIGH);
      post_error(POST_ERROR_CAMERA, 2); // this causes a delay
      digitalWrite(config.pin_pwdn, LOW);
      delay(500);
      ++retry;
    }
  }
  
  if (!camera_ok) {
    LEAF_ALERT("Camera not answering");
    if (wake_reason.startsWith("deepsleep")) {
      //publish("cmd/i2c_follower_action", "16"); // reset the esp
      //
      // can't use publish() here because the leaves aren't tapped yet
      //
      Leaf *control = find("app");
      if (control) {
	LEAF_ALERT("Rebooting to see if this helps after deep sleep");

	// Pretend to wake from sleep at next hard reboot
	saved_reset_reason = reset_reason;
	saved_wakeup_reason = wakeup_reason;

	//
	// Tell the application to flip the ESP's reset pin.
	// Due to the esp-cam module tying the camera and esp reset lines
	// together, this is the only way to last-ditch reset the camera.
	//
	message(control, "cmd/extreboot","1");
	// Give the control app time to receive this message
	delay(20*1000);
      }
    }
    LEAF_LEAVE;
    return;
  }

  String params = getParameters();
  LEAF_NOTICE("Camera parameters: %s", params.c_str());
  LEAF_NOTICE("  ___  __ _  _ __ ___    ___  _ __ __ _    ___  | | __");
  LEAF_NOTICE(" / __|/ _` || '_ ` _ \\  / _ \\| '__/ _` |  / _ \\ | |/ /");
  LEAF_NOTICE("| (__| (_| || | | | | ||  __/| | | (_| | | (_) ||   < ");
  LEAF_NOTICE(" \\___|\\__,_||_| |_| |_| \\___||_|  \\__,_|  \\___/ |_|\\_\\");

  LEAF_LEAVE;
}

bool Esp32CamLeaf::mqtt_receive(String type, String name, String topic, String payload)
{
    LEAF_ENTER(L_DEBUG);
    bool handled = Leaf::mqtt_receive(type, name, topic, payload);

    LEAF_INFO("RECV %s %s %s %s", type.c_str(), name.c_str(), topic.c_str(), payload.c_str());

    WHEN("cmd/camera_init", {
	bool withReset = (payload.toInt() != 0);
	init(withReset);
      })
    ELSEWHEN("cmd/camera_deinit", {
	esp_camera_deinit();
      })
    ELSEWHEN("cmd/camera_test", {
	bool withFlash = (payload.toInt() != 0);
	test(withFlash);
      })
    ELSEWHEN("cmd/camera_status", {
	if (!isReady()) {
  	  LEAF_ALERT("Camera not available");
	  mqtt_publish("status/camera", "offline");
        }
        else {
  	  mqtt_publish("status/camera", getParameters());
        }
      })
    ELSEWHEN("get/camera", {
	if (!isReady()) {
	  LEAF_ALERT("Camera not available");
	  mqtt_publish("status/camera", "offline");
	}
	else {
	  mqtt_publish("status/camera", String(getParameter(payload.c_str())));
	}
      })
    ELSEWHEN("set/camera", {
	if (!isReady()) {
	  LEAF_ALERT("Camera not available");
	  mqtt_publish("status/camera", "offline");
	}
	else {
	  int pos = payload.indexOf("=");
	  if (pos > 0) {
	    String param = payload.substring(0, pos);
	    String value = payload.substring(pos+1);
	    bool ok = setParameter(param.c_str(), value.toInt());
	    mqtt_publish("status/camera", TRUTH(ok));
	  }
	}
      })

    LEAF_LEAVE;
    RETURN(handled);
  }


#ifdef __JPEGDEC__
int Esp32CamLeaf::draw(JPEGDRAW *img)
{
  //LEAF_NOTICE("draw at [%d,%d] size [%d,%d] depth %d", img->x, img->y, img->iWidth, img->iHeight, img->iBpp);
  static const char *greyscales = "$@B%8&WM#*oahkbdpqwmZO0QLCJUYXzcvunxrjft/\\|()1{}[]?-_+~<>i!lI;:,\"^`'. ";

  uint8_t *pixels = (uint8_t *)img->pPixels;
  for (int j=0;j<img->iHeight;j++) {
    if ((img->y+j)%2) continue;
    for (int i=0;i<img->iWidth;i++) {
      uint8_t p = pixels[(j*img->iWidth)+i];
      char c = map(p,0,255,0,69);
      if (c>=70) {
	LEAF_ALERT("you fucked up and got index of %d", (int)c);
      }
      //LEAF_INFO("[%d,%d]=%u => %u", i, j, (unsigned int)p, (unsigned int)c);
      Serial.print(greyscales[c]);
    }
    Serial.println();
  }

  return 1;
}
#endif

void Esp32CamLeaf::test(bool flash)
{
  // Take a sample picture, and print it in ascii-art
  debug_flush=true;
  LEAF_NOTICE("Taking a sample image (%s flash)",flash?"with":"no");
  camera_fb_t *fb = getImage(flash);
  if (fb) {
    LEAF_NOTICE("Sample image taken");
    Serial.flush();
#ifdef __JPEGDEC__
    if (jpeg.openRAM(fb->buf, fb->len,
		     [](JPEGDRAW *pDraw)->int{
		       Esp32CamLeaf *that = (Esp32CamLeaf *)Leaf::get_leaf_by_type(leaves, String("camera"));
		       if (that) {
			 return that->draw(pDraw);
		       }
		       return 1;
		     })) {
      LEAF_INFO("Successfully opened JPEG image");
      int width = jpeg.getWidth();
      LEAF_INFO("Image size: %d x %d, orientation: %d, bpp: %d\n", width,
		jpeg.getHeight(), jpeg.getOrientation(), jpeg.getBpp());
      int scale = JPEG_SCALE_EIGHTH;
      if (width <= 160) {
	scale = JPEG_SCALE_HALF;
      }
      else if (width <= 320) {
	scale = JPEG_SCALE_QUARTER;
      }
      if (jpeg.hasThumb()) {
	LEAF_INFO("Thumbnail present: %d x %d\n", jpeg.getThumbWidth(), jpeg.getThumbHeight());
      }
      jpeg.setPixelType(EIGHT_BIT_GRAYSCALE);
      long lTime = micros();
      if (jpeg.decode(0,0,scale))
      {
	lTime = micros() - lTime;
	LEAF_INFO("Successfully decoded image in %d us", (int)lTime);
      }
      else {
	LEAF_ALERT("JPEG decode failed");
      }
      jpeg.close();
    }
#endif

    returnImage(fb);
  }
  else {
    LEAF_ALERT("Sample image FAILED");
  }
  debug_flush=false;

}

void Esp32CamLeaf::start()
{
    if (sample_enabled) {
      test(false);
    }
}



void Esp32CamLeaf::loop()
{
  Leaf::loop();
  unsigned long now = millis();
  if (test_interval_sec && ((last_test==0) || (now >= (last_test + test_interval_sec*1000)))) {
    last_test = now;
    this->test();
  }
}

void Esp32CamLeaf::flashOn()
{
  if (pin_flash > 0) {
    digitalWrite(pin_flash, HIGH);
    delay(150);
  }
}

void Esp32CamLeaf::flashOff()
{
  if (pin_flash > 0) {
    digitalWrite(pin_flash, LOW);
  }
}

void Esp32CamLeaf::pre_sleep(int duration)
{
  LEAF_ENTER(L_NOTICE);

  // Turn the flash off and hold it that way
  flashOff();
  gpio_hold_en((gpio_num_t)pin_flash);
  gpio_deep_sleep_hold_en();
  //rtc_gpio_hold_en((gpio_num_t)rtc_io_number_get((gpio_num_t)pin_flash));

  esp_camera_deinit();
  digitalWrite(PWDN_GPIO_NUM, HIGH);
  gpio_hold_en((gpio_num_t)PWDN_GPIO_NUM);
  //rtc_gpio_hold_en((gpio_num_t)rtc_io_number_get(PWDN_GPIO_NUM));

  // Turn the camera off and hold it that way
  /*
  if (sensor) {
    sensor->reset(sensor);
  }
  */

  LEAF_LEAVE;
}

void Esp32CamLeaf::post_sleep()
{
  LEAF_ENTER(L_NOTICE);

  LEAF_NOTICE("reenable flash after sleep");
  pinMode(pin_flash, OUTPUT);
  digitalWrite(pin_flash, LOW);
  gpio_hold_dis((gpio_num_t)pin_flash);

  gpio_hold_dis((gpio_num_t)PWDN_GPIO_NUM);
  //rtc_gpio_hold_dis((gpio_num_t)rtc_io_number_get(PWDN_RTC_GPIO_NUM));
  //pinMode(PWDN_GPIO_NUM, OUTPUT);
  //digitalWrite(PWDN_GPIO_NUM, LOW);

  LEAF_LEAVE;
}

camera_fb_t *Esp32CamLeaf::getImage(bool flash) {
  LEAF_ENTER(L_NOTICE);

  if (!camera_ok) {
    LEAF_ALERT("Camera is not ready, try initialising it now.");
    init();
    if (!camera_ok) {
      LEAF_ALERT("Darn it, the camera is sulking again. No photo for you.");
      return NULL;
    }
  }
  sensor = esp_camera_sensor_get();

  camera_fb_t * fb = NULL;
  int64_t fr_start = esp_timer_get_time();

  if (flash) flashOn();
  fb = esp_camera_fb_get();
  if (flash) flashOff();
  if (!fb) {
    LEAF_ALERT("Camera capture failed");
    LEAF_LEAVE;
    return NULL;
  }

  size_t out_len, out_width, out_height;
  uint8_t * out_buf;
  bool s;
  bool detected = false;
  int face_id = 0;

  if(fb->format != PIXFORMAT_JPEG){
    LEAF_ALERT("Non-JPEG image chunking not implemented yet");
    /*
    jpg_chunking_t jchunk = {req, 0};
    res = frame2jpg_cb(fb, 80, jpg_encode_stream, &jchunk)?ESP_OK:ESP_FAIL;
    httpd_resp_send_chunk(req, NULL, 0);
    fb_len = jchunk.len;
    */
    esp_camera_fb_return(fb);
    return NULL;
  }
  int64_t fr_end = esp_timer_get_time();
  LEAF_NOTICE("JPG captured: %uB %ums", (uint32_t)(fb->len), (uint32_t)((fr_end - fr_start)/1000));
  LEAF_LEAVE;
  return fb;
}

void Esp32CamLeaf::returnImage(camera_fb_t *buf)
{
  LEAF_ENTER(L_NOTICE);
  esp_camera_fb_return(buf);
  LEAF_LEAVE;
}


int Esp32CamLeaf::getParameter(const char *param) {

  if (strcasecmp(param, "framesize")==0) return sensor->status.framesize;
  if (strcasecmp(param, "quality")==0) return sensor->status.quality;
  if (strcasecmp(param, "quality")==0) return sensor->status.quality;
  if (strcasecmp(param, "brightness")==0) return sensor->status.brightness;
  if (strcasecmp(param, "contrast")==0) return sensor->status.contrast;
  if (strcasecmp(param, "saturation")==0) return sensor->status.saturation;
  if (strcasecmp(param, "sharpness")==0) return sensor->status.sharpness;
  if (strcasecmp(param, "special_effect")==0) return sensor->status.special_effect;
  if (strcasecmp(param, "wb_mode")==0) return sensor->status.wb_mode;
  if (strcasecmp(param, "awb")==0) return sensor->status.awb;
  if (strcasecmp(param, "awb_gain")==0) return sensor->status.awb_gain;
  if (strcasecmp(param, "aec")==0) return sensor->status.aec;
  if (strcasecmp(param, "aec2")==0) return sensor->status.aec2;
  if (strcasecmp(param, "ae_level")==0) return sensor->status.ae_level;
  if (strcasecmp(param, "aec_value")==0) return sensor->status.aec_value;
  if (strcasecmp(param, "agc")==0) return sensor->status.agc;
  if (strcasecmp(param, "agc_gain")==0) return sensor->status.agc_gain;
  if (strcasecmp(param, "gainceiling")==0) return sensor->status.gainceiling;
  if (strcasecmp(param, "bpc")==0) return sensor->status.bpc;
  if (strcasecmp(param, "wpc")==0) return sensor->status.wpc;
  if (strcasecmp(param, "raw_gma")==0) return sensor->status.raw_gma;
  if (strcasecmp(param, "lenc")==0) return sensor->status.lenc;
  if (strcasecmp(param, "vflip")==0) return sensor->status.vflip;
  if (strcasecmp(param, "hmirror")==0) return sensor->status.hmirror;
  if (strcasecmp(param, "dcw")==0) return sensor->status.dcw;
  if (strcasecmp(param, "colorbar")==0) return sensor->status.colorbar;
  if (strcasecmp(param, "face_detect")==0) return detection_enabled;
  if (strcasecmp(param, "face_enroll")==0) return is_enrolling;
  if (strcasecmp(param, "face_recognize")==0) return recognition_enabled;

  LEAF_ALERT("Unknown parameter [%s]", param);
  return -1;
}

String Esp32CamLeaf::getParameters()
{
    static char json_response[1024];

    if (sensor == NULL) {
      return String("{\"error\": \"Camera Sensor Error\"}");
    }
    char * p = json_response;
    *p++ = '{';

    p+=sprintf(p, "\"framesize\":%u,", sensor->status.framesize);
    p+=sprintf(p, "\"quality\":%u,", sensor->status.quality);
    p+=sprintf(p, "\"brightness\":%d,", sensor->status.brightness);
    p+=sprintf(p, "\"contrast\":%d,", sensor->status.contrast);
    p+=sprintf(p, "\"saturation\":%d,", sensor->status.saturation);
    p+=sprintf(p, "\"sharpness\":%d,", sensor->status.sharpness);
    p+=sprintf(p, "\"special_effect\":%u,", sensor->status.special_effect);
    p+=sprintf(p, "\"wb_mode\":%u,", sensor->status.wb_mode);
    p+=sprintf(p, "\"awb\":%u,", sensor->status.awb);
    p+=sprintf(p, "\"awb_gain\":%u,", sensor->status.awb_gain);
    p+=sprintf(p, "\"aec\":%u,", sensor->status.aec);
    p+=sprintf(p, "\"aec2\":%u,", sensor->status.aec2);
    p+=sprintf(p, "\"ae_level\":%d,", sensor->status.ae_level);
    p+=sprintf(p, "\"aec_value\":%u,", sensor->status.aec_value);
    p+=sprintf(p, "\"agc\":%u,", sensor->status.agc);
    p+=sprintf(p, "\"agc_gain\":%u,", sensor->status.agc_gain);
    p+=sprintf(p, "\"gainceiling\":%u,", sensor->status.gainceiling);
    p+=sprintf(p, "\"bpc\":%u,", sensor->status.bpc);
    p+=sprintf(p, "\"wpc\":%u,", sensor->status.wpc);
    p+=sprintf(p, "\"raw_gma\":%u,", sensor->status.raw_gma);
    p+=sprintf(p, "\"lenc\":%u,", sensor->status.lenc);
    p+=sprintf(p, "\"vflip\":%u,", sensor->status.vflip);
    p+=sprintf(p, "\"hmirror\":%u,", sensor->status.hmirror);
    p+=sprintf(p, "\"dcw\":%u,", sensor->status.dcw);
    p+=sprintf(p, "\"colorbar\":%u,", sensor->status.colorbar);
    p+=sprintf(p, "\"face_detect\":%u,", detection_enabled);
    p+=sprintf(p, "\"face_enroll\":%u,", is_enrolling);
    p+=sprintf(p, "\"face_recognize\":%u", recognition_enabled);
    *p++ = '}';
    *p++ = 0;

    return String(json_response);
}

bool Esp32CamLeaf::setParameter(const char *param, int val) {

  int res = 0;

  if (sensor == NULL) {
    LEAF_ALERT("Sensor not available");
    return false;
  }

  if(!strcmp(param, "framesize")) {
    if(sensor->pixformat == PIXFORMAT_JPEG) res = sensor->set_framesize(sensor, (framesize_t)val);
  }
  else if(!strcmp(param, "quality")) res = sensor->set_quality(sensor, val);
  else if(!strcmp(param, "contrast")) res = sensor->set_contrast(sensor, val);
  else if(!strcmp(param, "brightness")) res = sensor->set_brightness(sensor, val);
  else if(!strcmp(param, "saturation")) res = sensor->set_saturation(sensor, val);
  else if(!strcmp(param, "gainceiling")) res = sensor->set_gainceiling(sensor, (gainceiling_t)val);
  else if(!strcmp(param, "colorbar")) res = sensor->set_colorbar(sensor, val);
  else if(!strcmp(param, "awb")) res = sensor->set_whitebal(sensor, val);
  else if(!strcmp(param, "agc")) res = sensor->set_gain_ctrl(sensor, val);
  else if(!strcmp(param, "aec")) res = sensor->set_exposure_ctrl(sensor, val);
  else if(!strcmp(param, "hmirror")) res = sensor->set_hmirror(sensor, val);
  else if(!strcmp(param, "vflip")) res = sensor->set_vflip(sensor, val);
  else if(!strcmp(param, "awb_gain")) res = sensor->set_awb_gain(sensor, val);
  else if(!strcmp(param, "agc_gain")) res = sensor->set_agc_gain(sensor, val);
  else if(!strcmp(param, "aec_value")) res = sensor->set_aec_value(sensor, val);
  else if(!strcmp(param, "aec2")) res = sensor->set_aec2(sensor, val);
  else if(!strcmp(param, "dcw")) res = sensor->set_dcw(sensor, val);
  else if(!strcmp(param, "bpc")) res = sensor->set_bpc(sensor, val);
  else if(!strcmp(param, "wpc")) res = sensor->set_wpc(sensor, val);
  else if(!strcmp(param, "raw_gma")) res = sensor->set_raw_gma(sensor, val);
  else if(!strcmp(param, "lenc")) res = sensor->set_lenc(sensor, val);
  else if(!strcmp(param, "special_effect")) res = sensor->set_special_effect(sensor, val);
  else if(!strcmp(param, "wb_mode")) res = sensor->set_wb_mode(sensor, val);
  else if(!strcmp(param, "ae_level")) res = sensor->set_ae_level(sensor, val);
  else if(!strcmp(param, "face_detect")) {
    detection_enabled = val;
    if(!detection_enabled) {
      recognition_enabled = 0;
    }
  }
  else if(!strcmp(param, "face_enroll")) is_enrolling = val;
  else if(!strcmp(param, "face_recognize")) {
    recognition_enabled = val;
    if(recognition_enabled){
      detection_enabled = val;
    }
  }
  else {
    res = -1;
  }
  if (res!=0) {
    LEAF_ALERT("Set parameter (%s=%d) error %d", param, val, res);
  }
  return (res==0)?true:false;
}

// Local Variables:
// mode: C++
// c-basic-offset: 2
// End:

