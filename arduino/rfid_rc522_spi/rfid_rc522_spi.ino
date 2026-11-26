#include <SPI.h>
#include <MFRC522.h>

// RC522 SPI Pin Bağlantıları (Arduino Uno / Nano)
constexpr uint8_t RST_PIN = 9;
constexpr uint8_t SS_PIN  = 10;

MFRC522 mfrc522(SS_PIN, RST_PIN);

String inputLine = "";

void setup() {
  Serial.begin(115200);
  while (!Serial) {
    ; // Seri portun açılmasını bekle
  }
  SPI.begin();
  mfrc522.PCD_Init();
  Serial.println("READY:RC522_SPI");
}

void loop() {
  while (Serial.available() > 0) {
    char c = (char)Serial.read();
    if (c == '\r') {
      continue;
    }
    if (c == '\n') {
      inputLine.trim();
      if (inputLine.length() > 0) {
        handleCommand(inputLine);
      }
      inputLine = "";
    } else {
      if (inputLine.length() < 160) {
        inputLine += c;
      }
    }
  }
}

int hexCharToInt(char c) {
  if (c >= '0' && c <= '9') return c - '0';
  if (c >= 'A' && c <= 'F') return c - 'A' + 10;
  if (c >= 'a' && c <= 'f') return c - 'a' + 10;
  return -1;
}

bool parseHexBytes(const String& hexStr, byte* outBuf, uint8_t expectedBytes) {
  if (hexStr.length() != expectedBytes * 2) {
    return false;
  }
  for (uint8_t i = 0; i < expectedBytes; i++) {
    int hiVal = hexCharToInt(hexStr.charAt(i * 2));
    int loVal = hexCharToInt(hexStr.charAt(i * 2 + 1));
    if (hiVal < 0 || loVal < 0) {
      return false;
    }
    outBuf[i] = (byte)((hiVal << 4) | loVal);
  }
  return true;
}

bool parseKey(const String& hexKey, MFRC522::MIFARE_Key& key) {
  return parseHexBytes(hexKey, key.keyByte, 6);
}

bool parseAuthType(const String& typeStr, MFRC522::PICC_Command& authCmd) {
  if (typeStr == "A" || typeStr == "a") {
    authCmd = MFRC522::PICC_CMD_MF_AUTH_KEY_A;
    return true;
  }
  if (typeStr == "B" || typeStr == "b") {
    authCmd = MFRC522::PICC_CMD_MF_AUTH_KEY_B;
    return true;
  }
  return false;
}

bool waitForCard(unsigned long timeoutMs = 8000) {
  unsigned long start = millis();
  while (millis() - start < timeoutMs) {
    if (mfrc522.PICC_IsNewCardPresent() && mfrc522.PICC_ReadCardSerial()) {
      return true;
    }
    delay(30);
  }
  return false;
}

void endCardSession() {
  mfrc522.PICC_HaltA();
  mfrc522.PCD_StopCrypto1();
}

bool reselectCard() {
  mfrc522.PCD_StopCrypto1();
  byte atqa[2];
  byte atqaLen = sizeof(atqa);
  if (mfrc522.PICC_WakeupA(atqa, &atqaLen) != MFRC522::STATUS_OK) {
    return false;
  }
  return mfrc522.PICC_Select(&mfrc522.uid) == MFRC522::STATUS_OK;
}

void printHexBytes(const byte* data, byte len) {
  for (byte i = 0; i < len; i++) {
    if (data[i] < 0x10) Serial.print('0');
    Serial.print(data[i], HEX);
  }
}

void printAsciiBytes(const byte* data, byte len) {
  for (byte i = 0; i < len; i++) {
    char c = (char)data[i];
    if (c >= 32 && c <= 126) {
      Serial.print(c);
    } else {
      Serial.print('.');
    }
  }
}

