#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <signal.h>
#include <errno.h>
#include <ctype.h>

/* 跨平台相容性處理：區分 Windows (WinSock2) 與 Linux 套接字標頭檔 */
#ifdef _WIN32
    #include <winsock2.h>
    #include <ws2tcpip.h>
    #include <windows.h>
    #pragma comment(lib, "ws2_32.lib")
    #define close_socket closesocket
    typedef int socklen_t;
#else
    #include <sys/socket.h>
    #include <arpa/inet.h>
    #include <unistd.h>
    #include <fcntl.h>
    #define close_socket close
#endif

#include <openssl/ssl.h>
#include <openssl/err.h>

/* 系統預設參數與組態設定 */
#define PORT_DEFAULT 8080
#define MAX_CLIENTS 30
#define BUF_SIZE 2048
#define NAME_SIZE 50
#define HISTORY_LOG "chat.log"

/* 終端機文字顏色控制碼 (ANSI Escape Codes) */
#define CLR_RED     "\x1b[31m"
#define CLR_GRN     "\x1b[32m"
#define CLR_YLW     "\x1b[33m"
#define CLR_CYAN    "\x1b[36m"
#define CLR_MAG     "\x1b[35m"
#define CLR_RESET   "\x1b[0m"

/* 用戶連線資訊結構體 */
typedef struct {
    int fd;
    SSL *ssl;
    char nickname[NAME_SIZE];
    time_t connected_at;
    int message_count;   /* 統計該用戶發送的公頻訊息總數，供排序功能使用 */
} User;

/* 全域變數：伺服器上下文與用戶狀態陣列 */
static SSL_CTX *g_ctx = NULL;
static int g_server_fd = -1;
static User g_users[MAX_CLIENTS];

/* 伺服器統計數據計數器 */
static time_t g_start_time;
static int g_total_messages = 0;
static int g_total_joins = 0;

/* 清除並釋放所有配置的 Socket 與 SSL 資源 */
static void cleanup_all(void) {
    for (int i = 0; MAX_CLIENTS > i; ++i) {
        if (g_users[i].fd > 0) {
            if (g_users[i].ssl) {
                SSL_shutdown(g_users[i].ssl);
                SSL_free(g_users[i].ssl);
            }
            close_socket(g_users[i].fd);
            g_users[i].fd = 0;
            g_users[i].ssl = NULL;
        }
    }
    if (g_server_fd >= 0) {
        close_socket(g_server_fd);
        g_server_fd = -1;
    }
    if (g_ctx) {
        SSL_CTX_free(g_ctx);
        g_ctx = NULL;
    }
#ifdef _WIN32
    WSACleanup();
#endif
}

/* 訊號處理常式：攔截中斷訊號以執行安全關機 */
static void on_signal(int sig) {
    (void)sig;
    printf("\nServer shutting down...\n");
    cleanup_all();
    exit(0);
}

/* OpenSSL 錯誤輸出與異常終止處理 */
static void openssl_die(const char *msg) {
    fprintf(stderr, CLR_RED "%s\n" CLR_RESET, msg);
    ERR_print_errors_fp(stderr);
    cleanup_all();
    exit(1);
}

/* 將聊天紀錄附加寫入至本地日誌檔案 */
static void write_log(const char *text) {
    FILE *f = fopen(HISTORY_LOG, "a");
    if (f) {
        fprintf(f, "%s\n", text);
        fclose(f);
    }
}

/* 循環呼叫 SSL_write 確保緩衝區內的資料完全送出 */
static int ssl_send_all(SSL *ssl, const char *msg) {
    int total = 0;
    int len = (int)strlen(msg);
    while (len > total) {
        int n = SSL_write(ssl, msg + total, len - total);
        if (0 >= n) return -1;
        total += n;
    }
    return 0;
}

/* 發送訊息給指定索引值的單一用戶 */
static void send_to_user(int idx, const char *msg) {
    if (idx >= 0 && MAX_CLIENTS > idx && g_users[idx].fd > 0 && g_users[idx].ssl) {
        ssl_send_all(g_users[idx].ssl, msg);
    }
}

