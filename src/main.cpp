/**
 * main_stream.cpp
 * Kết hợp: Khung Streaming (STM32 example) + logic ESP32 (SMS, LED, xử lý JSON)
 *
 * Lưu ý: cần file ExampleFunctions.h (helper của FirebaseClient example)
 */

#define ENABLE_USER_AUTH
#define ENABLE_DATABASE

#include <Arduino.h>
#include <WiFi.h>
#include <FirebaseClient.h>
#include <ArduinoJson.h>
#include <HardwareSerial.h>
#include "ExampleFunctions.h"    // Bắt buộc: set_ssl_client_insecure_and_buffer, getAuth, auth_debug_print...

// === Cấu hình Wi-Fi / Firebase / SIM ===
#define WIFI_SSID "HOANG WIFI"
#define WIFI_PASSWORD "hhhhhhhh"

#define Web_API_KEY "Key"
#define DATABASE_URL "https://user.asia-southeast1.firebasedatabase.app/"
#define USER_EMAIL "akaybs@gmail.com"
#define USER_PASS "123456"

#define LED_PIN 2

// SIM A800
HardwareSerial sim800(1);
#define SIM800_TX 17
#define SIM800_RX 16
#define SIM800_BAUD 115200
bool simReady = false;

// === Firebase / clients (the STM32 example style) ===
SSL_CLIENT ssl_client, stream_ssl_client;
using AsyncClient = AsyncClientClass;
AsyncClient aClient(ssl_client), streamClient(stream_ssl_client);

UserAuth user_auth(Web_API_KEY, USER_EMAIL, USER_PASS, 3000);
FirebaseApp app;
RealtimeDatabase Database;

// Biến lưu kết quả stream (dùng như ví dụ)
AsyncResult streamResult;

// LED status
unsigned long lastBlinkTime = 0;
bool ledState = false;
int ledMode = 0; // 1 wifi err, 2 firebase err, 3 sim err, 4 ok

// ==================== HÀM TIỆN ÍCH ====================
String removeVietnameseAccents(String str) {
  const char *find[] = {"á","à","ả","ã","ạ","ă","ắ","ằ","ẳ","ẵ","ặ","â","ấ","ầ","ẩ","ẫ","ậ",
                        "đ",
                        "é","è","ẻ","ẽ","ẹ","ê","ế","ề","ể","ễ","ệ",
                        "í","ì","ỉ","ĩ","ị",
                        "ó","ò","ỏ","õ","ọ","ô","ố","ồ","ổ","ỗ","ộ","ơ","ớ","ờ","ở","ỡ","ợ",
                        "ú","ù","ủ","ũ","ụ","ư","ứ","ừ","ử","ữ","ự",
                        "ý","ỳ","ỷ","ỹ","ỵ",
                        "Á","À","Ả","Ã","Ạ","Ă","Ắ","Ằ","Ẳ","Ẵ","Ặ","Â","Ấ","Ầ","Ẩ","Ẫ","Ậ",
                        "Đ",
                        "É","È","Ẻ","Ẽ","Ẹ","Ê","Ế","Ề","Ể","Ễ","Ệ",
                        "Í","Ì","Ỉ","Ĩ","Ị",
                        "Ó","Ò","Ỏ","Õ","Ọ","Ô","Ố","Ồ","Ổ","Ỗ","Ộ","Ơ","Ớ","Ờ","Ở","Ỡ","Ợ",
                        "Ú","Ù","Ủ","Ũ","Ụ","Ư","Ứ","Ừ","Ử","Ữ","Ự",
                        "Ý","Ỳ","Ỷ","Ỹ","Ỵ"};
  const char *repl[] = {"a","a","a","a","a","a","a","a","a","a","a","a","a","a","a","a","a",
                        "d",
                        "e","e","e","e","e","e","e","e","e","e","e",
                        "i","i","i","i","i",
                        "o","o","o","o","o","o","o","o","o","o","o","o","o","o","o","o","o",
                        "u","u","u","u","u","u","u","u","u","u","u",
                        "y","y","y","y","y",
                        "A","A","A","A","A","A","A","A","A","A","A","A","A","A","A","A","A",
                        "D",
                        "E","E","E","E","E","E","E","E","E","E","E",
                        "I","I","I","I","I",
                        "O","O","O","O","O","O","O","O","O","O","O","O","O","O","O","O","O",
                        "U","U","U","U","U","U","U","U","U","U","U",
                        "Y","Y","Y","Y","Y"};
  for (int i = 0; i < (int)(sizeof(find) / sizeof(find[0])); i++)
    str.replace(find[i], repl[i]);
  return str;
}

