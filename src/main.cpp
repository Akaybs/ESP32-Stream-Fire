#include <WiFi.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <HardwareSerial.h>

const char *ssid = "HOANG WIFI";
const char *password = "hhhhhhhh";
String apiGetUrl = "https://hoanglsls-default-rtdb.asia-southeast1.firebasedatabase.app/roitai.json";
String apiPutUrl = "https://hoanglsls-default-rtdb.asia-southeast1.firebasedatabase.app/roitai/"; // sẽ append id + "/sms.json"


HardwareSerial sim800(1);
#define SIM800_TX 17
#define SIM800_RX 16
#define SIM800_BAUD 115200

// LED trạng thái
#define LED_PIN 2
bool blinkLed = false;
unsigned long lastBlinkTime = 0;
bool ledState = false;

unsigned long lastCheckTime = 0;
const unsigned long checkInterval = 60000;

// Biến đếm lỗi API và quản lý reset
int apiErrorCount = 0;
unsigned long lastRestartTime = 0;
const unsigned long restartInterval = 30UL * 60UL * 1000UL; // 30 phút

// ================== HÀM XỬ LÝ CHUỖI ==================
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
    String findUpper = String(find[i]);
    findUpper.toUpperCase();
    String replUpper = String(repl[i]);
    replUpper.toUpperCase();
    str.replace(findUpper, replUpper);
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

// ================== KIỂM TRA SÓNG & ĐĂNG KÝ MẠNG ==================
void checkSignalAndNetwork()
{
  String response = "";

  // Kiểm tra cường độ tín hiệu
  sim800.println("AT+CSQ");
  delay(1000);
  while (sim800.available())
  {
    response += (char)sim800.read();
  }
 // Serial.println("📡 [CSQ Raw] " + response);

  int csqVal = -1, ber = -1;
  if (response.indexOf("+CSQ:") != -1)
  {
    int idx = response.indexOf(":");
    if (idx != -1)
    {
      String data = response.substring(idx + 1);
      data.trim();
      csqVal = data.substring(0, data.indexOf(",")).toInt();
      ber = data.substring(data.indexOf(",") + 1).toInt();
    }
  }

  if (csqVal >= 0 && csqVal <= 31)
  {
    int dBm = -113 + (csqVal * 2); // công thức từ datasheet
    String level;
    if (csqVal <= 9)
      level = "Yếu";
    else if (csqVal <= 14)
      level = "Trung bình";
    else if (csqVal <= 19)
      level = "Khá";
    else
      level = "Mạnh (tốt)";

    Serial.println("📡 [CSQ] " + String(csqVal) + "," + String(ber) +
                   "  ~ " + String(dBm) + " dBm, " + level);
  }
  else
  {
    Serial.println("📡 [CSQ] Không xác định (99)");
  }

  // --- Kiểm tra tình trạng đăng ký mạng ---
  response = "";
  sim800.println("AT+CREG?");
  delay(1000);
  while (sim800.available())
  {
    response += (char)sim800.read();
  }
//  Serial.println("📡 [CREG Raw] " + response);

  int regStat = -1;
  if (response.indexOf("+CREG:") != -1)
  {
    int commaIdx = response.lastIndexOf(",");
    if (commaIdx != -1)
    {
      regStat = response.substring(commaIdx + 1).toInt();
    }
  }

  String netStatus;
  switch (regStat)
  {
  case 0:
    netStatus = "Chưa đăng ký, không tìm mạng";
    break;
  case 1:
    netStatus = "Đã đăng ký (Home network)";
    break;
  case 2:
    netStatus = "Đang tìm mạng...";
    break;
  case 3:
    netStatus = "Bị từ chối đăng ký";
    break;
  case 4:
    netStatus = "Không xác định";
    break;
  case 5:
    netStatus = "Đã đăng ký (Roaming)";
    break;
  default:
    netStatus = "Không rõ";
    break;
  }

  Serial.println("📡 [CREG] Trạng thái: " + netStatus);
}