/* 廣播訊息給所有目前在線的用戶 */
static void broadcast(const char *msg) {
    for (int i = 0; MAX_CLIENTS > i; ++i) {
        if (g_users[i].fd > 0 && g_users[i].ssl) {
            ssl_send_all(g_users[i].ssl, msg);
        }
    }
}

/* 計算當前在線用戶的總數 */
static int online_count(void) {
    int count = 0;
    for (int i = 0; MAX_CLIENTS > i; ++i) {
        if (g_users[i].fd > 0) count++;
    }
    return count;
}

/* 依據用戶暱稱檢索對應的陣列索引值 */
static int find_user_by_name(const char *name) {
    for (int i = 0; MAX_CLIENTS > i; ++i) {
        if (g_users[i].fd > 0 && strcmp(g_users[i].nickname, name) == 0) {
            return i;
        }
    }
    return -1;
}

/* 將傳入的秒數轉換格式化為時、分、秒字串 */
static void format_duration(long seconds, char *out, size_t out_size) {
    long hours = seconds / 3600;
    long minutes = (seconds % 3600) / 60;
    long secs = seconds % 60;

    if (hours > 0) {
        snprintf(out, out_size, "%ld hr %ld min %ld sec", hours, minutes, secs);
    } else if (minutes > 0) {
        snprintf(out, out_size, "%ld min %ld sec", minutes, secs);
    } else {
        snprintf(out, out_size, "%ld sec", secs);
    }
}

/* 讀取歷史日誌並發送給新加入的用戶，並進行流量延遲控制 */
static void send_history(SSL *ssl) {
    FILE *f = fopen(HISTORY_LOG, "r");
    if (!f) return;

    char line[BUF_SIZE + 256];
    ssl_send_all(ssl, CLR_YLW "--- Past Messages ---\n" CLR_RESET);
    while (fgets(line, sizeof(line), f)) {
        ssl_send_all(ssl, line);
#ifdef _WIN32
        Sleep(10);
#else
        usleep(10000);
#endif
    }
    ssl_send_all(ssl, CLR_YLW "---------------------\n" CLR_RESET);
    fclose(f);
}

/* 處理用戶斷線或離線時的資源釋放與公告 */
static void remove_user(int idx) {
    char bye[256];
    snprintf(bye, sizeof(bye), CLR_RED "[Server] %s left.\n" CLR_RESET, g_users[idx].nickname);
    printf("%s", bye);

    if (g_users[idx].ssl) {
        SSL_shutdown(g_users[idx].ssl);
        SSL_free(g_users[idx].ssl);
        g_users[idx].ssl = NULL;
    }
    close_socket(g_users[idx].fd);
    g_users[idx].fd = 0;
    g_users[idx].nickname[0] = '\0';

    broadcast(bye);
    write_log(bye);
}

/* 初始化 OpenSSL 函式庫並載入憑證與私鑰組態 */
static SSL_CTX *create_server_ctx(const char *cert_file, const char *key_file) {
    SSL_library_init();
    SSL_load_error_strings();
    OpenSSL_add_ssl_algorithms();

    const SSL_METHOD *method = TLS_server_method();
    SSL_CTX *ctx = SSL_CTX_new(method);
    if (!ctx) openssl_die("SSL_CTX_new failed.");

    /* 強制要求安全協定最低版本為 TLS 1.2 */
    SSL_CTX_set_min_proto_version(ctx, TLS1_2_VERSION);

    if (0 >= SSL_CTX_use_certificate_file(ctx, cert_file, SSL_FILETYPE_PEM)) {
        openssl_die("Cannot load certificate file. Check cert.pem path.");
    }
    if (0 >= SSL_CTX_use_PrivateKey_file(ctx, key_file, SSL_FILETYPE_PEM)) {
        openssl_die("Cannot load private key file. Check key.pem path.");
    }
    if (!SSL_CTX_check_private_key(ctx)) {
        openssl_die("Private key does not match certificate.");
    }
    return ctx;
}

