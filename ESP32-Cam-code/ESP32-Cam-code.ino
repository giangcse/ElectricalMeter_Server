#include "WiFi.h"
#include "esp_camera.h"
#include "esp_timer.h"
#include "img_converters.h"
#include "Arduino.h"
#include "soc/soc.h"           // Disable brownour problems
#include "soc/rtc_cntl_reg.h"  // Disable brownour problems
#include "driver/rtc_io.h"
#include <StringArray.h>
#include <SPIFFS.h>
#include <FS.h>
#include <HTTPClient.h>
#include <Base64.h>
#include <WiFiManager.h>
#include <PubSubClient.h>

const String url = "https://30ac-113-161-196-10.ngrok-free.app/upload";
const char* mqtt_server = "giangpt-hass.duckdns.org";
const int mqtt_port = 1883;
const char* mqtt_user = "giang";
const char* mqtt_password = "giang";
const char* mqtt_topic_capture = "esp32_cam_2F4A58/capture";
const char* mqtt_topic_info = "esp32_cam_2F4A58/info";

WiFiClient espClient;
PubSubClient client(espClient);
boolean takeNewPhoto = false;

// Photo File Name to save in SPIFFS
#define FILE_PHOTO "/photo.jpg"

// OV2640 camera module pins (CAMERA_MODEL_AI_THINKER)
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

void callback(char* topic, byte* payload, unsigned int length) {
  // Handle incoming MQTT messages
  String command = "";
  for (int i = 0; i < length; i++) {
    command += (char)payload[i];
  }

  if (command.equals("capture")) {
    takeNewPhoto = true;
  }
}

void sendDeviceInfo() {
  // Get MAC address of the ESP32
  uint8_t mac[6];
  WiFi.macAddress(mac);

  // Convert MAC address to a string
  String macAddress;
  for (int i = 0; i < 6; ++i) {
    macAddress += String(mac[i], 16);
    if (i < 5) macAddress += ":";
  }

  // Create JSON payload for device info
  String payload = "{\"mac\":\"" + macAddress + "\",\"ip\":\"" + WiFi.localIP().toString().c_str() + "\"}";

  // Publish the device info to the MQTT topic
  client.publish(mqtt_topic_info, payload.c_str());
}

void reconnect() {
  while (!client.connected()) {
    Serial.print("Attempting MQTT connection...");
    if (client.connect("ESP32-Cam", mqtt_user, mqtt_password)) {
      Serial.println("connected");
      client.subscribe(mqtt_topic_capture);

      // Send device info upon successful connection
      sendDeviceInfo();
    } else {
      Serial.print("failed, rc=");
      Serial.print(client.state());
      Serial.println(" try again in 5 seconds");
      delay(5000);
    }
  }
}

void setup() {
  // Serial port for debugging purposes
  Serial.begin(115200);

  // Connect to Wi-Fi
  WiFiManager wm;
  
  bool res;
  res = wm.autoConnect("ESP32-Cam", "12345678");
  if (!res) {
    Serial.println("Failed to connect");
    ESP.restart();
  } else {
    Serial.print("IP Address: ");
    Serial.println(WiFi.localIP());
  }

  if (!SPIFFS.begin(true)) {
    Serial.println("An Error has occurred while mounting SPIFFS");
    ESP.restart();
  }
  else {
    delay(500);
    Serial.println("SPIFFS mounted successfully");
  }

  SPIFFS.format();
  // Turn-off the 'brownout detector'
  WRITE_PERI_REG(RTC_CNTL_BROWN_OUT_REG, 0);

  // OV2640 camera module
  camera_config_t config;
  config.ledc_channel = LEDC_CHANNEL_0;
  config.ledc_timer = LEDC_TIMER_0;
  config.pin_d0 = Y2_GPIO_NUM;
  config.pin_d1 = Y3_GPIO_NUM;
  config.pin_d2 = Y4_GPIO_NUM;
  config.pin_d3 = Y5_GPIO_NUM;
  config.pin_d4 = Y6_GPIO_NUM;
  config.pin_d5 = Y7_GPIO_NUM;
  config.pin_d6 = Y8_GPIO_NUM;
  config.pin_d7 = Y9_GPIO_NUM;
  config.pin_xclk = XCLK_GPIO_NUM;
  config.pin_pclk = PCLK_GPIO_NUM;
  config.pin_vsync = VSYNC_GPIO_NUM;
  config.pin_href = HREF_GPIO_NUM;
  config.pin_sscb_sda = SIOD_GPIO_NUM;
  config.pin_sscb_scl = SIOC_GPIO_NUM;
  config.pin_pwdn = PWDN_GPIO_NUM;
  config.pin_reset = RESET_GPIO_NUM;
  config.xclk_freq_hz = 20000000;
  config.pixel_format = PIXFORMAT_JPEG;

  if (psramFound()) {
    config.frame_size = FRAMESIZE_UXGA;
    config.jpeg_quality = 10;
    config.fb_count = 2;
  } else {
    config.frame_size = FRAMESIZE_SVGA;
    config.jpeg_quality = 12;
    config.fb_count = 1;
  }
  // Camera init
  esp_err_t err = esp_camera_init(&config);
  if (err != ESP_OK) {
    Serial.printf("Camera init failed with error 0x%x", err);
    ESP.restart();
  }

  // Connect to MQTT
  client.setServer(mqtt_server, mqtt_port);
  client.setCallback(callback);
}

