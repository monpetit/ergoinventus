#include "mbed.h"
#include "SDFileSystem.h"
#include "EthernetInterface.h"

#include <stdio.h>
#include <string.h>

#define PORT   80

EthernetInterface eth;

TCPSocketServer svr;
bool serverIsListened = false;

TCPSocketConnection client;
bool clientIsConnected = false;

DigitalOut led1(LED1);        // 서버 응답 상태
DigitalOut led2(LED2);        // 소켓 연결 상태
DigitalOut SIGNAL_LED(D13);    // 신호 출력용

Ticker led_tick;

Serial pc(USBTX, USBRX);
SDFileSystem sd(PTE3, PTE1, PTE2, PTE4, "sd"); // MOSI, MISO, SCK, CS

/* --------------------------------------------
 * 문자열이 해당 패턴으로 끝나는지 판단하는 함수
 *
 * 성공하면 1 반환, 실패하면 0 반환
 * -------------------------------------------*/
int str_ends_with(const char *str, const char *suffix)
{

    if (str == NULL || suffix == NULL)
        return 0;

    size_t str_len = strlen(str);
    size_t suffix_len = strlen(suffix);

    if (suffix_len > str_len)
        return 0;

    return (strncmp(str + str_len - suffix_len, suffix, suffix_len) == 0);
}

/* --------------------------------------------
 * LED 점멸 함수
 * -------------------------------------------*/
void led_tick_func(void)
{
    if (serverIsListened)  {
        led1 = !led1;
    }
    else {
        led1 = false;
    }
}

/* --------------------------------------------
 * TCP Socket 서버 설정 함수
 *
 * 성공하면 0 반환, 실패하면 그 외의 값 반환
 * -------------------------------------------*/
int init_server(void)
{
    // ethernet interface 설정
    eth.init();        // DHCP 사용
    eth.connect();    // 연결
    // 연결에 성공하면 IP 주소 출력
    printf("IP Address is %s\r\n", eth.getIPAddress());
    if (strlen(eth.getIPAddress()) == 0)
        return -1;
    // tcp socket 설정 (포트번호: 80)
    if (svr.bind(PORT) < 0) {
        printf("tcp server bind failed.\r\n");
        return -2;
    }
    else {
        printf("tcp server bind successed.\r\n");
        serverIsListened = true;
    }
    if (svr.listen(1) < 0) {
        printf("tcp server listen failed.\r\n");
        return -3;
    }
    else {
        printf("tcp server is listening...\r\n");
    }
    return 0;
}

/* --------------------------------------------
 * 입력 스트링에서 URL 부분만 잘라서 반환
 * -------------------------------------------*/
char* get_url(char* buffer)
{
    size_t len = strlen(buffer);
    for (int i = 0; i < len; i++) {
        if ((buffer[i] == ' ') || (buffer[i] == '\t')) {
            buffer[i] = '\0';
            break;
        }
    }
    return buffer;
}

/* --------------------------------------------
 * HTTP 응답 함수
 * -------------------------------------------*/