/* 建立並監聽指定埠口的 TCP Socket */
static int create_listen_socket(int port) {
    int fd = socket(AF_INET, SOCK_STREAM, 0);
    if (0 > fd) {
        perror("socket");
        exit(1);
    }

    int on = 1;
    setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, (char *)&on, sizeof(on));

    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port = htons((unsigned short)port);

    if (0 > bind(fd, (struct sockaddr *)&addr, sizeof(addr))) {
        perror("bind");
        close_socket(fd);
        exit(1);
    }
    if (0 > listen(fd, 10)) {
        perror("listen");
        close_socket(fd);
        exit(1);
    }
    return fd;
}

/* 接收連線並執行 TLS 安全交握程序，同時讀取用戶暱稱 */
static void handle_new_client(void) {
    struct sockaddr_in cli_addr;
    socklen_t len = sizeof(cli_addr);
    int new_fd = accept(g_server_fd, (struct sockaddr *)&cli_addr, &len);
    if (0 > new_fd) return;

    SSL *ssl = SSL_new(g_ctx);
    if (!ssl) {
        close_socket(new_fd);
        return;
    }
    SSL_set_fd(ssl, new_fd);

    /* 執行 TLS Handshake */
    if (0 >= SSL_accept(ssl)) {
        fprintf(stderr, CLR_RED "TLS handshake failed.\n" CLR_RESET);
        ERR_print_errors_fp(stderr);
        SSL_free(ssl);
        close_socket(new_fd);
        return;
    }

    /* 讀取連線初始階段傳送的用戶暱稱 */
    char name[NAME_SIZE];
    int n = SSL_read(ssl, name, sizeof(name) - 1);
    if (0 >= n) {
        SSL_shutdown(ssl);
        SSL_free(ssl);
        close_socket(new_fd);
        return;
    }
    name[n] = '\0';
    name[strcspn(name, "\r\n")] = '\0';
    if (0 == strlen(name)) strncpy(name, "Anonymous", sizeof(name) - 1);

    /* 尋找未使用的結構體槽位 */
    int slot = -1;
    for (int i = 0; MAX_CLIENTS > i; ++i) {
        if (0 == g_users[i].fd) { slot = i; break; }
    }
    if (0 > slot) {
        ssl_send_all(ssl, "[Server] Chat room is full.\n");
        SSL_shutdown(ssl);
        SSL_free(ssl);
        close_socket(new_fd);
        return;
    }

    /* 寫入用戶端狀態欄位 */
    g_users[slot].fd = new_fd;
    g_users[slot].ssl = ssl;
    g_users[slot].connected_at = time(NULL);
    g_users[slot].message_count = 0;
    strncpy(g_users[slot].nickname, name, NAME_SIZE - 1);
    g_users[slot].nickname[NAME_SIZE - 1] = '\0';
    g_total_joins++;

    send_history(ssl);

    char welcome[256];
    snprintf(welcome, sizeof(welcome), CLR_YLW "[Server] %s is here! TLS=%s Cipher=%s\n" CLR_RESET,
             g_users[slot].nickname, SSL_get_version(ssl), SSL_get_cipher(ssl));
    printf("%s", welcome);
    broadcast(welcome);
    write_log(welcome);
}

/* 印出說明選單與支援之指令清單 */
static void show_help(int idx) {
    char help[BUF_SIZE];
    snprintf(help, sizeof(help),
        CLR_MAG
        "\n========== Bonus Commands ==========\n"
        "/help                Show this command menu\n"
        "/list                Show online users\n"
        "/secure              Show your TLS version and cipher\n"
        "/stats               Show server uptime and chat statistics\n"
        "/dm <name> <message> Send encrypted private message\n"
        "/me <action>         Send action message\n"
        "/shout <message>     Broadcast uppercase message\n"
        "/rename <newname>    Change your nickname\n"
        "/exit                Leave the chat room\n"
        "====================================\n\n"
        CLR_RESET);
    send_to_user(idx, help);
}

/* 向用戶回傳目前的 TLS 加密版本與密碼套件名稱 */
static void show_secure_info(int idx) {
    char msg[BUF_SIZE];
    snprintf(msg, sizeof(msg),
        CLR_GRN "[Secure Info] Your connection is encrypted. TLS=%s Cipher=%s\n" CLR_RESET,
        SSL_get_version(g_users[idx].ssl),
        SSL_get_cipher(g_users[idx].ssl));
    send_to_user(idx, msg);
}

