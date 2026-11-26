import sys
import time
from typing import Optional
import serial
import serial.tools.list_ports

DEFAULT_KEY = "FFFFFFFFFFFF"
HEX_CHARS = set("0123456789ABCDEF")


def select_serial_port() -> str:
    ports = list(serial.tools.list_ports.comports())
    if not ports:
        print("Hata: Bağlı seri port (COM) bulunamadı.")
        sys.exit(1)

    print("\nKullanılabilir Seri Portlar:")
    for idx, port in enumerate(ports, start=1):
        print(f"  [{idx}] {port.device} - {port.description}")

    while True:
        choice = input("\nArduino port numarasını seçin (örn. 1): ").strip()
        if choice.isdigit():
            idx = int(choice)
            if 1 <= idx <= len(ports):
                return ports[idx - 1].device
        print("Geçersiz seçim, tekrar deneyin.")


def connect_arduino(port_name: str, baudrate: int = 115200) -> serial.Serial:
    ser = serial.Serial(port_name, baudrate, timeout=10)
    # Arduino seri port açılışında resetlenir; hazırlanmasını bekle
    time.sleep(2.2)
    ser.reset_input_buffer()
    ser.write(b"PING\n")
    resp = ser.readline().decode("utf-8", errors="replace").strip()
    if resp == "PONG" or resp.startswith("READY:"):
        print(f"Bağlantı başarılı ({port_name}): {resp}")
    else:
        print(f"Uyarı: Beklenmeyen ilk yanıt ({resp}), devam ediliyor...")
    return ser


def prompt_block(allow_protected: bool) -> Optional[int]:
    block_str = input("Blok numarası (0-63): ").strip()
    if not block_str.isdigit():
        print("Hata: Geçerli bir sayı girin.")
        return None
    block = int(block_str)
    if not (0 <= block <= 63):
        print("Hata: Blok 0 ile 63 arasında olmalıdır.")
        return None
    if not allow_protected and (block == 0 or block % 4 == 3):
        print("Hata: Blok 0 ve sector trailer blokları (3, 7, 11, ...) koruma altındadır.")
        return None
    return block


def prompt_key_type() -> str:
    while True:
        key_type = input("Anahtar tipi seçin (A veya B) [A]: ").strip().upper()
        if not key_type:
            return "A"
        if key_type in ("A", "B"):
            return key_type
        print("Lütfen A veya B girin.")


def prompt_key(label: str) -> Optional[str]:
    raw = input(f"{label} [{DEFAULT_KEY}]: ").strip().replace(" ", "").upper()
    if not raw:
        return DEFAULT_KEY
    if len(raw) != 12 or any(c not in HEX_CHARS for c in raw):
        print("Hata: Anahtar tam olarak 12 hex karakter olmalıdır (örn: FFFFFFFFFFFF).")
        return None
    return raw


def send_and_read_single(ser: serial.Serial, command: str) -> str:
    ser.reset_input_buffer()
    ser.write((command + "\n").encode("utf-8"))
    resp = ser.readline().decode("utf-8", errors="replace").strip()
    return resp if resp else "ERR:TIMEOUT"


def read_stream_until(ser: serial.Serial, done_marker: str) -> None:
    while True:
        line = ser.readline().decode("utf-8", errors="replace").strip()
        if not line:
            print("Zaman aşımı veya bağlantı kesildi.")
            break
        print(f"  {line}")
        if line == done_marker or line.startswith("ERR:"):
            break


def cmd_read_uid(ser: serial.Serial) -> None:
    print("Kartı RC522 okuyucuya yaklaştırın (8 sn bekleniyor)...")
    resp = send_and_read_single(ser, "UID")
    print(f"Sonuç: {resp}")


def cmd_read_block(ser: serial.Serial) -> None:
    block = prompt_block(allow_protected=True)
    if block is None:
        return
    key_type = prompt_key_type()
    key = prompt_key("6 byte Hex Key")
    if key is None:
        return

    print("Kartı RC522 okuyucuya yaklaştırın...")
    resp = send_and_read_single(ser, f"READ {block} {key_type} {key}")
    print(f"Sonuç: {resp}")