void handleCommand(const String& cmd) {
  if (cmd == "PING") {
    Serial.println("PONG");
    return;
  }

  if (cmd == "UID") {
    if (!waitForCard()) {
      Serial.println("ERR:NO_CARD");
      return;
    }
    Serial.print("OK:UID:");
    printHexBytes(mfrc522.uid.uidByte, mfrc522.uid.size);
    Serial.println();
    endCardSession();
    return;
  }

  if (cmd.startsWith("READ ")) {
    // Format: READ <block> <A|B> <12_hex_key>
    int s1 = cmd.indexOf(' ');
    int s2 = cmd.indexOf(' ', s1 + 1);
    int s3 = cmd.indexOf(' ', s2 + 1);
    if (s2 < 0 || s3 < 0) {
      Serial.println("ERR:BAD_ARGS");
      return;
    }

    int block = cmd.substring(s1 + 1, s2).toInt();
    String keyTypeStr = cmd.substring(s2 + 1, s3);
    String keyHex = cmd.substring(s3 + 1);
    keyHex.trim();

    if (block < 0 || block > 63) {
      Serial.println("ERR:INVALID_BLOCK");
      return;
    }

    MFRC522::PICC_Command authCmd;
    if (!parseAuthType(keyTypeStr, authCmd)) {
      Serial.println("ERR:INVALID_KEY_TYPE");
      return;
    }

    MFRC522::MIFARE_Key key;
    if (!parseKey(keyHex, key)) {
      Serial.println("ERR:INVALID_KEY");
      return;
    }

    if (!waitForCard()) {
      Serial.println("ERR:NO_CARD");
      return;
    }

    MFRC522::StatusCode status = mfrc522.PCD_Authenticate(
      authCmd, (byte)block, &key, &(mfrc522.uid)
    );
    if (status != MFRC522::STATUS_OK) {
      Serial.print("ERR:AUTH_FAILED:");
      Serial.println(mfrc522.GetStatusCodeName(status));
      endCardSession();
      return;
    }

    byte buffer[18];
    byte size = sizeof(buffer);
    status = mfrc522.MIFARE_Read((byte)block, buffer, &size);
    if (status != MFRC522::STATUS_OK) {
      Serial.print("ERR:READ_FAILED:");
      Serial.println(mfrc522.GetStatusCodeName(status));
      endCardSession();
      return;
    }

    Serial.print("OK:BLOCK:");
    Serial.print(block);
    Serial.print(":");
    printHexBytes(buffer, 16);
    Serial.print(":");
    printAsciiBytes(buffer, 16);
    Serial.println();

    endCardSession();
    return;
  }

  if (cmd.startsWith("WRITE_TEXT ") || cmd.startsWith("WRITE_HEX ")) {
    // Format: WRITE_TEXT <block> <A|B> <12_hex_key> <text>
    // Format: WRITE_HEX  <block> <A|B> <12_hex_key> <32_hex_chars>
    bool isHexMode = cmd.startsWith("WRITE_HEX ");
    int s1 = cmd.indexOf(' ');
    int s2 = cmd.indexOf(' ', s1 + 1);
    int s3 = cmd.indexOf(' ', s2 + 1);
    int s4 = cmd.indexOf(' ', s3 + 1);
    if (s2 < 0 || s3 < 0 || s4 < 0) {
      Serial.println("ERR:BAD_ARGS");
      return;
    }

    int block = cmd.substring(s1 + 1, s2).toInt();
    String keyTypeStr = cmd.substring(s2 + 1, s3);
    String keyHex = cmd.substring(s3 + 1, s4);
    String payload = cmd.substring(s4 + 1);

    // Koruma: Blok 0 (üretici) ve sector trailer bloklarına (3, 7, 11, ..., 63) doğrudan yazmayı engelle
    if (block <= 0 || block > 63 || (block % 4 == 3)) {
      Serial.println("ERR:PROTECTED_BLOCK");
      return;
    }

    MFRC522::PICC_Command authCmd;
    if (!parseAuthType(keyTypeStr, authCmd)) {
      Serial.println("ERR:INVALID_KEY_TYPE");
      return;
    }

    MFRC522::MIFARE_Key key;
    if (!parseKey(keyHex, key)) {
      Serial.println("ERR:INVALID_KEY");
      return;
    }

    byte dataBlock[16];
    if (isHexMode) {
      payload.trim();
      if (!parseHexBytes(payload, dataBlock, 16)) {
        Serial.println("ERR:INVALID_HEX_PAYLOAD");
        return;
      }
    } else {
      for (byte i = 0; i < 16; i++) {
        if (i < payload.length()) {
          dataBlock[i] = (byte)payload.charAt(i);
        } else {
          dataBlock[i] = 0x00; // Kalan byte'ları sıfır ile doldur
        }
      }
    }

    if (!waitForCard()) {
      Serial.println("ERR:NO_CARD");
      return;
    }

    MFRC522::StatusCode status = mfrc522.PCD_Authenticate(
      authCmd, (byte)block, &key, &(mfrc522.uid)
    );
    if (status != MFRC522::STATUS_OK) {
      Serial.print("ERR:AUTH_FAILED:");
      Serial.println(mfrc522.GetStatusCodeName(status));
      endCardSession();
      return;
    }

    status = mfrc522.MIFARE_Write((byte)block, dataBlock, 16);
    if (status != MFRC522::STATUS_OK) {
      Serial.print("ERR:WRITE_FAILED:");
      Serial.println(mfrc522.GetStatusCodeName(status));
      endCardSession();
      return;
    }

    Serial.print("OK:WRITTEN:");
    Serial.println(block);
    endCardSession();
    return;
  }

  if (cmd.startsWith("DUMP ")) {
    // Format: DUMP <A|B> <12_hex_key>
    // Kullanıcı tarafından girilen tek bilinen anahtarla 64 bloğu okur
    int s1 = cmd.indexOf(' ');
    int s2 = cmd.indexOf(' ', s1 + 1);
    if (s2 < 0) {
      Serial.println("ERR:BAD_ARGS");
      return;
    }

    String keyTypeStr = cmd.substring(s1 + 1, s2);
    String keyHex = cmd.substring(s2 + 1);
    keyHex.trim();

    MFRC522::PICC_Command authCmd;
    if (!parseAuthType(keyTypeStr, authCmd)) {
      Serial.println("ERR:INVALID_KEY_TYPE");
      return;
    }

    MFRC522::MIFARE_Key key;
    if (!parseKey(keyHex, key)) {
      Serial.println("ERR:INVALID_KEY");
      return;
    }

    if (!waitForCard()) {
      Serial.println("ERR:NO_CARD");
      return;
    }

    Serial.print("UID:");
    printHexBytes(mfrc522.uid.uidByte, mfrc522.uid.size);
    Serial.println();

    for (byte sector = 0; sector < 16; sector++) {
      byte firstBlock = sector * 4;
      MFRC522::StatusCode status = mfrc522.PCD_Authenticate(
        authCmd, firstBlock, &key, &(mfrc522.uid)
      );
      if (status != MFRC522::STATUS_OK) {
        Serial.print("SECTOR:");
        Serial.print(sector);
        Serial.println(":AUTH_FAILED");
        reselectCard();
        continue;
      }

      for (byte offset = 0; offset < 4; offset++) {
        byte block = firstBlock + offset;
        byte buffer[18];
        byte size = sizeof(buffer);
        status = mfrc522.MIFARE_Read(block, buffer, &size);
        Serial.print("BLOCK:");
        Serial.print(block);
        Serial.print(":");
        if (status == MFRC522::STATUS_OK) {
          printHexBytes(buffer, 16);
          Serial.print(":");
          printAsciiBytes(buffer, 16);
          Serial.println();
        } else {
          Serial.println("READ_FAILED");
        }
      }
    }

    Serial.println("OK:DUMP_DONE");
    endCardSession();
    return;
  }

  if (cmd.startsWith("RESET_ALL ")) {
    // Format: RESET_ALL <A|B> <12_hex_key>
    // Bilinen mevcut anahtar ile doğrulama yaparak veri bloklarını 0x00,
    // sector trailer bloklarını fabrika varsayılanına (FFFFFFFFFFFF + FF078069 + FFFFFFFFFFFF) sıfırlar.
    int s1 = cmd.indexOf(' ');
    int s2 = cmd.indexOf(' ', s1 + 1);
    if (s2 < 0) {
      Serial.println("ERR:BAD_ARGS");
      return;
    }

    String keyTypeStr = cmd.substring(s1 + 1, s2);
    String keyHex = cmd.substring(s2 + 1);
    keyHex.trim();

    MFRC522::PICC_Command authCmd;
    if (!parseAuthType(keyTypeStr, authCmd)) {
      Serial.println("ERR:INVALID_KEY_TYPE");
      return;
    }

    MFRC522::MIFARE_Key key;
    if (!parseKey(keyHex, key)) {
      Serial.println("ERR:INVALID_KEY");
      return;
    }

    if (!waitForCard()) {
      Serial.println("ERR:NO_CARD");
      return;
    }

    byte zeroBlock[16] = {0};
    byte factoryTrailer[16] = {
      0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, // Key A
      0xFF, 0x07, 0x80, 0x69,             // Fabrika erişim bitleri
      0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF  // Key B
    };

    for (byte sector = 0; sector < 16; sector++) {
      byte firstBlock = sector * 4;
      byte trailerBlock = firstBlock + 3;

      MFRC522::StatusCode status = mfrc522.PCD_Authenticate(
        authCmd, trailerBlock, &key, &(mfrc522.uid)
      );
      if (status != MFRC522::STATUS_OK) {
        Serial.print("SECTOR:");
        Serial.print(sector);
        Serial.println(":AUTH_FAILED");
        reselectCard();
        continue;
      }

      bool sectorOk = true;
      for (byte offset = 0; offset < 3; offset++) {
        byte block = firstBlock + offset;
        if (block == 0) {
          continue; // Blok 0 üretici bloğudur, yazılmaz
        }
        status = mfrc522.MIFARE_Write(block, zeroBlock, 16);
        if (status != MFRC522::STATUS_OK) {
          sectorOk = false;
        }
      }

      status = mfrc522.MIFARE_Write(trailerBlock, factoryTrailer, 16);
      if (status != MFRC522::STATUS_OK) {
        sectorOk = false;
      }

      Serial.print("SECTOR:");
      Serial.print(sector);
      Serial.println(sectorOk ? ":RESET_OK" : ":WRITE_FAILED");
    }

    Serial.println("OK:RESET_DONE");
    endCardSession();
    return;
  }

  Serial.println("ERR:UNKNOWN_CMD");
}