String formatMoney(long money) {
  String s = String(money);
  String res = "";
  int len = s.length();
  int count = 0;
  for (int i = len - 1; i >= 0; i--) {
    res = s.charAt(i) + res;
    count++;
    if (count == 3 && i != 0) {
      res = "." + res;
      count = 0;
    }
  }
  return res;
}

// LED update
void updateLed() {
  unsigned long now = millis();
  if (ledMode == 1) {
    if (now - lastBlinkTime > 1000) {
      lastBlinkTime = now;
      ledState = !ledState;
      digitalWrite(LED_PIN, ledState);
    }
  } else if (ledMode == 2) {
    if (now - lastBlinkTime > 250) {
      lastBlinkTime = now;
      ledState = !ledState;
      digitalWrite(LED_PIN, ledState);
    }
  } else if (ledMode == 3) {
    static int blinkCount = 0;
    static unsigned long patternTimer = 0;
    if (now - patternTimer > 200) {
      patternTimer = now;
      blinkCount++;
      digitalWrite(LED_PIN, blinkCount % 2);
      if (blinkCount >= 4) {
        blinkCount = 0;
        delay(1600);
      }
    }
  } else if (ledMode == 4) {
    digitalWrite(LED_PIN, LOW);
  }
}

// ==================== GỬI SMS ====================
bool sendSMS(String phone, String content) {
  while (sim800.available()) sim800.read();

  sim800.println("AT+CMGF=1");
  delay(300);
  if (!sim800.find("OK")) Serial.println("⚠️ Không phản hồi CMGF");

  sim800.print("AT+CMGS=\"");
  sim800.print(phone);
  sim800.println("\"");
  delay(300);

  sim800.print(content);
  sim800.write(26); // Ctrl+Z

  String response = "";
  unsigned long start = millis();
  while (millis() - start < 8000) {
    while (sim800.available()) {
      char c = sim800.read();
      response += c;
    }
    if (response.indexOf("+CMGS:") != -1) {
      Serial.println("✅ SMS gửi thành công");
      return true;
    }
    if (response.indexOf("ERROR") != -1) {
      Serial.println("❌ Gửi thất bại");
      return false;
    }
  }
  Serial.println("⚠️ Hết thời gian chờ phản hồi");
  return false;
}

void updateSMSStatus(String id, String status) {
  String path = "/roitai/" + id + "/sms";
  Database.set<String>(aClient, path, status, [](AsyncResult &r) {
    if (r.isError())
      Serial.printf("❌ Update error: %s\n", r.error().message().c_str());
    else
      Serial.printf("✅ Updated %s\n", r.c_str());
  });
}

