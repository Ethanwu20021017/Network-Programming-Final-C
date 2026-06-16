#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>

/* 跨平台相容性處理：依作業系統載入對應的網路通訊標頭檔 */
#ifdef _WIN32
    #include <winsock2.h>
    #include <ws2tcpip.h>
    #include <conio.h>
    #pragma comment(lib, "ws2_32.lib")
    #define close_socket closesocket
#else
    #include <sys/socket.h>
    #include <arpa/inet.h>
    #include <unistd.h>
    #include <sys/select.h>
#endif

#include <openssl/ssl.h>
#include <openssl/err.h>

/* 系統預設參數設定 */
#define PORT_DEFAULT 8080
#define BUF_SIZE 2048

/* 終端機文字顏色控制碼 */
#define RED_TEXT   "\x1b[31m"
#define GREEN_TEXT "\x1b[32m"
#define YELLOW_TEXT "\x1b[33m"
#define RESET_TEXT "\x1b[0m"

/* 非阻塞式輸入檢測：檢查鍵盤輸入緩衝區內是否有資料準備讀取 */
static int is_kb_ready(void) {
#ifdef _WIN32
    return _kbhit();
#else
    fd_set fds;
    struct timeval tv = {0, 0};
    FD_ZERO(&fds);
    FD_SET(0, &fds);
    return select(1, &fds, NULL, NULL, &tv) > 0;
#endif
}

/* OpenSSL 內部錯誤之高亮標準錯誤輸出 */
static void openssl_print_error(const char *msg) {
    fprintf(stderr, RED_TEXT "%s\n" RESET_TEXT, msg);
    ERR_print_errors_fp(stderr);
}

/* 格式化訊息並呼叫 SSL_write 循環發送，確保換行符號與資料完整送出 */
static int ssl_send_line(SSL *ssl, const char *text) {
    char line[BUF_SIZE + 4];
    snprintf(line, sizeof(line), "%s\n", text);
    int total = 0;
    int len = (int)strlen(line);
    while (len > total) {
        int n = SSL_write(ssl, line + total, len - total);
        if (0 >= n) return -1;
        total += n;
    }
    return 0;
}

/* 初始化 OpenSSL 函式庫並建立客戶端安全上下文 */
static SSL_CTX *create_client_ctx(void) {
    SSL_library_init();
    SSL_load_error_strings();
    OpenSSL_add_ssl_algorithms();

    SSL_CTX *ctx = SSL_CTX_new(TLS_client_method());
    if (!ctx) return NULL;
    SSL_CTX_set_min_proto_version(ctx, TLS1_2_VERSION);

    /*
     * 本地測試與作業示範用途：關閉憑證驗證 (SSL_VERIFY_NONE)
     * 注意：通訊內容仍維持 TLS 加密狀態。若為正式環境需載入 CA 憑證並改用 SSL_VERIFY_PEER。
     */
    SSL_CTX_set_verify(ctx, SSL_VERIFY_NONE, NULL);
    return ctx;
}

