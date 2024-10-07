/*
  Tesselator (tessie) node for ESP32
  https://github.com/invpe/Tesselator
*/
#include <WiFi.h>
#include <WiFiUdp.h>
#include <WebServer.h>
#include <ArduinoOTA.h>
#include "SPIFFS.h"
#include "loader.h"

#define WIFI_SSID "COMPUTING"
#define WIFI_PASS "TESSIE1911COMP"
#define BINARY_FILE "/task_binary"
#define INPUT_FILE "/task_input"
#define OUTPUT_FILE "/task_output"
#define UDP_PORT 1911
#define BROADCAST_TIMER 5000

TimerHandle_t periodicTimer;
WiFiUDP udp;
WebServer WWWServer(80);
bool bBusy = false;
uint32_t uiTotalExecuted = 0;
File binaryFile;
File payloadFile;
String strArgument;

// Load task binary directly from file
unsigned char* LoadELFFile(const char* path, size_t* file_size) {
  File file = SPIFFS.open(path, "r");
  if (!file) {
    Serial.println("Failed to open file.");
    return NULL;
  }

  // Get the file size
  *file_size = file.size();
  Serial.print("File size: ");
  Serial.println(*file_size);

  // Allocate buffer to hold the binary data
  unsigned char* buffer = (unsigned char*)malloc(*file_size);
  if (!buffer) {
    Serial.println("Failed to allocate memory for binary file.");
    file.close();
    return NULL;
  }

  // Read the binary file into the buffer
  file.read(buffer, *file_size);
  file.close();

  // Return the binary data
  return buffer;
}
void BroadcastTimer(void* param) {

  // MAC
  String macAddress = WiFi.macAddress();

  // Include the node's status based on the bBusy flag
  String status = bBusy ? "busy" : "available";

  // Get free SPIFFS space
  size_t totalBytes = SPIFFS.totalBytes();
  size_t usedBytes = SPIFFS.usedBytes();
  size_t freeBytes = totalBytes - usedBytes;

  // Get the current RSSI
  int32_t rssi = WiFi.RSSI();

  // Manually construct the JSON string with MAC address, task count, status, free SPIFFS space, and RSSI
  String strAnnouncement = "{\"node\":\"TESSIE_NODE\",\"mac\":\"" + macAddress + "\",\"total_executed\":"
                           + String(uiTotalExecuted) + ",\"status\":\"" + status + "\",\"free_spiffs_bytes\":"
                           + String(freeBytes) + ",\"rssi\":" + String(rssi) + "}";

  udp.beginPacket("255.255.255.255", UDP_PORT);  // Broadcast message to the entire network
  udp.write((uint8_t*)strAnnouncement.c_str(), strAnnouncement.length());
  udp.endPacket();
}
void setup() {
  //
  Serial.begin(115200);

  Serial.println("Mounting FS...");
  while (!SPIFFS.begin()) {
    SPIFFS.format();
    Serial.println("Failed to mount file system");
    delay(1000);
  }


  // Open SPIFFS directory and perform cleanup
  File root = SPIFFS.open("/");
  File file = root.openNextFile();

  // Iterate and delete all files
  while (file) {
    Serial.println("Deleting: " + String(file.name()) + " " + String(file.size()));

    // Remove file
    String strFname = String("/") + file.name();

    SPIFFS.remove(strFname);

    // Move to next file
    file = root.openNextFile();
  }
  root.close();

  WiFi.hostname("TESSIE");
  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASS);

  // Wait for WIFI
  while (WiFi.status() != WL_CONNECTED) {
    Serial.println(".");
    delay(1000);
  }

  // Allow OTA for easier uploads
  ArduinoOTA
    .onStart([]() {
      String type;
      if (ArduinoOTA.getCommand() == U_FLASH)
        type = "sketch";
      else  // U_SPIFFS
        type = "filesystem";
    })
    .onEnd([]() {

    })
    .onProgress([](unsigned int progress, unsigned int total) {
      yield();
    })
    .onError([](ota_error_t error) {
      ESP.restart();
    });
  ArduinoOTA.setHostname("TESSIE");
  ArduinoOTA.begin();


  // Execute given binary with its payload
  WWWServer.on("/execute", [&]() {
    if (bBusy) {
      WWWServer.send(500, "application/json", "{\"status\": \"busy\"}");
      return;
    }

    // Mark busy from now
    bBusy = true;

    // Cleanup
    SPIFFS.remove(OUTPUT_FILE);

    // Load ELF binary
    size_t elf_file_size = 0;
    unsigned char* main_elf = LoadELFFile(BINARY_FILE, &elf_file_size);

    if (!main_elf) {
      WWWServer.send(500, "application/json", "{\"status\": \"error\", \"message\": \"Failed to load ELF binary\"}");
      bBusy = false;
      return;
    }

    const ELFLoaderSymbol_t exports[] = {
      { "puts", (void*)puts },
      { "printf", (void*)printf },
      { "fgets", (void*)fgets },
      { "fread", (void*)fread },
      { "fwrite", (void*)fwrite },
      { "fopen", (void*)fopen },
      { "fclose", (void*)fclose },
      { "fprintf", (void*)fprintf },
      { "fseek", (void*)fseek },
      { "ftell", (void*)ftell },
      { "fflush", (void*)fflush },
      { "fscanf", (void*)fscanf }
    };

    double a = log2(2);
    const ELFLoaderEnv_t env = { exports, sizeof(exports) / sizeof(*exports) };
    ELFLoaderContext_t* ctx = (ELFLoaderContext_t*)elfLoaderInitLoadAndRelocate(main_elf, &env);

    if (!ctx) {
      WWWServer.send(500, "application/json", "{\"status\": \"error\", \"message\": \"Failed to load ELF binary\"}");
      free(main_elf);
      bBusy = false;
      return;
    }

    // Set the main function to execute
    if (elfLoaderSetFunc(ctx, "local_main") != 0) {
      WWWServer.send(500, "application/json", "{\"status\": \"error\", \"message\": \"Failed to set function\"}");
      elfLoaderFree(ctx);
      free(main_elf);
      bBusy = false;
      return;
    }

    // Respond with success message
    WWWServer.send(200, "application/json", "{\"status\": \"success\", \"message\": \"Task started\"}");

    // Execute the function
    typedef void (*func_t)(const char*, size_t len);
    func_t func = (func_t)ctx->exec;
    func(strArgument.c_str(), strArgument.length());

    // Increment total executed tasks
    uiTotalExecuted++;

    // Clean up memory
    elfLoaderFree(ctx);
    free(main_elf);

    // Mark as not busy
    bBusy = false;
  });

  // Define route for binary upload using a lambda
  WWWServer.on(
    "/uploadbin", HTTP_POST, []() {
      strArgument = "";
      WWWServer.send(200);
    },
    [&]() {
      HTTPUpload& upload = WWWServer.upload();

      if (upload.status == UPLOAD_FILE_START) {
        SPIFFS.remove(BINARY_FILE);
        Serial.println("Binary upload started: " + upload.filename);
        binaryFile = SPIFFS.open(BINARY_FILE, FILE_WRITE);
        if (!binaryFile) {
          Serial.println("Failed to open binary file for writing");
          WWWServer.send(500, "application/json", "{\"status\": \"error\", \"message\": \"Failed to open binary file\"}");
          return;
        }
      } else if (upload.status == UPLOAD_FILE_WRITE) {
        Serial.println("Writing binary data..." + String(upload.currentSize));
        binaryFile.write(upload.buf, upload.currentSize);
      } else if (upload.status == UPLOAD_FILE_END) {
        binaryFile.close();
        Serial.println("Binary upload complete");
        WWWServer.send(200, "application/json", "{\"status\": \"success\", \"message\": \"Binary upload complete\"}");
      } else {
        Serial.println("Error during binary upload");
        WWWServer.send(500, "application/json", "{\"status\": \"error\", \"message\": \"Binary upload error\"}");
      }
    });


  // Define route for payload upload using a lambda
  WWWServer.on(
    "/uploadpayload", HTTP_POST, []() {
      strArgument = "";
      WWWServer.send(200);
    },
    [&]() {
      HTTPUpload& upload = WWWServer.upload();


      if (upload.status == UPLOAD_FILE_START) {
        SPIFFS.remove(INPUT_FILE);
        Serial.println("Payload upload started: " + upload.filename);
        payloadFile = SPIFFS.open(INPUT_FILE, FILE_WRITE);
        if (!payloadFile) {
          Serial.println("Failed to open payload file for writing");
          WWWServer.send(500, "application/json", "{\"status\": \"error\", \"message\": \"Failed to open payload file\"}");
          return;
        }
      } else if (upload.status == UPLOAD_FILE_WRITE) {
        // Write the payload data
        Serial.println("Writing payload data..." + String(upload.currentSize));
        payloadFile.write(upload.buf, upload.currentSize);
      } else if (upload.status == UPLOAD_FILE_END) {
        // Close the file when upload ends
        payloadFile.close();
        Serial.println("Payload upload complete");
        WWWServer.send(200, "application/json", "{\"status\": \"success\", \"message\": \"Payload upload complete\"}");
      } else {
        Serial.println("Error during payload upload");
        WWWServer.send(500, "application/json", "{\"status\": \"error\", \"message\": \"Payload upload error\"}");
      }
    });

  // Stream back output from the task
  WWWServer.on("/output", [&]() {
    // Stream the output file over HTTP
    File outputFile = SPIFFS.open(OUTPUT_FILE, "r");
    if (!outputFile) {
      WWWServer.send(500, "application/json", "{\"status\": \"error\", \"message\": \"Output file not found\"}");
      return;
    }

    WWWServer.streamFile(outputFile, "application/octet-stream");
    outputFile.close();
  });

  // Return if busy or available
  WWWServer.on("/status", [&]() {
    if (bBusy) {
      WWWServer.send(200, "application/json", "{\"status\": \"busy\"}");
    } else {
      WWWServer.send(200, "application/json", "{\"status\": \"available\"}");
    }
  });

  // Pass argument to the task
  WWWServer.on("/arg", [&]() {
    if (bBusy) {
      WWWServer.send(200, "application/json", "{\"status\": \"busy\"}");
    } else {
      strArgument = WWWServer.arg("plain");  // Store the argument
      Serial.println("Argument received: " + strArgument);
      WWWServer.send(200, "application/json", "{\"status\": \"ok\", \"argument\": \"" + strArgument + "\"}");
    }
  });

  // Init HTTP
  WWWServer.begin();

  // Initialize UDP
  udp.begin(UDP_PORT);

  // Initialize advertisement timer
  periodicTimer = xTimerCreate("PeriodicTimer", pdMS_TO_TICKS(BROADCAST_TIMER), pdTRUE, (void*)NULL, BroadcastTimer);
  xTimerStart(periodicTimer, 0);
}

void loop() {
  ArduinoOTA.handle();
  WWWServer.handleClient();
}