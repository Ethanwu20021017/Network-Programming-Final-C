# Final C — SSL/TLS Secure Multi-User Chat System

## 1. Project Overview

This project is the final assignment Part C for the networking course. The main goal of this program is to extend a basic TCP chat room into a **secure multi-user chat system** by integrating **SSL/TLS encryption** through OpenSSL.

In the original socket-based chat room, messages are transmitted directly through TCP. Although TCP can deliver data reliably, the content itself is not encrypted. Therefore, if someone captures the network traffic, the message content may be exposed. To improve the security of the chat system, this project adds a TLS layer between the TCP socket and the chat application logic.

In this implementation, both the server and client still use socket programming as the foundation, but all chat messages are transmitted through:

```c
SSL_read()
SSL_write()
```

instead of directly using plain TCP `recv()` and `send()`.

As a result, the system can provide a secure encrypted chat environment while still preserving the original multi-user chat room design.

The final system supports:

- SSL/TLS encrypted server connection
- SSL/TLS encrypted client connection
- Multiple SSL/TLS clients chatting at the same time
- Online user list
- Chat history
- Private encrypted message
- Server statistics
- TLS security information display
- User session information
- Chat activity ranking
- Several user-friendly bonus commands

---

## 2. Files

This project contains two main source files:

```text
Final_C_server.c
Final_C_client.c
```

### `Final_C_server.c`

This file implements the TLS-secured chat server. It is responsible for:

- Creating the TCP listening socket
- Loading the server certificate and private key
- Accepting SSL/TLS client connections
- Performing TLS handshake with each client
- Managing multiple online users
- Broadcasting messages to all connected users
- Handling user commands
- Recording chat history
- Providing bonus features such as `/whois` and `/top`

### `Final_C_client.c`

This file implements the TLS-secured chat client. It is responsible for:

- Connecting to the server
- Performing SSL/TLS handshake
- Sending the user's nickname to the server
- Sending user input through TLS
- Receiving encrypted messages from the server
- Printing the available command list for users

---

## 3. Required Libraries

This project uses:

- C language
- TCP socket programming
- OpenSSL
- WinSock2 on Windows
- POSIX socket APIs on Linux/macOS

The code is written with cross-platform support. The program uses conditional compilation to select the correct socket library depending on the operating system:

```c
#ifdef _WIN32
    // Windows socket headers and WinSock initialization
#else
    // Linux/macOS socket headers
#endif
```

Therefore, the same source code can be compiled on both Windows-based and Linux-based environments.

---

## 4. How to Compile

### 4.1 Windows / MSYS2 MinGW

If using MSYS2 MinGW on Windows, compile with:

```bash
gcc Final_C_server.c -o Final_C_server.exe -lssl -lcrypto -lws2_32
gcc Final_C_client.c -o Final_C_client.exe -lssl -lcrypto -lws2_32
```

The `-lws2_32` option is required on Windows because the socket functions rely on the WinSock2 library.

---

### 4.2 Linux / macOS

On Linux or macOS, compile with:

```bash
gcc Final_C_server.c -o Final_C_server -lssl -lcrypto
gcc Final_C_client.c -o Final_C_client -lssl -lcrypto
```

If OpenSSL development files are not installed, install them first.

For Ubuntu / Debian:

```bash
sudo apt update
sudo apt install build-essential libssl-dev
```

---

## 5. Generate Certificate and Private Key

Before running the TLS server, a certificate and private key are required.

For homework and local testing, a self-signed certificate can be generated using:

```bash
openssl req -x509 -newkey rsa:2048 -keyout key.pem -out cert.pem -days 365 -nodes
```

This command generates:

```text
cert.pem
key.pem
```

In this project:

- `cert.pem` is used as the server certificate.
- `key.pem` is used as the server private key.
- The server uses these files during the TLS handshake.
- After the TLS handshake, the actual chat messages are encrypted by session keys negotiated by TLS.

The key does not need to be regenerated when adding chat commands such as `/top` or `/whois`, because those commands are application-layer messages transmitted inside the already established TLS secure channel.

---

## 6. How to Run

### 6.1 Start the Server

On Windows:

```bash
./Final_C_server.exe 8080 cert.pem key.pem
```

