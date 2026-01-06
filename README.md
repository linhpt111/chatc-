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

## Test

### Test trên cùng 1 máy

1. **Mở Terminal 1** - Chạy Server:
   ```bash
   ./bin/server.exe
   ```

2. **Mở Terminal 2** - Chạy Client 1:
   ```bash
   ./bin/client.exe
   ```
   - Server: `127.0.0.1`
   - Port: `8888`
   - Username: `user1`

3. **Mở Terminal 3** - Chạy Client 2:
   ```bash
   ./bin/client.exe
   ```
   - Server: `127.0.0.1`
   - Port: `8888`
   - Username: `user2`

4. **Test các chức năng:**
   - ✅ Chat trực tiếp: Click vào user trong danh sách Online Users
   - ✅ Tạo group: Nhập tên group → Create Group
   - ✅ Chat group: Click vào group → gửi tin nhắn
   - ✅ Gửi file: Click Send File → chọn file
   - ✅ Chơi Caro: Click 🎮 bên cạnh user online

### Test qua mạng LAN

1. **Máy Server** - Tìm IP:
   ```cmd
   ipconfig    # Windows
   ip addr     # Linux
   ```

2. **Máy Server** - Mở firewall port 8888:
   ```cmd
   netsh advfirewall firewall add rule name="Chat Server" dir=in action=allow protocol=TCP localport=8888
   ```

3. **Máy Server** - Chạy server:
   ```bash
   ./bin/server.exe
   ```

4. **Máy Client** - Kết nối:
   - Server: `<IP máy server>` (VD: `192.168.1.100`)
   - Port: `8888`
   - Username: `tên tùy chọn`

### Gửi Client cho người khác (không cần cài đặt)

```bash
# Tạo bản release với đầy đủ DLL
mkdir -p release
cp bin/client.exe release/
ldd bin/client.exe | grep -E "ucrt64|mingw64" | awk '{print $3}' | xargs -I{} cp {} release/
```

Nén thư mục `release/` thành ZIP và gửi cho bạn bè.