// ================== GỬI SMS ==================
bool sendSMS(String phoneNumber, String message)
{
  // 🔹 Xóa toàn bộ SMS cũ để tránh FULL
  sim800.println("AT+CMGD=1,4");  
  delay(1000);

  sim800.println("AT+CMGF=1");
  delay(500);
  sim800.print("AT+CMGS=\"");
  sim800.print(phoneNumber);
  sim800.println("\"");
  delay(500);
  sim800.print(message);
  delay(500);
  sim800.write(26);

  String response = "";
  unsigned long start = millis();
  while (millis() - start < 5000)
  {
    while (sim800.available())
    {
      char c = sim800.read();
      response += c;
    }
    if (response.indexOf("+CMGS:") != -1)
    {
      Serial.println("📥 [Phan hoi Module 4G] " + response);
      blinkLed = false; // Không lỗi => tắt nhấp nháy
      digitalWrite(LED_PIN, LOW);
      return true;
    }
    if (response.indexOf("ERROR") != -1)
    {
      Serial.println("📥 [Phan hoi Module 4G] " + response);
      blinkLed = true; // Lỗi module 4G => nhấp nháy LED
      return false;
    }
  }
  Serial.println("📥 [Phan hoi Module 4G] " + response);
  blinkLed = true; // Timeout => nhấp nháy LED
  return false;
}

// ================== CẬP NHẬT TRẠNG THÁI ==================
bool updateSMSStatus(String id, String status)
{
  if (WiFi.status() == WL_CONNECTED)
  {
    HTTPClient http;
    // Tạo URL: https://.../roitai/<id>/sms.json
    String url = apiPutUrl + id + "/sms.json";
    Serial.println("📌 [Cap nhat SMS] URL: " + url);
    http.begin(url);
    http.addHeader("Content-Type", "application/json");

    // Ghi trực tiếp 1 giá trị string vào node sms (ví dụ: "Done")
    String jsonPayload = "\"" + status + "\"";
    int httpCode = http.PUT(jsonPayload);

    Serial.println("📌 [Cap nhat SMS] Ma HTTP: " + String(httpCode));
    String payload = http.getString();
    Serial.println("📌 [Cap nhat SMS] Phan hoi: " + payload);
    http.end();
    return (httpCode == 200);
  }
  return false;
}


