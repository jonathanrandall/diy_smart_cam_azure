#include "Arduino.h"
#include "esp_camera.h"
#include <WiFi.h>
#include <AsyncTCP.h>
#include <ESPAsyncWebServer.h>
#include "img_converters.h"
#include "image_util.h"
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
#include "ssid_stuff.h"
#include "camera_index.h"
#include "Free_Fonts.h"
#include "SPIFFS.h"
#include <fb_gfx.h>
#include <EEPROM.h>

#define EEPROM_SIZE 1
#define TEXT "starting app..."
/*
 * in file ssid_stuff.h
const char* ssid = "WiFi";
const char* password = "***";
*/

AsyncWebServer webserver(80);
AsyncWebSocket ws("/ws");

#include <SPI.h>
#include <TFT_eSPI.h>
TFT_eSPI tft = TFT_eSPI();
#include <TFT_eFEX.h>
TFT_eFEX  fex = TFT_eFEX(&tft);

const int push_button = 4;
bool is_start = true;
bool stream_or_display = true;
#include "Free_Fonts.h" // Include the header file attached to this sketch

const int jsonSize = JSON_ARRAY_SIZE(1) + JSON_ARRAY_SIZE(20) + 3 * JSON_OBJECT_SIZE(1) + 6 * JSON_OBJECT_SIZE(2) + JSON_OBJECT_SIZE(3) + 20 * JSON_OBJECT_SIZE(4) + 2 * JSON_OBJECT_SIZE(6);
DynamicJsonBuffer jsonBuffer(jsonSize+400);
String filelist;
camera_fb_t * fb = NULL;
String incoming;
String response;

uint8_t pictureNumber;
dl_matrix3du_t *rgb888_matrix = NULL;


bool init_wifi()
{
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.print("Ready! Use 'http://");
  Serial.print(WiFi.localIP());
  Serial.println("' to connect");
  
  delay(1000);
  return true;
}

void init_camera(){
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
}


void setup() {
  WRITE_PERI_REG(RTC_CNTL_BROWN_OUT_REG, 0);//disable brownout detector

  Serial.begin(115200);

  pinMode(push_button, INPUT);;// initialize io4 as an output for LED flash.
  digitalWrite(push_button, LOW); // flash off/

  //SPIFFS
  if (!SPIFFS.begin(true)) {
    Serial.println("SPIFFS initialisation failed!");
    ESP.restart();
    SPIFFS.begin(true);// Formats SPIFFS - could lose data https://github.com/espressif/arduino-esp32/issues/638
  }
  
  init_wifi();
  init_camera();
  EEPROM.begin(EEPROM_SIZE);
  pictureNumber = (uint8_t) EEPROM.read(0) + 1;

  SPI.begin(TFT_SCLK,TFT_MISO,TFT_MOSI,TFT_CS);
  tft.begin();
  tft.setRotation(3);  // 0 & 2 Portrait. 1 & 3 landscape
  tft.fillScreen(TFT_BLACK);
  tft.setCursor(35,55);
  tft.setTextColor(TFT_WHITE);
  tft.setTextSize(1);
//  tft.println(WiFi.localIP());
  delay(500);

  tft.setTextDatum(MC_DATUM);

  // Set text colour to orange with black background
  tft.setTextColor(TFT_WHITE, TFT_BLACK);
  
  tft.fillScreen(TFT_BLACK);            // Clear screen
  tft.setFreeFont(FSS12);                 // Select the font
  tft.drawString(sFF1, 160, 60, GFXFF);// Print the string name of the font
  tft.setFreeFont(FF1);                 // Select the font
  tft.drawString(TEXT, 160, 120, GFXFF);// Print the string name of the font
//  tft.drawString(String(WiFi.localIP()).c_str(), 160, 180, GFXFF);
  tft.setCursor(50, 180, 2);
  tft.println(WiFi.localIP());
  
  tft.setTextColor(TFT_GREEN, TFT_BLACK);    tft.setTextFont(4);
  
  delay(1000);
  delay(1000);
  ws.onEvent(onEvent);
  webserver.addHandler(&ws);

  webserver.on("/", HTTP_GET, [](AsyncWebServerRequest * request) {
    Serial.print("Sending interface...");
    AsyncWebServerResponse *response = request->beginResponse_P(200, "text/html", index_ov2640_html_gz, sizeof(index_ov2640_html_gz));
    response->addHeader("Content-Encoding", "gzip");
    request->send(response);
  });

  webserver.on("/image", HTTP_GET, [](AsyncWebServerRequest * request) {
    Serial.println("requesting image from SPIFFS");
    if (request->hasParam("id")) {
      AsyncWebParameter* p = request->getParam("id");
      Serial.printf("GET[%s]: %s\n", p->name().c_str(), p->value().c_str());
      String imagefile = p->value();
      imagefile = imagefile.substring(4); // remove img_
      request->send(SPIFFS, "/" + imagefile);
    }
  });
  webserver.serveStatic("/", SPIFFS, "/");

  webserver.begin();

  fex.listSPIFFS(); // Lists the files so you can see what is in the SPIFFS
 
}

