# MYFS — учебная файловая система поверх блочного устройства

Репозиторий содержит:

- модуль ядра Linux с регистрацией файловой системы в VFS;
- две копии superblock в заданных секторах блочного устройства;
- проверку целостности superblock через CRC32;
- автоматическое создание фиксированного набора файлов при инициализации ФС;
- чтение и запись в файлы из userspace;
- IOCTL-команды для обнуления файлов, стирания ФС, получения хэшей файлов и получения маппинга секторов;
- userspace CLI-программу для проверки работы.

Целевая версия ядра: `6.12.x`.

---

## 1. Принятая модель ФС

ФС работает поверх уже существующего блочного устройства, например `/dev/loop10`.

Superblock хранится в двух копиях:

- первая копия — в секторе `sb1_sector`;
- вторая копия — в секторе `sb2_sector`.

Оба сектора задаются параметрами модуля.

Все остальные сектора считаются областью данных. Эта область делится на предсозданные файлы одинакового размера.

Если параметр `file_size_sectors = M`, то каждый файл занимает `M` секторов.

Имена файлов генерируются автоматически:

```text
file_000000
file_000001
file_000002
...
```

Количество файлов рассчитывается так:

```text
file_count = (total_sectors - 2) / file_size_sectors
```

Два сектора вычитаются под две копии superblock. Если после деления остается хвост меньше размера файла, он не используется.

Секторы superblock исключаются из маппинга файлов. Например, если `sb1_sector=0`, `sb2_sector=128`, то данные файлов будут занимать все сектора, кроме `0` и `128`.

---

## 2. Структура репозитория

```text
.
├── include/
│   └── myfs_ioctl.h          # общий заголовок IOCTL для ядра и userspace
│
├── kernel/
│   ├── Makefile
│   ├── main.c                # регистрация ФС и параметры модуля
│   ├── super.c               # superblock, форматирование, восстановление копии
│   ├── inode.c               # inode-модель и маппинг inode -> file_id
│   ├── dir.c                 # lookup и вывод файлов в директории
│   ├── file.c                # read/write и CRC32 файлов
│   ├── ioctl.c               # IOCTL-команды
│   └── myfs.h
│
├── userspace/
│   ├── Makefile
│   └── myfsctl.c             # CLI для тестирования и IOCTL
│
└── scripts/
    ├── create-loop.sh
    ├── load.sh
    ├── mount.sh
    ├── umount.sh
    └── test.sh
```

---

## 3. Зависимости

Нужны:

```bash
sudo apt update
sudo apt install -y build-essential linux-headers-$(uname -r) util-linux
```

---

## 4. Сборка

Из корня репозитория:

```bash
make -C kernel
make -C userspace
```

После сборки появятся:

```text
kernel/myfs.ko
userspace/myfsctl
```

Если текущее ядро не `6.12.x`, собирайте так:

```bash
make -C kernel KDIR=/path/to/linux-6.12-build
make -C userspace
```

---

## 5. Создание тестового блочного устройства

Создать loop-устройство на базе файла:

```bash
scripts/create-loop.sh /tmp/myfs.img 64
```

Скрипт создаст файл `/tmp/myfs.img` размером 64 МБ и подключит его как loop device.

В выводе будет устройство, например:

```text
/dev/loop23
```

Проверить loop-устройства можно так:

```bash
losetup -a
```

---

## 6. Загрузка модуля

Через скрипт:

```bash
scripts/load.sh /dev/loop23 0 128 32 4
```

Параметры модуля:

| Параметр | Назначение |
|---|---|
| `disk_name` | блочное устройство, на котором разрешено монтировать ФС |
| `sb1_sector` | сектор первой копии superblock |
| `sb2_sector` | сектор второй копии superblock |
| `max_filename_len` | максимальная длина имени файла |
| `file_size_sectors` | размер одного файла в секторах |

Ограничения:

- `disk_name` обязателен;
- `sb1_sector` и `sb2_sector` должны быть разными;
- оба сектора superblock должны находиться внутри блочного устройства;
- `file_size_sectors >= 1`;
- `max_filename_len >= 11`, потому что минимальное имя имеет вид `file_000000`.

Посмотреть параметры загруженного модуля:

```bash
cat /sys/module/myfs/parameters/disk_name
cat /sys/module/myfs/parameters/sb1_sector
cat /sys/module/myfs/parameters/sb2_sector
cat /sys/module/myfs/parameters/max_filename_len
cat /sys/module/myfs/parameters/file_size_sectors
```

Посмотреть сообщения модуля:

```bash
sudo dmesg | tail -100
```

---

## 7. Монтирование

Через скрипт:

```bash
scripts/mount.sh /dev/loop23 /mnt
```

При первом монтировании модуль:

1. пытается прочитать обе копии superblock;
2. проверяет `magic`, версию, параметры и CRC32;
3. если валидна одна копия — восстанавливает вторую;
4. если обе копии отсутствуют или повреждены — форматирует новую ФС;
5. регистрирует root directory и предсозданные файлы в VFS.

Проверить список файлов:

```bash
ls -la /mnt | head
```

Пример ожидаемого вывода:

```text
итого 65538
drwxr-xr-x  2 root root 32767 мая 24 14:22 .
drwxr-xr-x 20 root root  4096 мая 24 13:55 ..
-rw-r--r--  1 ilya ilya  2048 мая 24 14:23 file_000000
-rw-r--r--  1 ilya ilya  2048 мая 24 14:23 file_000001
-rw-r--r--  1 ilya ilya  2048 мая 24 14:23 file_000002
-rw-r--r--  1 ilya ilya  2048 мая 24 14:23 file_000003
...
```

---

## 8. Проверка чтения и записи

CLI-программа `myfsctl` обходит все файлы в `/mnt`, в каждый файл пишет случайное `uint64_t`, затем читает его обратно и сравнивает значение.

```bash
sudo userspace/myfsctl test /mnt
```

Ожидаемый результат:

```text
Checked files: 32767
OK
```

---

## 9. IOCTL-команды

IOCTL вызываются через userspace-программу `myfsctl`.

### 9.1. Обнулить все файлы

```bash
sudo userspace/myfsctl zero /mnt
```

Что делает:

- проходит по всем секторам, принадлежащим файлам;
- записывает нули;
- superblock не трогает.

Проверка:

```bash
sudo userspace/myfsctl zero /mnt
sudo userspace/myfsctl hashes /mnt | head
```

После обнуления у одинаковых по размеру файлов CRC32 должен быть одинаковым.

---

### 9.2. Стереть ФС

```bash
sudo userspace/myfsctl erase /mnt
```

Что делает:

- обнуляет все файлы;
- обнуляет обе копии superblock;
- помечает текущий in-memory экземпляр ФС как стертый.

После этого нужно размонтировать ФС:

```bash
sudo umount /mnt
```

При следующем монтировании модуль увидит, что валидных superblock нет, и создаст ФС заново.

```bash
sudo mount -t myfs /dev/loop10 /mnt
```

---

### 9.3. Получить список хэшей файлов

```bash
sudo userspace/myfsctl hashes /mnt
```

Пример вывода:

```text
file_000000 crc32=0x2144df1c
file_000001 crc32=0x2144df1c
file_000002 crc32=0x8d13a912
...
```

Хэш считается по всем секторам файла.

---

### 9.4. Получить маппинг секторов для файла

```bash
sudo userspace/myfsctl mapping /mnt file_000000
```

Пример вывода для `file_size_sectors=4`:

```text
file_000000: file_id=0 sector_count=4
  [0] sector=1
  [1] sector=2
  [2] sector=3
  [3] sector=4
```

Если один из superblock находится внутри области данных, маппинг автоматически пропустит этот сектор.

Например, при `sb1_sector=0`, `sb2_sector=128` никакой файл не будет использовать физические сектора `0` и `128`.

---

## 10. Ход установки и проверки работоспособности

Ниже пример полного запуска с чистого образа.

```bash
# 1. Сборка
make -C kernel
make -C userspace

# 2. Создание loop device
scripts/create-loop.sh /tmp/myfs.img 64

# Допустим, скрипт вернул /dev/loop10

# 3. Загрузка модуля
scripts/load.sh /dev/loop10 0 128 32 4

# 4. Монтирование
scripts/mount.sh /dev/loop10 /mnt

# 5. Проверка, что файлы видны из userspace
ls -la /mnt | head

# 6. Проверка read/write по всем файлам
sudo userspace/myfsctl test /mnt

# 7. Проверка IOCTL mapping
sudo userspace/myfsctl mapping /mnt file_000000
sudo userspace/myfsctl mapping /mnt file_000001

# 8. Проверка IOCTL hashes
sudo userspace/myfsctl hashes /mnt | head

# 9. Проверка IOCTL zero
sudo userspace/myfsctl zero /mnt
sudo userspace/myfsctl hashes /mnt | head

# 10. Проверка IOCTL erase
sudo userspace/myfsctl erase /mnt
sudo umount /mnt

# 11. Повторное монтирование после erase: ФС должна создаться заново
sudo mount -t myfs /dev/loop10 /mnt
ls -la /mnt | head
sudo userspace/myfsctl test /mnt

# 12. Завершение
sudo umount /mnt
sudo rmmod myfs
sudo losetup -d /dev/loop10
rm -f /tmp/myfs.img
```

---