/* 顯示伺服器目前的運行統計數據 */
static void show_stats(int idx) {
    time_t now = time(NULL);
    long uptime = (long)(now - g_start_time);
    char msg[BUF_SIZE];
    snprintf(msg, sizeof(msg),
        CLR_YLW
        "[Server Stats]\n"
        "Online users: %d\n"
        "Total joins: %d\n"
        "Total public messages: %d\n"
        "Server uptime: %ld seconds\n"
        "Security: TLS encrypted chat room\n"
        CLR_RESET,
        online_count(), g_total_joins, g_total_messages, uptime);
    send_to_user(idx, msg);
}

/* 檢索並向請求方呈現特定用戶的詳細登入數據 */
static void handle_whois(int idx, char *buf) {
    char target[NAME_SIZE];

    if (1 != sscanf(buf, "/whois %49s", target)) {
        send_to_user(idx, CLR_RED "[Usage] /whois <name>\n" CLR_RESET);
        return;
    }

    int target_idx = find_user_by_name(target);
    if (0 > target_idx) {
        send_to_user(idx, CLR_RED "[WHOIS failed] User not found. Use /list to check online users.\n" CLR_RESET);
        return;
    }

    time_t now = time(NULL);
    long online_seconds = (long)(now - g_users[target_idx].connected_at);

    char connected_time[64];
    struct tm *tm_info = localtime(&g_users[target_idx].connected_at);
    if (tm_info) {
        strftime(connected_time, sizeof(connected_time), "%Y-%m-%d %H:%M:%S", tm_info);
    } else {
        snprintf(connected_time, sizeof(connected_time), "unknown");
    }

    char online_time[64];
    format_duration(online_seconds, online_time, sizeof(online_time));

    char msg[BUF_SIZE];
    snprintf(msg, sizeof(msg),
        CLR_MAG
        "\n========== User Information =========="
        "\nName           : %s"
        "\nConnected Time : %s"
        "\nOnline Time    : %s"
        "\nMessages Sent  : %d"
        "\nTLS Version    : %s"
        "\nCipher Suite   : %s"
        "\n======================================\n"
        CLR_RESET,
        g_users[target_idx].nickname,
        connected_time,
        online_time,
        g_users[target_idx].message_count,
        SSL_get_version(g_users[target_idx].ssl),
        SSL_get_cipher(g_users[target_idx].ssl));

    send_to_user(idx, msg);
}

/* 計算發話量排行榜 (使用基礎氣泡排序法對活躍度進行遞減排序) */
static void show_top(int idx) {
    int order[MAX_CLIENTS];
    int count = 0;

    for (int i = 0; MAX_CLIENTS > i; ++i) {
        if (g_users[i].fd > 0) order[count++] = i;
    }

    for (int i = 0; count - 1 > i; ++i) {
        for (int j = i + 1; count > j; ++j) {
            if (g_users[order[j]].message_count > g_users[order[i]].message_count) {
                int tmp = order[i];
                order[i] = order[j];
                order[j] = tmp;
            }
        }
    }

    char msg[BUF_SIZE];
    int used = snprintf(msg, sizeof(msg), CLR_YLW "\n========== Chat Activity Ranking ==========\n\n");

    for (int i = 0; count > i && 10 > i; ++i) {
        used += snprintf(msg + used, sizeof(msg) - used, "%2d. %-20s %d messages\n", i + 1, g_users[order[i]].nickname, g_users[order[i]].message_count);
        if (used >= (int)sizeof(msg) - 128) break;
    }

    if (0 == count) {
        used += snprintf(msg + used, sizeof(msg) - used, "No online users.\n");
    } else {
        used += snprintf(msg + used, sizeof(msg) - used, "Most Active User: %s\n", g_users[order[0]].nickname);
    }

    snprintf(msg + used, sizeof(msg) - used, "=========================================\n" CLR_RESET);
    send_to_user(idx, msg);
}

