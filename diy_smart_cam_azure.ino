#include "Arduino.h"
#include "esp_camera.h"
#include <WiFi.h>
#include <esp_http_client.h>
#include <base64.h>
#include <HTTPClient.h>
//#include <ArduinoJson.h>
#include "C:\Users\jonny\Documents\Arduino\libraries\ArduinoJson-5.13.5\ArduinoJson.h"
#include "soc/soc.h" // Disable brownout problems
#include "soc/rtc_cntl_reg.h" // Disable brownout problems
// Select camera model
//#define CAMERA_MODEL_WROVER_KIT // Has PSRAM
//#define CAMERA_MODEL_ESP_EYE // Has PSRAM
//#define CAMERA_MODEL_M5STACK_PSRAM // Has PSRAM
//#define CAMERA_MODEL_M5STACK_WIDE  // Has PSRAM
#define CAMERA_MODEL_AI_THINKER // Has PSRAM
//#define CAMERA_MODEL_TTGO_T_JOURNAL // No PSRAM

#include "camera_pins.h"

const char* ssid = "WiFi";
const char* password = "***";

void setup() {
  WRITE_PERI_REG(RTC_CNTL_BROWN_OUT_REG, 0);//disable brownout detector

  Serial.begin(9600);
  Serial.setDebugOutput(true);
  Serial.println();

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
  
  // if PSRAM IC present, init with UXGA resolution and higher JPEG quality
  //                      for larger pre-allocated frame buffer.
  if(psramFound()){
    config.frame_size = FRAMESIZE_QVGA;
    config.jpeg_quality = 10;
    config.fb_count = 2;
  } else {
    config.frame_size = FRAMESIZE_QVGA;
    config.jpeg_quality = 12;
    config.fb_count = 1;
  }

#if defined(CAMERA_MODEL_ESP_EYE)
  pinMode(13, INPUT_PULLUP);
  pinMode(14, INPUT_PULLUP);
#endif

  // camera init
  esp_err_t err = esp_camera_init(&config);
  if (err != ESP_OK) {
    Serial.printf("Camera init failed with error 0x%x", err);
    delay(1000);
    ESP.restart();
  }


#if defined(CAMERA_MODEL_M5STACK_WIDE)
  s->set_vflip(s, 1);
  s->set_hmirror(s, 1);
#endif

  WiFi.begin(ssid, password);

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(F("."));
  }
  Serial.println("");
  Serial.println(F("WiFi connected"));
  
  classifyImage();
 
}

void classifyImage()
{
    String response;

    // Capture picture
    camera_fb_t *fb = NULL;
    fb = esp_camera_fb_get();

    if (!fb)
    {
        Serial.println(F("Camera capture failed"));
        return;
    }
    else
    {
        Serial.println(F("Camera capture OK"));
    }

    size_t size = fb->len;
    int len = (int) fb->len;
//    uint8_t *buffer = (uint8_t *)malloc(len+1);
//    for (int i = 0; i<len; i++){
//      buffer[i] = (uint8_t) fb->buf[i];
//    }
//    buffer[len] = '\0';
    String buffer=""; // base64::encode((uint8_t *)fb->buf, fb->len);
//    buffer.resize(len);
    for (int i = 0; i < len; i++){
      buffer+= (char) fb->buf[i];
    }
    //buffer[len] = '\0';

//    uint8_t buffer[] = calloc(;

//    buffer = "";
    // Uncomment this if you want to show the payload
//    Serial.println(payloadData);general-image-recognition

    

    // Generic model 1581820110264581908ce024b12b4bfb 
    // clip text 2489aad78abf4b39a128fbbc64a8830c
    //String model_id = "aaa03c23b3724a16a56b629203edc62c";
    String endpoint = "https://mycomputervision3.cognitiveservices.azure.com/";
    String subscriptionKey = "***************************";
    String uri = endpoint + "vision/v3.2/describe";//detect";
    Serial.println(uri);

    HTTPClient http;
    http.begin(uri);
    http.addHeader("Content-Length", (String(len)).c_str());
    http.addHeader("Content-Type", "application/octet-stream"); //multipart/form-data
//    http.addHeader("Content-Type", "multipart/form-data");
    http.addHeader("Ocp-Apim-Subscription-Key", subscriptionKey);
    Serial.println(String(len));
    
//    int httpResponseCode = esp_http_client_set_post_field(http, buffer.c_str(),(len));
//    Serial.println(buffer);
//    for (int i = 0 ; i<len;i++){
//      Serial.print((char)fb->buf[i]);
//    }
//    Serial.println();
    int httpResponseCode = http.POST((buffer));
    esp_camera_fb_return(fb);
    if (httpResponseCode > 0)
    {
        Serial.print(httpResponseCode);
        Serial.print(F("Returned String: "));
        response = http.getString();
        Serial.println(response);
    }
    else
    {
        Serial.print(F("POST Error: "));
        Serial.print(httpResponseCode);
        return;
    }

    // Parse the json response: Arduino assistant
    const int jsonSize = JSON_ARRAY_SIZE(1) + JSON_ARRAY_SIZE(20) + 3 * JSON_OBJECT_SIZE(1) + 6 * JSON_OBJECT_SIZE(2) + JSON_OBJECT_SIZE(3) + 20 * JSON_OBJECT_SIZE(4) + 2 * JSON_OBJECT_SIZE(6);
     DynamicJsonBuffer jsonBuffer(jsonSize+400);

     JsonObject& doc = jsonBuffer.parseObject(response);

  if (!doc.success()) {
    Serial.println("JSON parsing failed!");
    
  }
     
//     DynamicJsonDocument doc(jsonSize+390); //here
    // deserializeJson(doc, response);

//    StaticJsonDocument<jsonSize+12> doc;
    // Deserialize the JSON document
//    DeserializationError error = deserializeJson(doc, response);
    // Test if parsing succeeds.
//    if (error)
//    {
//        Serial.print(F("deserializeJson() failed: "));
//        Serial.println(error.f_str());
//        return;
//    }
/*
    for (int i = 0; i < 10; i++)
    {
        const String name = doc["outputs"][0]["data"]["concepts"][i]["name"];
        const char *p = doc["outputs"][0]["data"]["concepts"][i]["value"];
        Serial.println("=====================");
        Serial.print(F("Name:"));
        Serial.println(name);
        Serial.print(F("Prob:"));
        Serial.println(p);
        Serial.println();
    }*/
}
void loop() {
  // put your main code here, to run repeatedly:
  vTaskDelete(NULL);
//delay(10);

}
