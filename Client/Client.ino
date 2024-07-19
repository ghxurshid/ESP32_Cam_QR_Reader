//>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>> 01 ESP32 Cam QR Code Scanner
/*
   Reference :
   - ESP32-CAM QR Code Reader (off-line) : https://www.youtube.com/watch?v=ULZL37YqJc8
   - ESP32QRCodeReader_Page : https://github.com/fustyles/Arduino/tree/master/ESP32-CAM_QRCode_Recognition/ESP32QRCodeReader_Page

   The source of the "quirc" library I shared on this project: https://github.com/fustyles/Arduino/tree/master/ESP32-CAM_QRCode_Recognition/ESP32QRCodeReader_Page
*/

/* Including the libraries. */
#include <WiFi.h>
#include <WiFiClient.h>

#include "esp_camera.h"
#include "soc/soc.h"
#include "soc/rtc_cntl_reg.h"
#include "quirc.h"

/* creating a task handle */
TaskHandle_t QRCodeReader_Task;

/* Select camera model */
//#define CAMERA_MODEL_WROVER_KIT
//#define CAMERA_MODEL_ESP_EYE
//#define CAMERA_MODEL_M5STACK_PSRAM
//#define CAMERA_MODEL_M5STACK_WITHOUT_PSRAM
//#define CAMERA_MODEL_M5STACK_WITHOUT_PSRAM
#define CAMERA_MODEL_AI_THINKER


/* GPIO of camera models */
#if defined(CAMERA_MODEL_AI_THINKER)
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
#else
#error "Camera model not selected"
#endif

#define LED1_PIN 13
#define LED2_PIN 12

struct QRCodeData
{
  bool valid;
  int dataType;
  uint8_t payload[1024];
  int payloadLen;
};

int zx;
struct quirc *q = NULL;
uint8_t *image = NULL;
camera_fb_t * fb = NULL;
struct quirc_code code;
struct quirc_data data;
quirc_decode_error_t err;
struct QRCodeData qrCodeData;

String QRCodeResult = "";
String QRCodeTemplate = "";

const char *ssid = "PLUM TECHNOLOGIES";          // Change this to your WiFi SSID
const char *password = "455855454";  // Change this to your WiFi password

const char *host = "192.168.4.1";        // This should not be changed
const int httpPort = 5784;                        // This should not be changed

static WiFiClient reader_client;

void ErrorHandle(char* msg)
{
  Serial.println(msg);
  while (1) {
    digitalWrite(LED2_PIN, HIGH);
    delay(200);
    digitalWrite(LED2_PIN, LOW);
    delay(200);
  }
}

void setup() {
  // Disable brownout detector.
  WRITE_PERI_REG(RTC_CNTL_BROWN_OUT_REG, 0);

  /* Init serial communication speed (baud rate). */
  Serial.begin(115200);
  Serial.setDebugOutput(true);
  Serial.println();

  pinMode(LED1_PIN, OUTPUT);
  pinMode(LED2_PIN, OUTPUT);

  // We start by connecting to a WiFi network

  Serial.println();
  Serial.println("******************************************************");
  Serial.println("Connecting to WiFi");

  WiFi.begin(ssid, password);

  int attempCount = 20;
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
    if (attempCount -- < 0) ErrorHandle("FATAL: Can't connect to wifi:(");
  }

  Serial.println("");
  Serial.println("WiFi connected");
  Serial.println("IP address: ");
  Serial.println(WiFi.localIP());

  // Connect client to the host
  if (!reader_client.connect(host, httpPort)) {
    ErrorHandle("FATAL: Can't connect to the host:(");
  } else {
    unsigned long timeout = millis();
    while (reader_client.available() == 0) {
      if (millis() - timeout > 5000) {
        Serial.println(">>> Client Timeout !");
        break;
      }
    }

    // Read all the lines of the reply from server and print them to Serial
    if (reader_client.available()) {
      QRCodeTemplate = reader_client.readStringUntil('\r');
    } else {
      ErrorHandle("FATAL: Can't get template data from host:(");
    }
  }

  /* Camera configuration. */
  Serial.println("Start configuring and initializing the camera...");
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
  config.xclk_freq_hz = 30000000;
  config.pixel_format = PIXFORMAT_GRAYSCALE;
  config.frame_size = FRAMESIZE_QVGA;
  config.jpeg_quality = 15;
  config.fb_count = 1;

#if defined(CAMERA_MODEL_ESP_EYE)
  pinMode(13, INPUT_PULLUP);
  pinMode(14, INPUT_PULLUP);
#endif

  esp_err_t err = esp_camera_init(&config);
  if (err != ESP_OK) {
    Serial.printf("Camera init failed with error 0x%x", err);
    ESP.restart();
  }

  sensor_t * s = esp_camera_sensor_get();
  s->set_framesize(s, FRAMESIZE_QVGA);

  Serial.println("Configure and initialize the camera successfully.");
  Serial.println();

  /* create "QRCodeReader_Task" using the xTaskCreatePinnedToCore() function */
  xTaskCreatePinnedToCore(
    QRCodeReader,          /* Task function. */
    "QRCodeReader_Task",   /* name of task. */
    10000,                 /* Stack size of task */
    NULL,                  /* parameter of the task */
    1,                     /* priority of the task */
    &QRCodeReader_Task,    /* Task handle to keep track of created task */
    0);                    /* pin task to core 0 */

  pinMode (4, OUTPUT);
}

void loop() {
  // put your main code here, to run repeatedly:
  delay(1);
}

/* The function to be executed by "QRCodeReader_Task" */
// This function is to instruct the camera to take or capture a QR Code image, then it is processed and translated into text.
void QRCodeReader( void * pvParameters ) {
  Serial.println("QRCodeReader is ready.");
  Serial.print("QRCodeReader running on core ");
  Serial.println(xPortGetCoreID());
  Serial.println();

  /* ----- Loop to read QR Code in real time. ----------*/
  while (1) {
    q = quirc_new();
    if (q == NULL) {
      Serial.print("can't create quirc object\r\n");
      continue;
    }

    fb = esp_camera_fb_get();
    if (!fb)
    {
      Serial.println("Camera capture failed");
      continue;
    }

    quirc_resize(q, fb->width, fb->height);
    image = quirc_begin(q, NULL, NULL);
    memcpy(image, fb->buf, fb->len);
    quirc_end(q);

    int count = quirc_count(q);
    if (count > 0) {
      quirc_extract(q, 0, &code);
      err = quirc_decode(&code, &data);

      if (err) {
        Serial.println("Decoding FAILED");
        QRCodeResult = "Decoding FAILED";
      } else {
        Serial.printf("Decoding successful:\n");
        dumpData(&data);

        if (QRCodeResult == QRCodeTemplate) {
          reader_client.println(QRCodeResult);
        }
      }
      Serial.println();
    }

    esp_camera_fb_return(fb);
    fb = NULL;
    image = NULL;
    quirc_destroy(q);
  }
}

/* Function to display the results of reading the QR Code on the serial monitor. */
void dumpData(const struct quirc_data *data)
{
  Serial.printf("Version: %d\n", data->version);
  Serial.printf("ECC level: %c\n", "MLHQ"[data->ecc_level]);
  Serial.printf("Mask: %d\n", data->mask);
  Serial.printf("Length: %d\n", data->payload_len);
  Serial.printf("Payload: %s\n", data->payload);
  zx++;
  Serial.print(zx);
  QRCodeResult = (const char *)data->payload;
}