/* 處理私人密語傳送 (僅傳遞至目標用戶與發送者用戶本身) */
static void handle_dm(int idx, char *buf) {
    char target[NAME_SIZE];
    char message[BUF_SIZE];

    if (2 != sscanf(buf, "/dm %49s %[^\n]", target, message)) {
        send_to_user(idx, CLR_RED "[Usage] /dm <name> <message>\n" CLR_RESET);
        return;
    }

    int target_idx = find_user_by_name(target);
    if (0 > target_idx) {
        send_to_user(idx, CLR_RED "[DM failed] User not found.\n" CLR_RESET);
        return;
    }

    char out[BUF_SIZE + 256];
    snprintf(out, sizeof(out), CLR_MAG "[Private TLS DM] %s -> you: %s\n" CLR_RESET, g_users[idx].nickname, message);
    send_to_user(target_idx, out);

    snprintf(out, sizeof(out), CLR_MAG "[Private TLS DM] you -> %s: %s\n" CLR_RESET, g_users[target_idx].nickname, message);
    send_to_user(idx, out);
}

/* 發送第三方敘事動作訊息 */
static void handle_me(int idx, char *buf) {
    char *action = buf + 4;
    if (0 == strlen(action)) {
        send_to_user(idx, CLR_RED "[Usage] /me <action>\n" CLR_RESET);
        return;
    }

    char out[BUF_SIZE + 256];
    snprintf(out, sizeof(out), CLR_MAG "* %s %s\n" CLR_RESET, g_users[idx].nickname, action);
    broadcast(out);
    write_log(out);
}

/* 將內文轉換為全大寫字母後進行高亮廣播 */
static void handle_shout(int idx, char *buf) {
    char *message = buf + 7;
    if (0 == strlen(message)) {
        send_to_user(idx, CLR_RED "[Usage] /shout <message>\n" CLR_RESET);
        return;
    }

    char upper[BUF_SIZE];
    strncpy(upper, message, sizeof(upper) - 1);
    upper[sizeof(upper) - 1] = '\0';

    for (int i = 0; upper[i]; ++i) {
        upper[i] = (char)toupper((unsigned char)upper[i]);
    }

    char out[BUF_SIZE + 256];
    snprintf(out, sizeof(out), CLR_YLW "[SHOUT] %s: %s\n" CLR_RESET, g_users[idx].nickname, upper);
    broadcast(out);
    write_log(out);
}

/* 更改當前用戶暱稱 (內含名稱重複性檢驗) */
static void handle_rename(int idx, char *buf) {
    char newname[NAME_SIZE];
    if (1 != sscanf(buf, "/rename %49s", newname)) {
        send_to_user(idx, CLR_RED "[Usage] /rename <newname>\n" CLR_RESET);
        return;
    }

    if (find_user_by_name(newname) >= 0) {
        send_to_user(idx, CLR_RED "[Rename failed] This name is already used.\n" CLR_RESET);
        return;
    }

    char oldname[NAME_SIZE];
    strncpy(oldname, g_users[idx].nickname, NAME_SIZE - 1);
    oldname[NAME_SIZE - 1] = '\0';

    strncpy(g_users[idx].nickname, newname, NAME_SIZE - 1);
    g_users[idx].nickname[NAME_SIZE - 1] = '\0';

    char out[256];
    snprintf(out, sizeof(out), CLR_YLW "[Server] %s is now known as %s.\n" CLR_RESET, oldname, g_users[idx].nickname);
    broadcast(out);
    write_log(out);
}