/* 主程式：解析參數、建立通訊端、執行 TLS 交握並開啟 I/O 輪詢 */
int main(int argc, char *argv[]) {
    if (3 > argc) {
        printf("usage: %s chat_server your_name [port]\n", argv[0]);
        printf("example: %s 127.0.0.1 Wayne 8080\n", argv[0]);
        return 1;
    }

    const char *srv_ip = argv[1];
    const char *nickname = argv[2];
    int port = (argc >= 4) ? atoi(argv[3]) : PORT_DEFAULT;
    if (0 >= port) port = PORT_DEFAULT;

#ifdef _WIN32
    WSADATA ws_data;
    if (0 != WSAStartup(MAKEWORD(2, 2), &ws_data)) {
        printf("WSAStartup failed.\n");
        return 1;
    }
#endif

    SSL_CTX *ctx = create_client_ctx();
    if (!ctx) {
        openssl_print_error("SSL_CTX_new failed.");
        return 1;
    }

    int client_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (0 > client_fd) {
        perror("socket");
        SSL_CTX_free(ctx);
        return 1;
    }

    struct sockaddr_in srv_info;
    memset(&srv_info, 0, sizeof(srv_info));
    srv_info.sin_family = AF_INET;
    srv_info.sin_port = htons((unsigned short)port);

    if (0 >= inet_pton(AF_INET, srv_ip, &srv_info.sin_addr)) {
        printf(RED_TEXT "IP address error.\n" RESET_TEXT);
        close_socket(client_fd);
        SSL_CTX_free(ctx);
        return 1;
    }

    printf("Server Address is: %s %d\n", srv_ip, port);
    printf("Connecting to the Chat Server...\n");
    if (0 > connect(client_fd, (struct sockaddr *)&srv_info, sizeof(srv_info))) {
        perror("connect");
        close_socket(client_fd);
        SSL_CTX_free(ctx);
        return 1;
    }

    /* 將 Socket 檔案描述符綁定至 OpenSSL 結構體 */
    SSL *ssl = SSL_new(ctx);
    SSL_set_fd(ssl, client_fd);

    printf("Starting SSL/TLS handshake...\n");
    if (0 >= SSL_connect(ssl)) {
        openssl_print_error("SSL_connect failed.");
        SSL_free(ssl);
        close_socket(client_fd);
        SSL_CTX_free(ctx);
        return 1;
    }

    /* 交握成功，印出加密資訊與支援之選單指令 */
    printf(GREEN_TEXT "Connected securely. TLS=%s Cipher=%s\n" RESET_TEXT, SSL_get_version(ssl), SSL_get_cipher(ssl));
    printf("Hello %s, to send message, enter text followed by enter.\n", nickname);
    printf(GREEN_TEXT "This chat is protected by SSL/TLS.\n" RESET_TEXT);
    printf(YELLOW_TEXT "Available Commands:\n" RESET_TEXT);

    printf("  /help                   Show all commands\n");
    printf("  /list                   Show online users\n");
    printf("  /secure                 Show TLS version and cipher\n");
    printf("  /stats                  Show server statistics\n");
    printf("  /dm <name> <message>    Send encrypted private message\n");
    printf("  /me <action>            Action message\n");
    printf("  /shout <message>        Broadcast uppercase message\n");
    printf("  /rename <newname>       Change nickname\n");

    printf(GREEN_TEXT "  /whois <name>" RESET_TEXT);
    printf("          Show secure user information\n");

    printf(GREEN_TEXT "  /top" RESET_TEXT);
    printf("                    Show active user ranking\n");

    printf("  /exit                   Leave chat\n\n");
    printf("> ");
    fflush(stdout);

    /* 連線成功後，依伺服器規定優先發送用戶暱稱 */
    if (0 > ssl_send_line(ssl, nickname)) {
        printf(RED_TEXT "Failed to send nickname.\n" RESET_TEXT);
        SSL_free(ssl);
        close_socket(client_fd);
        SSL_CTX_free(ctx);
        return 1;
    }

    char msg_buf[BUF_SIZE];
    /* 核心事件處理迴圈：同時輪詢鍵盤標準輸入與遠端網路 Socket 接收 */
    while (1) {
        if (is_kb_ready()) {
            if (fgets(msg_buf, sizeof(msg_buf), stdin)) {
                msg_buf[strcspn(msg_buf, "\r\n")] = '\0';
                if (strlen(msg_buf) > 0) {
                    if (0 > ssl_send_line(ssl, msg_buf)) {
                        printf(RED_TEXT "\nSend failed.\n" RESET_TEXT);
                        break;
                    }
                    if (0 == strcmp(msg_buf, "/exit")) {
                        printf("Bye!\n");
                        break;
                    }
                }
                printf("> ");
                fflush(stdout);
            }
        }

        fd_set rfds;
        struct timeval to = {0, 10000}; /* 設定 select 阻塞超時時間為 10 毫秒 */
        FD_ZERO(&rfds);
        FD_SET(client_fd, &rfds);

        if (select(client_fd + 1, &rfds, NULL, NULL, &to) > 0) {
            int n = SSL_read(ssl, msg_buf, sizeof(msg_buf) - 1);
            if (n > 0) {
                msg_buf[n] = '\0';
                printf("\r%s> ", msg_buf);
                fflush(stdout);
            } else {
                printf(RED_TEXT "\nServer closed the secure connection.\n" RESET_TEXT);
                break;
            }
        }
    }

    /* 結束執行，執行 TLS 關閉手續並釋放系統通訊資源 */
    SSL_shutdown(ssl);
    SSL_free(ssl);
    close_socket(client_fd);
    SSL_CTX_free(ctx);
#ifdef _WIN32
    WSACleanup();
#endif
    return 0;
}