void loop() {
  // Handle MQTT connections
  if (!client.connected()) {
    reconnect();
  }
  client.loop();
  
  static unsigned long lastCaptureTime = 0;
  const unsigned long captureInterval = 60000*5; // 60 seconds

  if (takeNewPhoto) {
    capturePhotoSaveSpiffs();
    sendPhotoToServer();
    takeNewPhoto = false;
  }

  unsigned long currentMillis = millis();
  if (currentMillis - lastCaptureTime >= captureInterval) {
    capturePhotoSaveSpiffs();
    sendPhotoToServer();
    lastCaptureTime = currentMillis;
  }

  delay(1);
}


// Check if photo capture was successful
bool checkPhoto( fs::FS &fs ) {
  File f_pic = fs.open( FILE_PHOTO );
  unsigned int pic_sz = f_pic.size();
  return ( pic_sz > 100 );
}

void deleteOldPhoto() {
  // Delete the old photo file
  if (SPIFFS.exists(FILE_PHOTO)) {
    SPIFFS.remove(FILE_PHOTO);
    Serial.println("Old photo file deleted");
  }
}


// Capture Photo and Save it to SPIFFS
void capturePhotoSaveSpiffs( void ) {
  camera_fb_t * fb = NULL; // pointer
  bool ok = 0; // Boolean indicating if the picture has been taken correctly

  do {
    // Take a photo with the camera
    Serial.println("Taking a photo...");

    fb = esp_camera_fb_get();
    if (!fb) {
      Serial.println("Camera capture failed");
      return;
    }

    // Delete the old photo file
    deleteOldPhoto();

    // Photo file name
    Serial.printf("Picture file name: %s\n", FILE_PHOTO);
    File file = SPIFFS.open(FILE_PHOTO, FILE_WRITE);

    // Insert the data in the photo file
    if (!file) {
      Serial.println("Failed to open file in writing mode");
    }
    else {
      file.write(fb->buf, fb->len); // payload (image), payload length
      Serial.print("The picture has been saved in ");
      Serial.print(FILE_PHOTO);
      Serial.print(" - Size: ");
      Serial.print(file.size());
      Serial.println(" bytes");
    }
    // Close the file
    file.close();
    esp_camera_fb_return(fb);

    // check if file has been correctly saved in SPIFFS
    ok = checkPhoto(SPIFFS);
  } while ( !ok );
}

void sendPhotoToServer() {
  // Get MAC address of the ESP32
  uint8_t mac[6];
  WiFi.macAddress(mac);

  // Convert MAC address to a string
  String macAddress;
  for (int i = 0; i < 6; ++i) {
    macAddress += String(mac[i], 16);
    if (i < 5) macAddress += ":";
  }
  HTTPClient http;
  http.begin(url);
  http.addHeader("Accept", "*/*");

  // Add IP and MAC address form data
  http.POST("mac=" + macAddress);

  // Read image file from SPIFFS and send it as form data
  File imageFile = SPIFFS.open(FILE_PHOTO, "r");
  if (imageFile) {
    http.POST("image=@" + String(FILE_PHOTO));
    imageFile.close();
  }

  http.end();
}