// ==================== FIREBASE CALLBACK (Stream) ====================
void processData(AsyncResult &aResult) {
  if (!aResult.isResult())
    return;

  // 🧭 Log sự kiện Firebase
  if (aResult.isEvent()) {
    Serial.printf("[Sự kiện] %s | Mã: %d\n",
                  aResult.eventLog().message().c_str(),
                  aResult.eventLog().code());
  }

  // ⚠️ Log lỗi Firebase (nếu có)
  if (aResult.isError()) {
    Serial.printf("[Lỗi] %s | Mã: %d\n",
                  aResult.error().message().c_str(),
                  aResult.error().code());
    return;
  }

  // 📡 Khi có dữ liệu từ Firebase stream
  if (aResult.available()) {
    RealtimeDatabaseResult &stream = aResult.to<RealtimeDatabaseResult>();

    if (stream.isStream()) {
      Serial.println("=== STREAM UPDATE ===");
      Serial.printf("Sự kiện: %s\n", stream.event().c_str());
    
      Serial.printf("Dữ liệu: %s\n", stream.to<const char*>());
     // Serial.println("=====================");
    }

    // ✅ Lấy dữ liệu JSON hợp lệ
    const char *payload = stream.to<const char*>();
    if (payload == nullptr || strlen(payload) == 0 || strcmp(payload, "null") == 0) {
      //Serial.println("⚠️ Không có JSON hợp lệ để xử lý.");
      return;
    }

    // ✅ Log JSON nhận được
   // Serial.print("📦 JSON: ");
   // Serial.println(payload);

    // ✅ Parse JSON (ArduinoJson 7.4.2 — tự cấp phát bộ nhớ)
    JsonDocument doc;
    DeserializationError err = deserializeJson(doc, payload);
    if (err) {
      Serial.print("❌ deserializeJson error: ");
      Serial.println(err.c_str());
      return;
    }

    JsonObject root = doc.as<JsonObject>();

    // ✅ Lặp qua từng phần tử trong JSON
    for (JsonPair kv : root) {
      JsonObject item = kv.value().as<JsonObject>();
      String id = String(kv.key().c_str());
      String name = item["name"] | "";
      String phone = item["phone"] | "";
      String iphone = item["iphone"] | "";
      String imei = item["imei"] | "";
      String loi = item["loi"] | "";
      String thanhtoan = item["thanhtoan"] | "";
      String smsStatus = item["sms"] | "";
      String thoigian = item["thoigian"] | "";
      String tienText = item["tienText"] | "";
      int soLuongNo = item["soLuongNo"] | 0;
      long totalDebtVal = item["totalDebt"] | 0;

      // Chuẩn hóa ký hiệu tiền
      tienText.replace("₫", "VND");
      tienText.replace("đ", "VND");
      loi.replace("₫", "VND");
      loi.replace("đ", "VND");

      // ✅ Tạo nội dung SMS theo trạng thái
      String smsContent = "";
      if (smsStatus == "Send" && thanhtoan == "TT") {
        smsContent = "TB: " + name + "\nBan da " + loi + "\nTime: " + thoigian + "\nhttps://hoanglsls.web.app";
      } else if (thanhtoan == "Ok" && smsStatus == "Yes") {
        smsContent = "TB:" + id + " " + name + "\nTHANH TOAN OK\n" + iphone +
                     "\nImei: " + imei + "\nLoi: " + loi + "\nTien: " + tienText +
                     "\n" + thoigian + "\n";
        if (soLuongNo >= 1 && totalDebtVal > 0)
          smsContent += "Tong no (" + String(soLuongNo) + " may): " +
                        formatMoney(totalDebtVal) + " VND\n";
      } else if (thanhtoan == "Nợ" && smsStatus == "Yes") {
        smsContent = "ID:" + id + " " + name + "\nCHUA THANH TOAN\n" + iphone +
                     "\nImei: " + imei + "\nLoi: " + loi + "\nTien: " + tienText +
                     "\n" + thoigian + "\n";
        if (soLuongNo >= 1 && totalDebtVal > 0)
          smsContent += "Tong no (" + String(soLuongNo) + " may): " +
                        formatMoney(totalDebtVal) + " VND\n";
      } else if (thanhtoan == "Back" && smsStatus == "Yes") {
        smsContent = "TB:" + id + " " + name + "\nHOAN TIEN\n" + iphone +
                     "\nImei: " + imei + "\nLoi: " + loi + "\nTien: " + tienText +
                     "\n" + thoigian + "\n";
        if (soLuongNo >= 1 && totalDebtVal > 0)
          smsContent += "Tong no (" + String(soLuongNo) + " may): " +
                        formatMoney(totalDebtVal) + " VND\n";
      }

      // ✅ Gửi SMS nếu có nội dung
      if (smsContent.length() > 0) {
        // Hiển thị trước tin nhắn sẽ gửi
        Serial.println("🟦 [XEM TRƯỚC TIN NHẮN SMS]");
        Serial.printf("→ Người nhận: %s\n", phone.c_str());
        //Serial.println("→ Nội dung:");
        Serial.println(smsContent);
        Serial.println("──────────────────────────────");

        smsContent = removeVietnameseAccents(smsContent);
        int pos = smsContent.indexOf("Tong no");
        String smsMain = smsContent;
        String smsDebt = "";
        if (pos != -1 && smsContent.length() > 160) {
          smsMain = smsContent.substring(0, pos);
          smsDebt = smsContent.substring(pos);
        }

        bool ok1 = sendSMS(phone, smsMain);
        updateSMSStatus(id, ok1 ? "Done" : "Error");

        if (smsDebt.length() > 0) {
          delay(3000);
          bool ok2 = sendSMS(phone, smsDebt);
          (void)ok2;
        }

        Serial.printf("📤 SMS sent (stream): ID=%s | phone=%s | status=%s\n",
                      id.c_str(), phone.c_str(), ok1 ? "✅ OK" : "❌ FAIL");
      }
    }
  }
}


