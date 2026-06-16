# Final C - SSL/TLS Secure Multi-User Chat System

## Project Overview

這份專案是網路程式設計（Network Programming）Final Project 的 C 題實作。專案的主要目標，是在原本 TCP Socket Chat Room 的基礎上，進一步整合 **OpenSSL** 與 **SSL/TLS 加密機制**，建立一個具備安全通訊能力（Secure Communication）的多人聊天室系統。

在一般的 TCP Chat Room 中，雖然可以可靠地傳送資料，但所有聊天內容都是以明文（Plain Text）的形式進行傳輸。若通訊封包遭到攔截，第三方便有機會直接讀取聊天內容。因此，我希望透過這份作業，將課堂中學習到的 Socket Programming 與 SSL/TLS 技術結合，讓聊天室不只是能夠聊天，而是能夠 **安全地聊天（Communicate Securely）**。

在本專案中，Client 與 Server 仍然以 TCP Socket 作為基礎架構，但所有資料傳輸皆透過：

```c
SSL_read()
SSL_write()
```

進行加密傳輸，而不再直接使用：

```c
recv()
send()
```

因此，所有聊天室訊息、私人訊息、指令回應以及 Bonus 功能，皆受到 TLS 保護。

換句話說，這份專案不只是：

> Make users chat with each other.

而是：

> Make users communicate securely through SSL/TLS.

---

## Why I Built This Project

在 Midterm 時，我已經完成了一個 Multi-user Chat Room，支援多人聊天、聊天紀錄以及線上使用者管理。

到了 Final，我希望在不破壞原有架構的前提下，進一步加入：

* SSL/TLS Encryption
* TLS Handshake
* Secure Private Message
* User Session Information
* Chat Statistics
* User Activity Ranking

讓這份作業從一個基本聊天室，逐漸擴充成：

**A Secure Multi-user Chat System with User Analytics and Encrypted Private Messaging**

---

## Main Features

### Required Features (Instructor Requirements)

#### ✅ Your Server can accept SSL/TLS connection

Server 使用 OpenSSL 建立 TLS Server Context，並透過：

```c
SSL_accept()
```

接受 Client 的 TLS 連線請求，完成 TLS Handshake。

連線成功後，Server 會顯示：

```text
TLS Chat Server started on port 8080.
Waiting for SSL/TLS clients...
```

---

#### ✅ Your Client can issue SSL/TLS connection to server

Client 建立 TCP Connection 後，透過：

```c
SSL_connect()
```

與 Server 建立 SSL/TLS 安全通道。

成功連線後會顯示：

```text
Connected securely.
TLS=TLSv1.3
Cipher=TLS_AES_256_GCM_SHA384
```

代表目前聊天室已經透過 TLS 進行加密傳輸。

---

#### ✅ Your Server can distribute Chats among multiple SSL/TLS connected users

Server 使用：

```c
SSL_write()
SSL_read()
```

進行所有訊息傳輸。

每位使用者皆擁有自己的：

```c
SSL *ssl
```

因此，即使有多位 Client 同時在線聊天，所有資料仍然透過 TLS 保護。

---

# Bonus Features

除了老師指定的 SSL/TLS 功能外，我另外加入了一些額外功能，希望讓聊天室更加完整且具有互動性。

---

### `/help`

顯示所有支援的指令。

```text
/help
```

---

### `/list`

顯示目前在線上的使用者。

```text
/list

[Online] Wayne Mark Gina
```

---

### `/secure`

顯示目前連線所使用的 TLS Version 與 Cipher Suite。

```text
/secure

TLS=TLSv1.3
Cipher=TLS_AES_256_GCM_SHA384
```

---

### `/stats`

顯示聊天室統計資訊。

```text
/stats

Online users : 3
Total joins : 3
Total public messages : 18
Server uptime : 135 seconds
```

---

### `/dm <name> <message>`

TLS 加密私人訊息。

```text
/dm Mark Hello Mark
```

只有：

* 發送者
* 接收者

可以看見訊息，其餘使用者不會收到。

---

### `/me <action>`

動作訊息（Action Message）。

```text
/me is presenting Final Project
```

聊天室會顯示：

```text
* Wayne is presenting Final Project
```

---

### `/shout <message>`

大寫廣播訊息。

```text
/shout tls secure chat success
```

輸出：

```text
[SHOUT] Wayne:
TLS SECURE CHAT SUCCESS
```

---

### `/rename <newname>`

修改使用者暱稱。

```text
/rename SuperWayne
```

聊天室：

```text
[Server]
Wayne is now known as SuperWayne
```

---

### `/whois <name>` ⭐

查看指定使用者的 Session Information。

```text
/whois Mark
```

輸出：

```text
Name           : Mark
Connected Time : 2026-06-17 13:25:01
Online Time    : 15 min 30 sec
Messages Sent  : 8

TLS Version    : TLSv1.3
Cipher Suite   : TLS_AES_256_GCM_SHA384
```

我希望透過這個功能，讓聊天室不只是聊天工具，也能夠查看使用者的安全連線資訊。

---

### `/top` ⭐

聊天室活躍排行榜。

```text
/top
```

輸出：

```text
========== Chat Activity Ranking ==========

1. Wayne    20 messages
2. Mark      8 messages
3. Gina      5 messages

Most Active User : Wayne
```

依照使用者發送的訊息數量進行排序，增加聊天室的互動性。

---

## Self Checklist

| Category | Feature                        | Status |
| -------- | ------------------------------ | ------ |
| Required | Server SSL/TLS Connection      | ✅      |
| Required | Client SSL/TLS Connection      | ✅      |
| Required | Multi-user SSL/TLS Chat        | ✅      |
| Bonus    | /help                          | ✅      |
| Bonus    | /list                          | ✅      |
| Bonus    | /secure                        | ✅      |
| Bonus    | /stats                         | ✅      |
| Bonus    | /dm                            | ✅      |
| Bonus    | /me                            | ✅      |
| Bonus    | /shout                         | ✅      |
| Bonus    | /rename                        | ✅      |
| Bonus    | /whois                         | ✅      |
| Bonus    | /top                           | ✅      |
| Bonus    | Chat History                   | ✅      |
| Bonus    | Cross-platform (Windows/Linux) | ✅      |

---

## Conclusion

這份專案讓我更深入理解了 Socket Programming 與 SSL/TLS 的實際運作方式。

從最初的 TCP Chat Room，到後來整合 OpenSSL、建立 TLS Handshake，再加入 Session Information、Private Messaging 與 User Analytics，我逐漸了解到，一個安全的網路應用程式不只是「能傳送資料」，更重要的是：

> **How to protect the data while communicating.**

對我而言，這份 Final Project 不只是完成課堂作業，更是一個將 Socket、TLS 與系統設計整合在一起的實作練習，也是我第一次親手完成一個真正具備安全通訊能力的多人聊天室系統。