On Linux/macOS:

```bash
./Final_C_server 8080 cert.pem key.pem
```

Expected server output:

```text
TLS Chat Server started on port 8080.
Certificate: cert.pem | Key: key.pem
Waiting for SSL/TLS clients...
Bonus enabled: /help, /secure, /stats, /whois, /top, /dm, /me, /shout, /rename
```

---

### 6.2 Start Clients

If the server and clients are running on the same computer, use `127.0.0.1`:

```bash
./Final_C_client 127.0.0.1 Wayne 8080
./Final_C_client 127.0.0.1 Mark 8080
./Final_C_client 127.0.0.1 Gina 8080
```

On Windows:

```bash
./Final_C_client.exe 127.0.0.1 Wayne 8080
./Final_C_client.exe 127.0.0.1 Mark 8080
./Final_C_client.exe 127.0.0.1 Gina 8080
```

`127.0.0.1` means localhost, which means the client connects to the server running on the same machine.

If the server is running on another machine, replace `127.0.0.1` with the server IP address.

Example:

```bash
./Final_C_client 192.168.1.10 Wayne 8080
```

---

## 7. Main SSL/TLS Design

### 7.1 Server Side TLS

The server creates an OpenSSL server context using:

```c
SSL_CTX_new(TLS_server_method())
```

The server also loads the certificate and private key:

```c
SSL_CTX_use_certificate_file()
SSL_CTX_use_PrivateKey_file()
```

For each new client, the server creates a new SSL object and performs TLS handshake:

```c
SSL_new()
SSL_set_fd()
SSL_accept()
```

If the handshake succeeds, the user is added to the online user list.

The server also prints the TLS version and cipher suite when a user joins the chat room.

---

### 7.2 Client Side TLS

The client creates an OpenSSL client context using:

```c
SSL_CTX_new(TLS_client_method())
```

After the TCP connection is established, the client binds the socket to an SSL object and performs the TLS handshake:

```c
SSL_new()
SSL_set_fd()
SSL_connect()
```

If the handshake succeeds, the client prints the TLS version and cipher suite:

```text
Connected securely. TLS=TLSv1.3 Cipher=TLS_AES_256_GCM_SHA384
```

This confirms that the client is not using plain TCP anymore. It is communicating with the server through an SSL/TLS encrypted channel.

---

### 7.3 Encrypted Message Transmission

After the TLS connection is established, all communication is transmitted through OpenSSL functions:

```c
SSL_read()
SSL_write()
```

This applies to:

- Normal public chat messages
- Private messages
- Command messages
- Server responses
- User list responses
- Statistics responses

Therefore, even bonus commands such as `/dm`, `/whois`, and `/top` are still transmitted through the TLS channel.

---

## 8. Required Features from the Assignment

### 8.1 Server Can Accept SSL/TLS Connection

The server supports SSL/TLS connections by using OpenSSL. When a new client connects, the server performs:

```c
SSL_accept()
```

If the TLS handshake succeeds, the client is accepted into the chat room.

This satisfies the requirement:

```text
Your Server can accept SSL / TLS connection. +20
```

---

### 8.2 Client Can Issue SSL/TLS Connection to Server

The client connects to the server through TCP first, then starts TLS negotiation by using:

```c
SSL_connect()
```

After the connection succeeds, the client prints the TLS version and cipher suite.

This satisfies the requirement:

```text
Your Client can issue SSL / TLS connection to server. +10
```

---

### 8.3 Server Can Distribute Chats among Multiple SSL/TLS Connected Users

The server keeps all connected TLS users in an array of `User` structures. Each user has:

```c
int fd;
SSL *ssl;
char nickname[NAME_SIZE];
```

When a normal chat message is received, the server broadcasts it to all online users through:

```c
SSL_write()
```

Since every user has an individual SSL connection, the broadcast messages are distributed among multiple SSL/TLS connected users.

This satisfies the requirement:

```text
Your Server can distribute Chats among multiple SSL/TLS connected users. +5
```

---

## 9. Bonus Features

In addition to the required TLS features, this project includes several extra functions to make the chat room more complete and interactive.

All bonus commands are transmitted through the same SSL/TLS encrypted channel.

---

### 9.1 `/help`

