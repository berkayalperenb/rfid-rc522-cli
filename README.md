# RFID‑RC522 CLI  

MFRC522 RFID okuyucu için kompakt, çapraz platform bir komut satırı arabirimi. Proje şunları içerir:

- **Arduino firmware** (`arduino/rfid_rc522_spi`) Arduino Uno/Nano üzerinde çalışır ve basit bir seri protokol sağlar.
- **Python konsol istemcisi** (`cli/rfid_console.py`) Arduino ile seri port üzerinden iletişim kurar ve MIFARE Classic kartları okuma, yazma ve fabrika ayarlarına sıfırlama işlemlerini menü tabanlı bir arayüzle sunar.

Depo, Arduino IDE ve Python 3 dışındaki hiçbir bağımlılık gerektirmeden sistemi derlemek, flaşlamak ve çalıştırmak için düzenlenmiştir.

---

## İçindekiler

1. [Donanım Gereksinimleri](#donanım-gereksinimleri)
2. [Yazılım Gereksinimleri](#yazılım-gereksinimleri)
3. [Başlarken](#başlarken)
   - [Arduino Firmware](#arduino-firmware)
   - [Python CLI](#python-cli)
4. [Desteklenen Komutlar](#desteklenen-komutlar)
5. [Proje Yapısı](#proje-yapısı)
6. [Katkıda Bulunma](#katkıda-bulunma)
7. [Lisans](#lisans)

---

## Donanım Gereksinimleri

| Bileşen | Önerilen Parça |
|---------|----------------|
| MCU     | Arduino Uno veya Nano |
| RFID Modülü | MFRC522 (SPI) |
| Güç     | 3.3 V (VCC’yi **5 V**’a bağlamayın) |
| Bağlantılar | Aşağıdaki şemaya bakın |

```
RC522 Pin   → Arduino Pin
3.3V       → 3.3V
GND        → GND
RST        → D9
SDA (SS)   → D10
MOSI       → D11
MISO       → D12
SCK        → D13
```

> **Not:** RC522 modülü 3.3 V ile çalışır. 5 V bağlamak çipin yanmasına neden olur.

---

## Yazılım Gereksinimleri

- **Arduino IDE** (veya PlatformIO) ve `MFRC522` kütüphanesi yüklü olmalı.
- **Python 3.8+**
- `pyserial` paketi (`pip install -r requirements.txt`)

---

## Başlarken

### Arduino Firmware
1. `arduino/rfid_rc522_spi/rfid_rc522_spi.ino` dosyasını Arduino IDE’de açın.
2. **Sketch → Include Library → Manage Libraries…** üzerinden **MFRC522** kütüphanesini kurun.
3. **Tools → Board** ve **Tools → Port** menülerinden doğru kartı ve bağlantı noktasını seçin.
4. **Upload** tuşuna basın. Sketch, seri port açıldığında `READY:RC522_SPI` mesajını verir.

### Python CLI
1. Terminalde repo kök dizinine geçin:
   ```
   cd cli
   pip install -r requirements.txt
   ```
2. İstemciyi çalıştırın:
   ```
   python rfid_console.py
   ```
3. Ekranda çıkan menüyü izleyin. Program, bağlı COM portunu seçmenizi ister ve ardından desteklenen komutları çalıştırabilirsiniz.

---

## Desteklenen Komutlar
| Komut | Açıklama |
|-------|----------|
| `PING` | Basit sağlık kontrolü – `PONG` döner. |
| `UID` | Kartın UID’sini okur ve ekrana bastırır. |
| `READ <blok> <A|B> <12‑hex‑key>` | Belirtilen Key A ya da Key B ile kimlik doğrulama yapar ve 16 byte’lık bir bloğu okur. |
| `WRITE_TEXT <blok> <A|B> <12‑hex‑key> <metin>` | En fazla 16 ASCII karakteri belirtilen bloğa yazar. |
| `WRITE_HEX <blok> <A|B> <12‑hex‑key> <32‑hex‑karakter>` | 16 byte raw veriyi onaltılık (hex) biçiminde yazar. |
| `DUMP <A|B> <12‑hex‑key>` | Tek bir bilinen anahtar ile **tüm 64 blok**u okur ve hex/ASCII çıktısı verir. |
| `RESET_ALL <A|B> <12‑hex‑key>` | Bilinen bir anahtar ile kimlik doğrulama yapar, tüm veri bloklarını `0x00` ve sektör trailer’larını fabrika varsayılanına (Key A/B: `FFFFFFFFFFFF`, Access: `FF078069`) sıfırlar. |

Tüm komutlar, işlenme sonuçlarını `OK:` ya da `ERR:` ile başlatılan satırlar olarak döndürür; bu da otomatik ayrıştırmayı kolaylaştırır.

---

## Proje Yapısı
```
rfid-rc522-cli/
│   README.md                # Bu dosya
│  
│
├── arduino/
│   └── rfid_rc522_spi/
│       └── rfid_rc522_spi.ino   # Arduino firmware
│
└── cli/
    ├── requirements.txt    # Python bağımlılıkları (pyserial)
    └── rfid_console.py     # Python komut satırı istemcisi
```

---