/* 解析用戶端資料，依據內容分流至斜線指令或一般對話通道 */
static void handle_client_msg(int idx) {
    char buf[BUF_SIZE];
    int n = SSL_read(g_users[idx].ssl, buf, sizeof(buf) - 1);
    if (0 >= n) {
        remove_user(idx);
        return;
    }
    buf[n] = '\0';
    buf[strcspn(buf, "\r\n")] = '\0';

    if (0 == strcmp(buf, "/exit")) { remove_user(idx); return; }
    if (0 == strcmp(buf, "/help")) { show_help(idx); return; }
    if (0 == strcmp(buf, "/secure")) { show_secure_info(idx); return; }
    if (0 == strcmp(buf, "/stats")) { show_stats(idx); return; }
    if (0 == strncmp(buf, "/whois", 6)) { handle_whois(idx, buf); return; }
    if (0 == strcmp(buf, "/top")) { show_top(idx); return; }

    if (0 == strcmp(buf, "/list")) {
        char list[BUF_SIZE] = CLR_CYAN "[Online] ";
        for (int j = 0; MAX_CLIENTS > j; ++j) {
            if (g_users[j].fd > 0) {
                strncat(list, g_users[j].nickname, sizeof(list) - strlen(list) - 2);
                strncat(list, " ", sizeof(list) - strlen(list) - 1);
            }
        }
        strncat(list, "\n" CLR_RESET, sizeof(list) - strlen(list) - 1);
        ssl_send_all(g_users[idx].ssl, list);
        return;
    }

    if (0 == strncmp(buf, "/dm ", 4)) { handle_dm(idx, buf); return; }
    if (0 == strncmp(buf, "/me ", 4)) { handle_me(idx, buf); return; }
    if (0 == strncmp(buf, "/shout ", 7)) { handle_shout(idx, buf); return; }
    if (0 == strncmp(buf, "/rename ", 8)) { handle_rename(idx, buf); return; }

    /* 處理標準對話訊息，附加時間戳記並進行廣播 */
    time_t t = time(NULL);
    char *ts = ctime(&t);
    if (ts) ts[strlen(ts) - 1] = '\0';

    char out[BUF_SIZE + 256];
    snprintf(out, sizeof(out), "%s : " CLR_CYAN "%s" CLR_RESET " said: %s\n", ts ? ts : "time", g_users[idx].nickname, buf);
    broadcast(out);
    write_log(out);
    g_users[idx].message_count++;
    g_total_messages++;
}

/* 主程式：解析命令列參數、初始化環境並啟動事件迴圈 */
int main(int argc, char *argv[]) {
    int port = (argc >= 2) ? atoi(argv[1]) : PORT_DEFAULT;
    const char *cert_file = (argc >= 3) ? argv[2] : "cert.pem";
    const char *key_file  = (argc >= 4) ? argv[3] : "key.pem";

    if (0 >= port) port = PORT_DEFAULT;
    signal(SIGINT, on_signal);
#ifndef _WIN32
    signal(SIGPIPE, SIG_IGN); /* 忽略 SIGPIPE 訊號，防止因客戶端異常斷線造成核心終止 */
#endif

#ifdef _WIN32
    WSADATA wsa;
    if (0 != WSAStartup(MAKEWORD(2, 2), &wsa)) {
        fprintf(stderr, "WSAStartup failed.\n");
        return 1;
    }
#endif

    g_start_time = time(NULL);
    remove(HISTORY_LOG);
    memset(g_users, 0, sizeof(g_users));

    g_ctx = create_server_ctx(cert_file, key_file);
    g_server_fd = create_listen_socket(port);

    printf(CLR_GRN "TLS Chat Server started on port %d.\n" CLR_RESET, port);
    printf("Certificate: %s | Key: %s\n", cert_file, key_file);
    printf("Waiting for SSL/TLS clients...\n");
    printf(CLR_MAG "Bonus enabled: /help, /secure, /stats, /whois, /top, /dm, /me, /shout, /rename\n" CLR_RESET);

    /* 多路複用事件非阻塞式監聽迴圈 (I/O Multiplexing via select) */
    while (1) {
        fd_set rfds;
        FD_ZERO(&rfds);
        FD_SET(g_server_fd, &rfds);
        int max_fd = g_server_fd;

        for (int i = 0; MAX_CLIENTS > i; ++i) {
            if (g_users[i].fd > 0) {
                FD_SET(g_users[i].fd, &rfds);
                if (g_users[i].fd > max_fd) max_fd = g_users[i].fd;
            }
        }

        if (0 > select(max_fd + 1, &rfds, NULL, NULL, NULL)) continue;

        if (FD_ISSET(g_server_fd, &rfds)) handle_new_client();

        for (int i = 0; MAX_CLIENTS > i; ++i) {
            if (g_users[i].fd > 0 && FD_ISSET(g_users[i].fd, &rfds)) {
                handle_client_msg(i);
            }
        }
    }
}