```text
/help
```

Shows the command menu.

This helps users quickly understand which commands are supported by the chat system.

---

### 9.2 `/list`

```text
/list
```

Shows all currently online users.

Example output:

```text
[Online] Wayne Mark Gina
```

This feature is useful when users want to know who is currently in the chat room.

---

### 9.3 `/secure`

```text
/secure
```

Shows the current TLS version and cipher suite of the user's own connection.

Example output:

```text
[Secure Info] Your connection is encrypted. TLS=TLSv1.3 Cipher=TLS_AES_256_GCM_SHA384
```

This is useful because users can directly confirm that the connection is really protected by SSL/TLS.

---

### 9.4 `/stats`

```text
/stats
```

Shows server-side chat room statistics.

Example output:

```text
[Server Stats]
Online users: 3
Total joins: 3
Total public messages: 12
Server uptime: 95 seconds
Security: TLS encrypted chat room
```

This feature provides a simple system status dashboard for the chat server.

---

### 9.5 `/dm <name> <message>`

```text
/dm Mark Hello Mark, this is a private message.
```

Sends a private message to a specific user.

Only the sender and the target user can see the message. Other users will not receive it.

Example:

Wayne sends:

```text
/dm Mark Hello Mark, this is a private TLS message.
```

Mark receives:

```text
[Private TLS DM] Wayne -> you: Hello Mark, this is a private TLS message.
```

Wayne receives:

```text
[Private TLS DM] you -> Mark: Hello Mark, this is a private TLS message.
```

Gina does not receive this message.

This feature adds a private communication function while still using the TLS encrypted channel.

---

### 9.6 `/me <action>`

```text
/me is testing the final project
```

Broadcasts an action-style message.

Example output:

```text
* Wayne is testing the final project
```

This is inspired by action messages used in chat rooms and messaging platforms.

---

### 9.7 `/shout <message>`

```text
/shout tls secure chat success
```

Broadcasts an uppercase highlighted message.

Example output:

```text
[SHOUT] Wayne: TLS SECURE CHAT SUCCESS
```

This makes the chat room more interactive and easier to demonstrate.

---

### 9.8 `/rename <newname>`

```text
/rename SuperWayne
```

Changes the user's nickname.

Example output:

```text
[Server] Wayne is now known as SuperWayne.
```

The server also checks whether the new name is already used by another online user.

---

### 9.9 `/whois <name>`

```text
/whois Mark
```

Shows detailed secure session information of a specific online user.

Example output:

```text
========== User Information ==========
Name           : Mark
Connected Time : 2026-06-16 21:42:13
Online Time    : 5 min 21 sec
Messages Sent  : 3
TLS Version    : TLSv1.3
Cipher Suite   : TLS_AES_256_GCM_SHA384
======================================
```

This feature is one of the main creative bonus functions.

It allows users to inspect another user's session information, including:

- Nickname
- Connected time
- Online duration
- Number of public messages sent
- TLS version
- Cipher suite

This makes the system feel more like a secure chat room with session management, not just a simple message relay server.

---

### 9.10 `/top`

```text
/top
```

Shows the chat activity ranking of online users.

Example output:

```text
========== Chat Activity Ranking ==========

 1. Wayne                8 messages
 2. Mark                 3 messages
 3. Gina                 1 messages

Most Active User: Wayne
=========================================
```

This feature ranks users by the number of public messages they have sent.

It adds a simple analytics function to the chat system and makes the demo more interactive.

---

### 9.11 `/exit`

```text
/exit
```

Leaves the chat room and closes the secure connection.

When a user leaves, the server broadcasts a leaving message to other online users.

---

## 10. Chat History

The server stores public chat messages and system join/leave messages in:

```text
chat.log
```

When a new user joins the chat room, the server sends previous chat history to the new user.

This allows late-joining users to understand what happened before they entered the chat room.

The history is reset every time the server restarts, because the server removes the old log file at startup.

---

## 11. Multi-User Handling

The server supports up to:

```text
30 users
```

This value is defined by:

```c
#define MAX_CLIENTS 30
```

The server uses `select()` to monitor:

- The server listening socket
- All connected client sockets

This allows the server to handle multiple clients without creating one thread per client.