// ==================== SETUP ====================
bool checkSimModule() {
  sim800.println("AT");
  unsigned long t = millis();
  while (millis() - t < 1000) {
    if (sim800.find("OK")) return true;
  }
  return false;
}

void setup() {
  pinMode(LED_PIN, OUTPUT);
  digitalWrite(LED_PIN, LOW);
  Serial.begin(115200);
  sim800.begin(SIM800_BAUD, SERIAL_8N1, SIM800_RX, SIM800_TX);
  Serial.println("🔹 SIM A800 initializing...");

  // Kết nối WiFi
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  Serial.print("Đang kết nối WiFi");
  while (WiFi.status() != WL_CONNECTED) {
    Serial.print(".");
    ledMode = 1;
    updateLed();
    delay(300);
  }
  Serial.println();
  Serial.print("Đã kết nối, IP: ");
  Serial.println(WiFi.localIP());

  simReady = checkSimModule();
  Serial.println(simReady ? "✅ SIM A800 OK" : "❌ SIM A800 ERROR");

  // === Khởi tạo Firebase (the STM32 example style) ===
  Firebase.printf("Firebase Client v%s\n", FIREBASE_CLIENT_VERSION);

  // Thiết lập ssl client + bộ đệm phù hợp
  set_ssl_client_insecure_and_buffer(ssl_client);
  set_ssl_client_insecure_and_buffer(stream_ssl_client);

  Serial.println("Đang khởi tạo FirebaseApp...");
  initializeApp(aClient, app, getAuth(user_auth), auth_debug_print, "authTask");

  app.getApp<RealtimeDatabase>(Database);
  Database.url(DATABASE_URL);

  // Cấu hình streamClient chỉ nghe các sự kiện cần thiết
  streamClient.setSSEFilters("put,patch,get,auth_revoked");

  Serial.println("📡 Đang lấy toàn bộ dữ liệu /roitai ...");

  // Lấy toàn bộ JSON tại /roitai (lần đầu) — callback sẽ xử lý
  Database.get(aClient, "/roitai", processData, "getAllData");

  Serial.println("Đang nghe dữ liệu từ Firebase...");
  // Lưu ý: theo ví dụ STM32, việc xử lý stream xảy ra trong loop() qua processData(streamResult)
}

// ==================== LOOP ====================
void loop() {
  // Vòng lặp chính phải gọi app.loop() để duy trì kết nối và xác thực
  app.loop();

  // Cập nhật LED
  updateLed();

  // Chế độ LED theo trạng thái
  if (WiFi.status() != WL_CONNECTED)
    ledMode = 1;
  else if (!app.ready())
    ledMode = 2;
  else if (!simReady)
    ledMode = 3;
  else
    ledMode = 4;

  // Xử lý dữ liệu stream (dữ liệu mới sẽ làm streamResult có sẵn và gọi processData)
  processData(streamResult);

  // Thêm 1 delay nhỏ để tránh vòng loop quá nhanh (tùy cấu hình)
  delay(5);
}
