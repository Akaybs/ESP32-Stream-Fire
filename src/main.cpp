#include <WiFi.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <HardwareSerial.h>

const char *ssid = "HOANG WIFI";
const char *password = "hhhhhhhh";
String apiGetUrl = "https://User.firebasedatabase.app/roitai.json";
String apiPutUrl = "https://User.firebasedatabase.app/roitai/";

HardwareSerial sim800(1);
#define SIM800_TX 17
#define SIM800_RX 16
#define SIM800_BAUD 115200

#define LED_PIN 2
bool blinkLed = false;
unsigned long lastBlinkTime = 0;
bool ledState = false;

unsigned long lastCheckTime = 0;
const unsigned long checkInterval = 60000;

int apiErrorCount = 0;
unsigned long lastRestartTime = 0;
const unsigned long restartInterval = 30UL * 60UL * 1000UL;

// ================== BỎ DẤU TIẾNG VIỆT ==================
String removeVietnameseAccents(String str)
{
  const char *find[] = {"á", "à", "ả", "ã", "ạ", "ă", "ắ", "ằ", "ẳ", "ẵ", "ặ", "â", "ấ", "ầ", "ẩ", "ẫ", "ậ",
                        "đ",
                        "é", "è", "ẻ", "ẽ", "ẹ", "ê", "ế", "ề", "ể", "ễ", "ệ",
                        "í", "ì", "ỉ", "ĩ", "ị",
                        "ó", "ò", "ỏ", "õ", "ọ", "ô", "ố", "ồ", "ổ", "ỗ", "ộ", "ơ", "ớ", "ờ", "ở", "ỡ", "ợ",
                        "ú", "ù", "ủ", "ũ", "ụ", "ư", "ứ", "ừ", "ử", "ữ", "ự",
                        "ý", "ỳ", "ỷ", "ỹ", "ỵ",
                        "Á", "À", "Ả", "Ã", "Ạ", "Ă", "Ắ", "Ằ", "Ẳ", "Ẵ", "Ặ", "Â", "Ấ", "Ầ", "Ẩ", "Ẫ", "Ậ",
                        "Đ",
                        "É", "È", "Ẻ", "Ẽ", "Ẹ", "Ê", "Ế", "Ề", "Ể", "Ễ", "Ệ",
                        "Í", "Ì", "Ỉ", "Ĩ", "Ị",
                        "Ó", "Ò", "Ỏ", "Õ", "Ọ", "Ô", "Ố", "Ồ", "Ổ", "Ỗ", "Ộ", "Ơ", "Ớ", "Ờ", "Ở", "Ỡ", "Ợ",
                        "Ú", "Ù", "Ủ", "Ũ", "Ụ", "Ư", "Ứ", "Ừ", "Ử", "Ữ", "Ự",
                        "Ý", "Ỳ", "Ỷ", "Ỹ", "Ỵ"};
  const char *repl[] = {"a", "a", "a", "a", "a", "a", "a", "a", "a", "a", "a", "a", "a", "a", "a", "a", "a",
                        "d",
                        "e", "e", "e", "e", "e", "e", "e", "e", "e", "e", "e",
                        "i", "i", "i", "i", "i",
                        "o", "o", "o", "o", "o", "o", "o", "o", "o", "o", "o", "o", "o", "o", "o", "o", "o",
                        "u", "u", "u", "u", "u", "u", "u", "u", "u", "u", "u",
                        "y", "y", "y", "y", "y",
                        "A", "A", "A", "A", "A", "A", "A", "A", "A", "A", "A", "A", "A", "A", "A", "A", "A",
                        "D",
                        "E", "E", "E", "E", "E", "E", "E", "E", "E", "E", "E",
                        "I", "I", "I", "I", "I",
                        "O", "O", "O", "O", "O", "O", "O", "O", "O", "O", "O", "O", "O", "O", "O", "O", "O",
                        "U", "U", "U", "U", "U", "U", "U", "U", "U", "U", "U",
                        "Y", "Y", "Y", "Y", "Y"};
  for (int i = 0; i < sizeof(find) / sizeof(find[0]); i++)
  {
    str.replace(find[i], repl[i]);
  }
  return str;
}