void onEvent(AsyncWebSocket * server, AsyncWebSocketClient * client, AwsEventType type, void * arg, uint8_t *data, size_t len)
{
  // String incoming = String((char *)data); No idea why.. gave extra characters in data for short names.
  // so ....
  for (size_t i = 0; i < len; i++) {
    incoming += (char)(data[i]);
  }
  Serial.println(incoming);

  if (incoming.substring(0, 7) == "delete:") {
    String deletefile = incoming.substring(7);
    incoming = "";
    int fromUnderscore = deletefile.lastIndexOf('_') + 1;
    int untilDot = deletefile.lastIndexOf('.');
    String fileId = deletefile.substring(fromUnderscore, untilDot);
    Serial.println(fileId);
    Serial.println("image delete");
    SPIFFS.remove("/selfie_t_" + fileId + ".jpg");
    SPIFFS.remove("/selfie_f_" + fileId + ".jpg");
    client->text("removed:" + deletefile); // file deleted. request browser update
    
  } else {
    Serial.println("sending list");
    client->text(filelist_spiffs());
  }
}

String filelist_spiffs()
{

  filelist = "";
  fs::File root = SPIFFS.open("/");

  fs::File file = root.openNextFile();
  while (file) {
    String fileName = file.name();
    // Serial.println(fileName);
    filelist = filelist + fileName;
    file = root.openNextFile();
  }
  Serial.println(filelist);
  return filelist;
}

void latestFileSPIFFS()
{
  fs::File root = SPIFFS.open("/");

  fs::File file = root.openNextFile();
  while (file) {
    String fileName = file.name();
    Serial.println(fileName);
    int fromUnderscore = fileName.lastIndexOf('_') + 1;
    int untilDot = fileName.lastIndexOf('.');
    String fileId = fileName.substring(fromUnderscore, untilDot);
    Serial.println(fileId);
//    file_number = max(file_number, fileId.toInt()); // replace filenumber if fileId is higher
    file = root.openNextFile();
  }
}



void classifyImage()
{
    

    // Capture picture
    
//    fb = esp_camera_fb_get();

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

    String buffer=""; // base64::encode((uint8_t *)fb->buf, fb->len);
//    buffer.resize(len);
    for (int i = 0; i < len; i++){
      buffer+= (char) fb->buf[i];
    }
    
   
    String endpoint = "https://mycomputervision3.cognitiveservices.azure.com/";
    String subscriptionKey = "***";
    String uri = endpoint + "vision/v3.2/describe";//detect";
    Serial.println(uri);

    HTTPClient http;
    http.begin(uri);
    http.addHeader("Content-Length", (String(len)).c_str());
    http.addHeader("Content-Type", "application/octet-stream"); //multipart/form-data
//    http.addHeader("Content-Type", "multipart/form-data");
    http.addHeader("Ocp-Apim-Subscription-Key", subscriptionKey);
    Serial.println(String(len));
    
    
    
    int httpResponseCode = http.POST((buffer));
//    esp_camera_fb_return(fb);
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
    JsonObject& doc = jsonBuffer.parseObject(response);

  if (!doc.success()) {
    Serial.println("JSON parsing failed!");
    
  }
     

/*
 * https://westus.dev.cognitive.microsoft.com/docs/services/computer-vision-v3-2/operations/56f91f2e778daf14a499f21f
 {
  "description": {
    "tags": [
      "person",
      "man",
      "outdoor",
      "window",
      "glasses"
    ],
    "captions": [
      {
        "text": "Satya Nadella sitting on a bench",
        "confidence": 0.48293603002174407
      },
      {
        "text": "Satya Nadella is sitting on a bench",
        "confidence": 0.40037006815422832
      },
      {
        "text": "Satya Nadella sitting in front of a building",
        "confidence": 0.38035155997373377
      }
    ]
  },
  "requestId": "ed2de1c6-fb55-4686-b0da-4da6e05d283f",
  "metadata": {
    "width": 1500,
    "height": 1000,
    "format": "Jpeg"
  },
  "modelVersion": "2021-04-01"
}
 */

    int i = 0;
    while(true){
      const char *tags = doc["description"]["tags"][i++];

    if(tags==nullptr) break; // Serial.println("null charachter");
    Serial.println(tags);

    }
    const char *text = doc["description"]["captions"][0]["text"];
    Serial.println(text);
    
}

