# Chat App (Cross-Platform)

## Cài đặt

### Windows (MSYS2)

#### 1. Cài MSYS2
https://www.msys2.org/

#### 2. Cài thư viện
Mở **MSYS2 MinGW 64-bit** và chạy:
```bash
pacman -S mingw-w64-x86_64-gcc mingw-w64-x86_64-gtk3 make
```

#### 3. Build
```bash
make all
```

### Linux (Ubuntu/Debian)

#### 1. Cài thư viện
```bash
sudo apt install g++ libgtk-3-dev make
```

#### 2. Build
```bash
make all
```

## Chạy

### Windows
```bash
./bin/server.exe    # Server
./bin/client.exe    # Client
```

### Linux
```bash
./bin/server    # Server
./bin/client    # Client
```