String formatMoney(long money)
{
  String s = String(money);
  String res = "";
  int len = s.length();
  int count = 0;
  for (int i = len - 1; i >= 0; i--)
  {
    res = s.charAt(i) + res;
    count++;
    if (count == 3 && i != 0)
    {
      res = "." + res;
      count = 0;
    }
  }
  return res;
}

// ================== GỬI SMS ==================
bool sendSMS(String phoneNumber, String message)
{
  Serial.println("📤 [AT] Xoa SMS cu: AT+CMGD=1,4");
  sim800.println("AT+CMGD=1,4");
  delay(1000);

  Serial.println("📤 [AT] Chuyen sang Text mode: AT+CMGF=1");
  sim800.println("AT+CMGF=1");
  delay(500);

  String atCmd = "AT+CMGS=\"" + phoneNumber + "\"";
  Serial.println("📤 [AT] " + atCmd);
  sim800.println(atCmd);
  delay(500);

  Serial.println("📤 [SMS Noi dung]\n" + message);
  sim800.print(message);
  delay(500);
  sim800.write(26); // Ctrl+Z kết thúc tin nhắn

  String response = "";
  unsigned long start = millis();
  while (millis() - start < 8000) // chờ lâu hơn một chút
  {
    while (sim800.available())
    {
      char c = sim800.read();
      response += c;
    }
    if (response.indexOf("+CMGS:") != -1)
    {
      Serial.println("✅ [SIM800] SMS gui thanh cong");
      Serial.println("📥 [Phan hoi] " + response);
      blinkLed = false;
      digitalWrite(LED_PIN, LOW);
      return true;
    }
    if (response.indexOf("ERROR") != -1)
    {
      Serial.println("❌ [SIM800] SMS gui that bai");
      Serial.println("📥 [Phan hoi] " + response);
      blinkLed = true;
      return false;
    }
  }

  Serial.println("⚠ [SIM800] Timeout khong nhan duoc phan hoi");
  Serial.println("📥 [Phan hoi] " + response);
  blinkLed = true;
  return false;
}

// ================== CẬP NHẬT TRẠNG THÁI ==================
bool updateSMSStatus(String id, String status)
{
  if (WiFi.status() == WL_CONNECTED)
  {
    HTTPClient http;
    String url = apiPutUrl + id + "/sms.json";
    String jsonPayload = "\"" + status + "\"";

    Serial.println("🌐 [PUT] URL: " + url);
    Serial.println("🌐 [PUT] Payload: " + jsonPayload);

    http.begin(url);
    http.addHeader("Content-Type", "application/json");
    int httpCode = http.PUT(jsonPayload);

    Serial.println("🌐 [PUT] HTTP Code: " + String(httpCode));
    String response = http.getString();
    Serial.println("🌐 [PUT] Response: " + response);

    http.end();
    return (httpCode == 200);
  }
  return false;
}