void rgb_print(dl_matrix3du_t *image_matrix, uint32_t color, const char * str, int y){
               fb_data_t fb;
               fb.width = image_matrix->w;
               fb.height = image_matrix->h;
               fb.data = image_matrix->item;
               fb.bytes_per_pixel = 3;
               fb.format = FB_BGR888;
//               fb_gfx_print(&fb, (fb.width - (strlen(str) * 14)) / 2, y, color, str);
               fb_gfx_print(&fb, (2+ (strlen(str) * 4)) / 2, y, color, str);
}

void print_to_image_and_tft(){
  int i = 0;
  JsonObject& doc = jsonBuffer.parseObject(response);
  //x, y, font
  tft.setCursor(4, 2, 2);
  while(true){
    const char *tags = doc["description"]["tags"][i++];
    if(tags==nullptr) break; // Serial.println("null charachter");
    tft.println(tags);
    rgb_print(rgb888_matrix, 0x000000FF, tags, i*15 + 2);
    if(i==5) break;
  }
  tft.setCursor(4, 220, 2);
  const char *text = doc["description"]["captions"][0]["text"];
  tft.print(text);
  rgb_print(rgb888_matrix, 0x000000FF, text, fb->height-15);
  
}

void loop() {
  int push_button_state  = digitalRead(push_button);
//delay(10);
  if(stream_or_display) {  //take photo and output to tft.
    fb = esp_camera_fb_get();
    if (!fb) {
      Serial.println("Camera capture failed");
    }
    fex.drawJpg((const uint8_t*)fb->buf, fb->len, 0, 6);
    esp_camera_fb_return(fb);
  }

  if(push_button_state){
    delay(200);
    stream_or_display = !stream_or_display;
  
  if(!stream_or_display){
  //take photo and classify.
    Serial.println("Capture image");
    fb = esp_camera_fb_get();
    if (!fb) {
      Serial.println("Camera capture failed");
    }
    fex.drawJpg((const uint8_t*)fb->buf, fb->len, 0, 6);
    classifyImage();
 
  
    char *full_filename = (char*)malloc(23 + sizeof(pictureNumber));
    sprintf(full_filename, "/spiffs/selfie_f_%d.jpg", pictureNumber);
  
    FILE *fullres = fopen(full_filename, "w");
     Serial.println(sizeof((fb->buf)[1]));
  //   tft.readRectRGB(0, 0, 320*240, 1, fb->buf);
    if (fullres != NULL)  {
      size_t err = fwrite(fb->buf, 1, fb->len, fullres);
      Serial.printf("File saved: %s\n", full_filename);
    }  else  {
      Serial.println("Could not open file"); 
    }
    fclose(fullres);
  
    rgb888_matrix = dl_matrix3du_alloc(1, fb->width, fb->height, 3);
    fmt2rgb888(fb->buf, fb->len, fb->format, rgb888_matrix->item);

    print_to_image_and_tft();

    Serial.println("printing to image");
    size_t _jpg_buf_len = 0;
    uint8_t * _jpg_buf = NULL;

 
    bool jpeg_converted = fmt2jpg(rgb888_matrix->item, fb->width*fb->height*3, fb->width, fb->height, PIXFORMAT_RGB888, 80, &_jpg_buf, &_jpg_buf_len); //&fb->buf
    Serial.println("converted jpg");
    dl_matrix3du_free(rgb888_matrix);
  
    //more file stuff
  
    full_filename = (char*)malloc(23 + sizeof(pictureNumber));
    sprintf(full_filename, "/spiffs/selfie_t_%d.jpg", pictureNumber);
    fullres = fopen(full_filename, "w");

    if (fullres != NULL)  {
      size_t err = fwrite(_jpg_buf, 1, _jpg_buf_len, fullres);
      free(_jpg_buf);
      Serial.printf("File saved: %s\n", full_filename);
    }  else  {
      Serial.println("Could not open file"); 
    }
    fclose(fullres);
    esp_camera_fb_return(fb);
    fb = NULL;
    free(full_filename);
  //  delay(5000);
    delay(500);
    
    pictureNumber++;
    EEPROM.write(0, pictureNumber);
    EEPROM.commit();

    ws.cleanupClients();
    } //if(!stream

  }//if(push_button_state


}//loop