void make_response(char* buffer)
{
    FILE* fp;
    //setup http response header & data
    char echoHeader[256] = {};
    char filename[1024] = {};
    char resbuffer[1024];
    char* url = get_url(buffer);

    // URL이 /ON 이면 SIGNAL_LED ON
    if ((strcmp(url, "/ON") == 0) ||
            (strcmp(url, "/on") == 0)) {
        SIGNAL_LED = 1;
        url = (char*)"/led-on.html";
    }

    // URL이 /OFF 이면 SIGNAL_LED OFF
    if ((strcmp(url, "/OFF") == 0) ||
            (strcmp(url, "/off") == 0)) {
        SIGNAL_LED = 0;
        url = (char*)"/led-off.html";
    }

    // SD Card 상의 파일명으로 매치
    sprintf(filename, "/sd/www%s", url);
    printf("URL = [%s]\r\n", url);
    printf("FILE NAME = [%s]\r\n", filename);

    // URL에 파일명이 없으면 index.html로 매칭
    if (strcmp(filename, "/sd/www/") == 0)
        sprintf(filename, "/sd/www/index.html");

    printf("FINAL FILE NAME = [%s]\r\n", filename);

    // html page open
    fp = fopen(filename, "r");
    if (!fp) {
        // 파일을 찾을 수 없으면 404 에러 메시지 출력
        sprintf(echoHeader, "HTTP/1.1 404 Not Found\r\n"
                "Content-Length: 16\r\n"
                "Content-Type: text/html\r\n"
                "Connection: Close\r\n\r\n");
        client.send(echoHeader, strlen(echoHeader));
        client.send((char*)"FILE NOT FOUND\r\n", 16);
        clientIsConnected = false;
        printf("echo back done.\r\n");
        return;
    }

    // 파일 크기 출력
    fseek(fp, 0L, SEEK_END);
    size_t sz = ftell(fp);
    printf("file size = %d\r\n", sz);

    // 응답 헤더
    sprintf(echoHeader, "HTTP/1.1 200 OK\r\nContent-Length: %d\r\n", sz);
    client.send(echoHeader, strlen(echoHeader));

    // 파일 종류에 따른 응답 헤더 (Content-Type)
    if (str_ends_with(url, ".html") || str_ends_with(url, ".htm")) {
        sprintf(echoHeader, "Content-Type: text/html\r\n");
    }
    else if (str_ends_with(url, ".css")) {
        sprintf(echoHeader, "Content-Type: text/css\r\n");
    }
    else if (str_ends_with(url, ".png")) {
        sprintf(echoHeader, "Content-Type: image/png\r\n");
    }
    else if (str_ends_with(url, ".jpg")) {
        sprintf(echoHeader, "Content-Type: image/jpeg\r\n");
    }
    else if (str_ends_with(url, ".gif")) {
        sprintf(echoHeader, "Content-Type: image/gif\r\n");
    }
    else if (str_ends_with(url, ".3gp")) {
        sprintf(echoHeader, "Content-Type: video/mpeg\r\n");
    }
    else if (str_ends_with(url, ".pdf")) {
        sprintf(echoHeader, "Content-Type: application/pdf\r\n");
    }
    else if (str_ends_with(url, ".js")) {
        sprintf(echoHeader, "Content-Type: application/x-javascript\r\n");
    }
    else if (str_ends_with(url, ".xml")) {
        sprintf(echoHeader, "Content-Type: application/xml\r\n");
    }
    else {
        sprintf(echoHeader, "Content-Type: text\r\n");
    }
    client.send(echoHeader, strlen(echoHeader));

    sprintf(echoHeader, "Connection: Close\r\n\r\n");
    client.send(echoHeader, strlen(echoHeader));

    // 파일 내용을 읽어서 클라이언트로 전송
    fseek(fp, 0L, SEEK_SET);
    size_t n = 1;
    while (n) {
        n = fread(resbuffer, sizeof(char), 1023, fp);
        resbuffer[n] = '\0';
        client.send(resbuffer, n);
    }
    fclose(fp);
    clientIsConnected = false;
    printf("echo back done.\r\n");
}

int main(void)
{
    pc.baud(115200);
    led_tick.attach(&led_tick_func, 0.5);

    if (init_server() < 0) {
        printf("HTTP Server initization failed...\r\n");
        for (;;) ;
    }

    //listening for http GET request
    while (serverIsListened) {
        //blocking mode(never timeout)
        if (svr.accept(client) < 0) {
            printf("failed to accept connection.\r\n");
        }
        else {
            printf("connection success!\r\nIP: %s\r\n",client.get_address());
            clientIsConnected = true;
            led2 = true;

            while (clientIsConnected) {
                char buffer[1024] = {};
                switch (client.receive(buffer, 1023)) {
                case 0:
                    printf("recieved buffer is empty.\r\n");
                    clientIsConnected = false;
                    break;
                case -1:
                    printf("failed to read data from client.\r\n");
                    clientIsConnected = false;
                    break;
                default:
                    printf("Recieved Data: %d\r\n\r\n%.*s\r\n",
                           strlen(buffer),strlen(buffer),buffer);
                    if (buffer[0] == 'G' && buffer[1] == 'E' && buffer[2] == 'T') {
                        printf("GET request incomming.\r\n");
                        make_response(buffer+4);
                    }
                    break;
                }
            }
            printf("close connection.\r\ntcp server is listening...\r\n");
            client.close();
            led2 = false;
        }
    }
}
