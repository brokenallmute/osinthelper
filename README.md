# OSINT Helper

Быстрый консольный инструмент на C для OSINT и работы с большими базами данных (десятки гигабайт).

## Возможности

- Поиск по базам (CSV/TXT/LOG/JSON/SQL и др.) в папке `databases/`
- Пробив IP (страна, город, провайдер, ASN, координаты)
- Проверка email (реальная SMTP-валидация через NeverBounce)
- Временная почта (генерация, проверка входящих через Mail.tm)
- Красивое цветное CLI-меню, прогресс-бар, многопоточность

## Установка и запуск (Linux / Termux)

### Зависимости

Linux (Debian/Ubuntu):
```bash
sudo apt update
sudo apt install build-essential libcurl4-openssl-dev git
```

Arch:
```bash
sudo pacman -S base-devel curl git
```

Termux:
```bash
pkg update
pkg install clang make curl git
```

### Сборка

```bash
git clone https://github.com/yourusername/osinthelper.git
cd osinthelper
make
./osint-helper
```

## Как пользоваться

- **Базы**: кидаешь свои `.csv/.txt/.log/.json/.sql` в папку `databases/`
- В меню выбираешь:
  - `[1]` Поиск в базах — вводишь строку, получаешь все совпадения с указанием файла
  - `[2]` Пробив IP — вводишь IP, видишь страну/город/ISP/координаты
  - `[3]` Проверка Email — вводишь email, видишь статус (существует / нет)
  - `[4]` Временная почта — получаешь временный email, ждёшь письма
  - `[5]` Список баз — постраничный вывод всех файлов и их размеров

## Особенности

- Поиск по ~18–19 GB выполняется за ~8 секунд на SSD (несколько ГБ/с)
- Используются: `mmap`, `memmem`, `pthread`, сортировка файлов по размеру
- Работает и на ПК (Linux x86_64), и в Termux (Android, ARM) — проект пересобирается под нужную архитектуру

## Важное

- Папка с базами: `./databases`
- Проект чисто консольный, без GUI
- Используй только для легальных задач OSINT