When a new socket becomes readable, the server checks whether it is:

- A new client connection
- A message from an existing client

This structure keeps the server simple and suitable for a socket programming assignment.

---

## 12. Cross-Platform Support

This project supports both Windows and Linux-based environments.

On Windows, the program uses:

```c
winsock2.h
ws2tcpip.h
WSAStartup()
closesocket()
```

On Linux/macOS, the program uses:

```c
sys/socket.h
arpa/inet.h
unistd.h
close()
```

The TLS part is handled by OpenSSL on both platforms, so the application logic remains mostly the same.

---

## 13. Important Note about Certificate Verification

For this homework and local demo, the client disables certificate verification:

```c
SSL_CTX_set_verify(ctx, SSL_VERIFY_NONE, NULL);
```

This means the traffic is still encrypted, but the client does not verify whether the server certificate is signed by a trusted Certificate Authority.

This setting is acceptable for local testing with a self-signed certificate.

For a real production system, the client should load trusted CA certificates and use:

```c
SSL_VERIFY_PEER
```

This would prevent man-in-the-middle attacks and provide stronger server identity verification.

---

## 14. Example Demo Flow

A suggested demo flow is shown below.

### Step 1: Start the server

```bash
./Final_C_server 8080 cert.pem key.pem
```

### Step 2: Start three clients

```bash
./Final_C_client 127.0.0.1 Wayne 8080
./Final_C_client 127.0.0.1 Mark 8080
./Final_C_client 127.0.0.1 Gina 8080
```

### Step 3: Confirm TLS connection

```text
/secure
```

### Step 4: Show online users

```text
/list
```

### Step 5: Send public messages

```text
Hello everyone!
```

### Step 6: Send private encrypted message

```text
/dm Mark This message is private and protected by TLS.
```

### Step 7: Show server statistics

```text
/stats
```

### Step 8: Show user session information

```text
/whois Mark
```

### Step 9: Show activity ranking

```text
/top
```

### Step 10: Rename user

```text
/rename SuperWayne
```

### Step 11: Leave chat room

```text
/exit
```

---

## 15. Self-Checklist for Instructor

| Category | Requirement / Feature | Status | Notes |
|---|---|---:|---|
| Required | Server can accept SSL/TLS connection | Done | Server uses `SSL_accept()` |
| Required | Client can issue SSL/TLS connection to server | Done | Client uses `SSL_connect()` |
| Required | Server can distribute chats among multiple SSL/TLS connected users | Done | Server broadcasts messages through `SSL_write()` |
| Bonus | `/help` command menu | Done | Shows all supported commands |
| Bonus | `/list` online user list | Done | Shows currently connected users |
| Bonus | `/secure` TLS information | Done | Shows TLS version and cipher suite |
| Bonus | `/stats` server statistics | Done | Shows uptime, joins, online users, and messages |
| Bonus | `/dm <name> <message>` private message | Done | Private message between sender and receiver |
| Bonus | `/me <action>` action message | Done | Broadcasts action-style message |
| Bonus | `/shout <message>` uppercase broadcast | Done | Broadcasts highlighted uppercase message |
| Bonus | `/rename <newname>` change nickname | Done | Includes duplicate name checking |
| Bonus | `/whois <name>` secure user session information | Done | Shows connected time, online time, message count, TLS version, and cipher suite |
| Bonus | `/top` activity ranking | Done | Shows ranking by public message count |
| Bonus | Chat history | Done | New users receive previous chat messages |
| Bonus | Cross-platform compatibility | Done | Supports Windows and Linux/macOS through conditional compilation |

---

## 16. Summary

This project implements a secure multi-user chat room based on TCP socket programming and OpenSSL.

The required part of the assignment is completed by supporting SSL/TLS on both the server and client sides. The server can accept TLS clients, the client can connect through TLS, and multiple SSL/TLS users can chat with each other at the same time.

Beyond the required features, I also added several bonus commands to make the chat room more practical and easier to demonstrate. In particular, `/whois` and `/top` extend the system from a basic secure chat room into a small secure chat platform with session information and user activity analytics.

Overall, this project helped me understand not only how socket communication works, but also how SSL/TLS can be integrated into a real network application to protect transmitted data.