// ================== LẤY DỮ LIỆU & GỬI SMS ==================
void checkAndSendSMS()
{
  if (WiFi.status() == WL_CONNECTED)
  {
    HTTPClient http;
    http.begin(apiGetUrl);
    int httpCode = http.GET();

    if (httpCode == 200)
    {
      apiErrorCount = 0;
      digitalWrite(LED_PIN, LOW);
      blinkLed = false;

      String payload = http.getString();
      Serial.println("📥 [Phan hoi API] " + payload);

      if (payload == "null" || payload.length() == 0)
      {
        http.end();
        return;
      }

      DynamicJsonDocument doc(32768);
      DeserializationError error = deserializeJson(doc, payload);

      if (!error && doc.is<JsonObject>())
      {
        JsonObject root = doc.as<JsonObject>();

        for (JsonPair kv : root)
        {
          String id = String(kv.key().c_str());
          JsonObject obj = kv.value().as<JsonObject>();

          String name = obj["name"] | "";
          String phone = obj["phone"] | "";
          String iphone = obj["iphone"] | "";
          String imei = obj["imei"] | "";
          String loi = obj["loi"] | "";
          String tienText = obj["tienText"] | "";
          String thanhtoan = obj["thanhtoan"] | "";
          String thoigian = obj["thoigian"] | "";
          long totalDebtVal = obj["totalDebt"] | 0;
          int soLuongNo = obj["soLuongNo"] | 0;
          String smsStatus = obj["sms"] | "";

          // chuẩn hóa tiền tệ
          tienText.replace("₫", " VND");
          loi.replace("₫", " VND");

          String smsContent;
          if (smsStatus == "Send" && thanhtoan == "TT")
          {
            smsContent = "TB: " + name + "\n" +
                         "Ban da " + loi + "\n" +
                         "Time: " + thoigian + "\n" +
                         "https://hoanglsls.web.app";
          }
          // --- Ok + Yes ---
          else if (thanhtoan == "Ok" && smsStatus == "Yes")
          {
            smsContent =
                "ID:" + id + " " + name + "\n" +
                "THANH TOAN OK\n" +
                "May: " + iphone + "\n" +
                "Imei: " + imei + "\n" +
                "Loi: " + loi + "\n" +
                "Tien: " + tienText + "\n" +
                //"T.Toan: " + thanhtoan + "\n" +
                "Time: " + thoigian + "\n";

            if (soLuongNo >= 1 && totalDebtVal > 0)
            {
              smsContent += "Tong no (" + String(soLuongNo) + " may): " + formatMoney(totalDebtVal) + " VND\n";
            }
          }
          // --- Nợ + Yes ---
          else if (thanhtoan == "Nợ" && smsStatus == "Yes")
          {
            smsContent =
                "ID:" + id + " " + name + "\n" +
                "CHUA THANH TOAN\n" +
                "May: " + iphone + "\n" +
                "Imei: " + imei + "\n" +
                "Loi: " + loi + "\n" +
                "Tien: " + tienText + "\n" +

                "Time: " + thoigian + "\n";

            if (soLuongNo >= 1 && totalDebtVal > 0)
            {
              smsContent += "Tong no (" + String(soLuongNo) + " may): " + formatMoney(totalDebtVal) + " VND\n";
            }
          }

          if (smsContent.length() > 0)
          {
            // 1) Bỏ dấu
            smsContent = removeVietnameseAccents(smsContent);

            // 2) Đếm độ dài tin nhắn
            Serial.println("📏 [SMS Length] " + String(smsContent.length()) + " ky tu");

            // 3) Nếu dài hơn 160 và có dòng "Tong no"
            int pos = smsContent.indexOf("Tong no");
            String smsMain = smsContent;
            String smsDebt = "";

            if (pos != -1 && smsContent.length() > 160)
            {
              // Cắt phần chính
              smsMain = smsContent.substring(0, pos);
              smsMain.trim();

              // Cắt phần Tổng nợ
              smsDebt = smsContent.substring(pos);
              smsDebt.trim();
            }

            // -------- Gửi tin nhắn chính --------
            Serial.println("📌 [Xem truoc SMS 1]\n" + smsMain);
            bool success = sendSMS(phone, smsMain);
            String newStatus = success ? "Done" : "Error";
            updateSMSStatus(id, newStatus);
            delay(5000);

            // -------- Nếu có SMS Tổng nợ, gửi tiếp --------
            if (smsDebt.length() > 0)
            {
              Serial.println("📌 [Xem truoc SMS 2]\n" + smsDebt);
              bool success2 = sendSMS(phone, smsDebt);
              String newStatus2 = success2 ? "Done" : "Error";
              updateSMSStatus(id, newStatus2);
              delay(5000);
            }
          }
        }
      }
    }
    http.end();
  }
}

// ================== SETUP ==================
void setup()
{
  Serial.begin(115200);
  pinMode(LED_PIN, OUTPUT);
  digitalWrite(LED_PIN, LOW);

  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED)
  {
    delay(500);
    Serial.print(".");
  }
  Serial.println("✅ WiFi connected");

  sim800.begin(SIM800_BAUD, SERIAL_8N1, SIM800_RX, SIM800_TX);
  Serial.println("📌 Module 4G Ok");

  checkAndSendSMS();
}

// ================== LOOP ==================
void loop()
{
  if (millis() - lastCheckTime > checkInterval)
  {
    lastCheckTime = millis();
    checkAndSendSMS();
  }

  if (blinkLed)
  {
    if (millis() - lastBlinkTime > 500)
    {
      lastBlinkTime = millis();
      ledState = !ledState;
      digitalWrite(LED_PIN, ledState);
    }
  }
}