// ================== LẤY DỮ LIỆU API & GỬI SMS ==================
void checkAndSendSMS()
{
  if (WiFi.status() == WL_CONNECTED)
  {
    HTTPClient http;
    http.begin(apiGetUrl);
    int httpCode = http.GET();

    if (httpCode == 200)
    {
      apiErrorCount = 0;          // reset lỗi khi API thành công
      digitalWrite(LED_PIN, LOW); // API OK => tắt LED
      blinkLed = false;

      String payload = http.getString();
      Serial.println("📥 [Phan hoi API] " + payload);

      // 🔹 Nếu Firebase trả về null hoặc rỗng thì bỏ qua
      if (payload == "null" || payload.length() == 0)
      {
        //Serial.println("⚠ Khong co du lieu trong DB, tiep tuc doi...");
        http.end();
        return; // kết thúc hàm, sẽ gọi lại sau 1 phút
      }

      DynamicJsonDocument doc(16384); // tăng dung lượng để parse Firebase JSON
      DeserializationError error = deserializeJson(doc, payload);

      if (!error && doc.is<JsonObject>())
      {
        for (JsonPair kv : doc.as<JsonObject>())
        {
          String id = String(kv.key().c_str());   // key Firebase, ví dụ "-Nx9abCDEfg12"
          JsonObject obj = kv.value().as<JsonObject>();

          // Lấy dữ liệu trong object
          String name       = obj["name"].as<String>();
          String phone      = obj["phone"].as<String>();
          String iphone     = obj["iphone"].as<String>();
          String imei       = obj["imei"].as<String>();
          String loi        = obj["loi"].as<String>();
          String tienText   = obj["tienText"].as<String>();
          String thanhtoan  = obj["thanhtoan"].as<String>();
          String thoigian   = obj["thoigian"].as<String>();
          long totalDebtVal = obj["totalDebt"] | 0;
          int soLuongNo     = obj["soLuongNo"] | 0;
          String smsStatus  = obj["sms"].as<String>();

          // Xử lý nội dung SMS
          tienText.replace("₫", "VND");
          tienText = removeVietnameseAccents(tienText);
          loi.replace("₫", "VND");
          loi = removeVietnameseAccents(loi);

          String smsContent;

          if (smsStatus == "Send" && thanhtoan == "TT")
          {
            smsContent = "TB: " + name + "\n" +
                         "Ban da " + loi + "\n";
            if (totalDebtVal == 0)
            {
              smsContent += "Time: " + thoigian + "\n";
            }
            else
            {
              smsContent += "Time: " + thoigian + "\n";
              smsContent += "TONG NO: " + formatMoney(totalDebtVal) + " VND\n";
            }
            smsContent += "https://hoanglsls.web.app";
          }
          else if (thanhtoan == "Ok" && smsStatus == "Yes")
          {
            if (totalDebtVal == 0)
            {
              smsContent =
                  "ID:" + id + " " + name + "\n" +
                  "Thanh toan thanh cong\n" +
                  "May: " + iphone + "\n" +
                  "Imei: " + imei + "\n" +
                  "Loi: " + loi + "\n" +
                  "Tien: " + tienText + "\n" +
                  "T.Toan: " + thanhtoan + "\n" +
                  "Time: " + thoigian + "\n";
            }
            else
            {
              smsContent =
                  "ID:" + id + " " + name + "\n" +
                  "May: " + iphone + "\n" +
                  "Imei: " + imei + "\n" +
                  "Loi: " + loi + "\n" +
                  "Tien: " + tienText + "\n" +
                  "T.Toan: " + thanhtoan + "\n" +
                  "Time: " + thoigian + "\n" +
                  "Tong no (" + String(soLuongNo) + " may): " + formatMoney(totalDebtVal) + " VND\n";
            }
          }
          else
          {
            smsContent =
                "ID:" + id + " " + name + "\n" +
                "May: " + iphone + "\n" +
                "Imei: " + imei + "\n" +
                "Loi: " + loi + "\n" +
                "Tien: " + tienText + "\n" +
                "T.Toan: " + thanhtoan + "\n" +
                "Time: " + thoigian + "\n" +
                "Tong no (" + String(soLuongNo) + " may): " + formatMoney(totalDebtVal) + " VND";
          }

          smsContent = removeVietnameseAccents(smsContent);
          Serial.println("📌 [Xem truoc SMS]\n" + smsContent);

          // Gửi SMS
          bool success = sendSMS(phone, smsContent);
          String newStatus = success ? "Done" : "Error";
          Serial.println(success ? "✅ Sending SMS to: " + phone
                                 : "❌ Sending SMS Error to: " + phone);

          // Cập nhật trạng thái SMS lên Firebase
          bool updated = false;
          for (int attempt = 1; attempt <= 3; attempt++)
          {
            if (updateSMSStatus(id, newStatus))
            {
              Serial.println("✅ Cap nhat trang thai '" + newStatus + "' thanh cong (lan " + String(attempt) + ")");
              updated = true;
              break;
            }
            if (attempt < 3)
            {
              Serial.println("⚠ Lan " + String(attempt) + " that bai, thu lai sau 10 giay...");
              delay(10000);
            }
          }
          if (!updated)
          {
            Serial.println("❌ Khong the cap nhat trang thai '" + newStatus + "' sau 3 lan thu");
          }

          delay(5000); // nghỉ giữa các SMS
        }
      }
      else
      {
        Serial.println("❌ Loi parse JSON: " + String(error.c_str()));
      }
    }
    else
    {
      Serial.println("❌ [Error] API GET that bai, ma: " + String(httpCode));
      digitalWrite(LED_PIN, HIGH); // Lỗi GET => sáng LED liên tục
      blinkLed = false;
      apiErrorCount++;
      Serial.println("⚠ So lan loi API lien tiep: " + String(apiErrorCount));

      // Nếu quá 5 lần lỗi => restart
      if (apiErrorCount >= 5 && millis() - lastRestartTime > restartInterval)
      {
        Serial.println("🚨 Qua 5 lan loi API, khoi dong lai ESP32...");
        delay(2000);
        lastRestartTime = millis();
        ESP.restart();
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
  Serial.print("📥 Connecting to WiFi");
  while (WiFi.status() != WL_CONNECTED)
  {
    delay(500);
    Serial.print(".");
  }
  Serial.println("✅ WiFi connected Ok!");

  sim800.begin(SIM800_BAUD, SERIAL_8N1, SIM800_RX, SIM800_TX);
  Serial.println("📌 Module 4G Ok");
// 🔹 Kiểm tra sóng và đăng ký mạng lần đầu
checkSignalAndNetwork();
  // 🔹 Gọi API ngay sau khi kết nối thành công
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

  // Xử lý nhấp nháy LED khi lỗi module 4G
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