def cmd_dump_all(ser: serial.Serial) -> None:
    key_type = prompt_key_type()
    key = prompt_key("Tüm 64 blok için bilinen tek Hex Key")
    if key is None:
        return

    print("Kartı RC522 okuyucuya yaklaştırın (okuma bitene kadar sabit tutun)...")
    ser.reset_input_buffer()
    ser.write(f"DUMP {key_type} {key}\n".encode("utf-8"))
    read_stream_until(ser, "OK:DUMP_DONE")


def cmd_write_text(ser: serial.Serial) -> None:
    block = prompt_block(allow_protected=False)
    if block is None:
        return
    key_type = prompt_key_type()
    key = prompt_key("6 byte Hex Key")
    if key is None:
        return

    text = input("Yazılacak karakter/metin (en fazla 16 karakter): ")
    if len(text) > 16:
        print("Uyarı: Metin 16 karakterden uzun olduğu için ilk 16 karakter yazılacak.")
        text = text[:16]

    print("Kartı RC522 okuyucuya yaklaştırın...")
    resp = send_and_read_single(ser, f"WRITE_TEXT {block} {key_type} {key} {text}")
    print(f"Sonuç: {resp}")


def cmd_write_hex(ser: serial.Serial) -> None:
    block = prompt_block(allow_protected=False)
    if block is None:
        return
    key_type = prompt_key_type()
    key = prompt_key("6 byte Hex Key")
    if key is None:
        return

    hex_input = input("Yazılacak 16 byte HEX veri (32 hex karakter): ").strip().replace(" ", "").upper()
    if len(hex_input) != 32 or any(c not in HEX_CHARS for c in hex_input):
        print("Hata: Veri tam olarak 32 hex karakter (16 byte) olmalıdır.")
        return

    print("Kartı RC522 okuyucuya yaklaştırın...")
    resp = send_and_read_single(ser, f"WRITE_HEX {block} {key_type} {key} {hex_input}")
    print(f"Sonuç: {resp}")


def cmd_reset_all(ser: serial.Serial) -> None:
    print("UYARI: Bu işlem karttaki veri bloklarını (0x00) ve sektör şifrelerini")
    print("fabrika ayarına (Key A/B: FFFFFFFFFFFF, Access: FF078069) çevirir.")
    if input("Devam etmek için ONAY yazın: ").strip() != "ONAY":
        print("İşlem iptal edildi.")
        return

    key_type = prompt_key_type()
    key = prompt_key("Karttaki mevcut geçerli Hex Key")
    if key is None:
        return

    print("Kartı RC522 okuyucuya yaklaştırın (sıfırlama bitene kadar sabit tutun)...")
    ser.reset_input_buffer()
    ser.write(f"RESET_ALL {key_type} {key}\n".encode("utf-8"))
    read_stream_until(ser, "OK:RESET_DONE")


def main() -> None:
    print("=== Arduino RC522 SPI RFID Python CMD Konsolu ===")
    port_name = select_serial_port()
    ser = connect_arduino(port_name)

    try:
        while True:
            print("\n--- Menü ---")
            print("[1] Kart UID Oku")
            print("[2] Belirli Bir Bloğu Oku (Key A / Key B ile)")
            print("[3] Tüm 64 Bloğu Oku (Bilinen Tek Key A veya Key B ile)")
            print("[4] Bloğa Karakter (Metin) Yaz")
            print("[5] Bloğa Hex Yaz (16 Byte / 32 Hex Karakter)")
            print("[6] Tüm Veri ve Şifreleri Fabrika Ayarlarına Sıfırla")
            print("[0] Çıkış")

            choice = input("Seçiminiz: ").strip()
            if choice == "1":
                cmd_read_uid(ser)
            elif choice == "2":
                cmd_read_block(ser)
            elif choice == "3":
                cmd_dump_all(ser)
            elif choice == "4":
                cmd_write_text(ser)
            elif choice == "5":
                cmd_write_hex(ser)
            elif choice == "6":
                cmd_reset_all(ser)
            elif choice == "0":
                print("Çıkılıyor...")
                break
            else:
                print("Geçersiz menü seçimi.")
    finally:
        ser.close()
        print("Seri port kapatıldı.")


if __name__ == "__main__":
    main()
