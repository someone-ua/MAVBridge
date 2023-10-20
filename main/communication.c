#include <stdio.h>
#include <sys/cdefs.h>
#include <sys/fcntl.h>
#include <sys/param.h>
#include <string.h>
#include "communication.h"
#include <esp_vfs_dev.h>
#include "driver/uart.h"
#include "esp_log.h"
#include <c_library_v2/ardupilotmega/mavlink.h>
#include <esp_timer.h>
#include "lwip/err.h"
#include "lwip/sockets.h"
#include "lwip/sys.h"
#include <lwip/netdb.h>
#include "esp_netif.h"
#include "esp_system.h"


#define TAG "COMM"

int addr_family = AF_INET;
int ip_protocol = IPPROTO_IP;

int uart_socket;
int out_udp_socket;
int in_udp_socket;

struct sockaddr_in dest_addr = {
     .sin_family = AF_INET,
     .sin_port = htons(CONFIG_MODEM_INET_SERVER_PORT)
};

int open_serial_socket() {
    int serial_socket;
    uart_config_t uart_config = {
            .baud_rate = CONFIG_FC_UART_BAUDRATE,
            .data_bits = UART_DATA_8_BITS,
            .parity    = UART_PARITY_DISABLE,
            .stop_bits = UART_STOP_BITS_1,
            .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
            .source_clk = UART_SCLK_DEFAULT
    };
    ESP_ERROR_CHECK(uart_param_config(UART_NUM_2, &uart_config));
    ESP_ERROR_CHECK(uart_set_pin(UART_NUM_2, CONFIG_FC_UART_TX_PIN, CONFIG_FC_UART_RX_PIN, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE));
    ESP_ERROR_CHECK(uart_driver_install(UART_NUM_2, 1024, 0, 0, NULL, 0));
    if ((serial_socket = open("/dev/uart/2", O_RDWR)) == -1) {
        ESP_LOGE(TAG, "Cannot open UART2");
        close(serial_socket);
        uart_driver_delete(UART_NUM_2);
        return ESP_FAIL;
    }
    esp_vfs_dev_uart_use_driver(2);
    return serial_socket;
}

int open_out_udp_socket()
{
    dest_addr.sin_addr.s_addr = inet_addr(CONFIG_MODEM_INET_SERVER_IP);

    int sock = socket(addr_family, SOCK_DGRAM, ip_protocol);
    if (sock < 0) {
        ESP_LOGE(TAG, "Unable to create socket: errno %d", errno);
        return sock;
    }

    // Set timeout
    struct timeval timeout;
    timeout.tv_sec = 1;
    timeout.tv_usec = 0;
    setsockopt (sock, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof timeout);

    ESP_LOGI(TAG, "Socket created, sending to %s:%d", CONFIG_MODEM_INET_SERVER_IP, CONFIG_MODEM_INET_SERVER_PORT);

    return sock;
}

uint8_t recv_buf[1024];

_Noreturn void communication_module_server(void *parameters)
{
    uart_socket = open_serial_socket();
    if (uart_socket < 0)
        return;

    out_udp_socket = open_out_udp_socket();
    if (out_udp_socket < 0)
        return;

    mavlink_status_t status[2];
    mavlink_message_t msg[2];
    uint8_t buf[300];
    while (1)
    {
        uint8_t c;
        int recived_size = read(uart_socket,&recv_buf,sizeof(recv_buf));
        if  (recived_size > 0)
        {
            for (int i = 0; i < recived_size; i++)
            {
                c = recv_buf[i];
                if (mavlink_parse_char(MAVLINK_COMM_0,c,&msg[0],&status[0]))
                {
                    int size = mavlink_msg_to_send_buffer(buf, &msg[0]);
                    int err = sendto(out_udp_socket, buf, size, 0, &dest_addr, sizeof(dest_addr));
                    ESP_LOGD(TAG,"sent %i",err);
                    if (err < 0) {
                        ESP_LOGE(TAG, "Error occurred during sending: errno %d", errno);
                    }
                }
            }
        }

        struct sockaddr_storage source_addr; // Large enough for both IPv4 or IPv6
        socklen_t socklen = sizeof(source_addr);
        recived_size = recvfrom(out_udp_socket,recv_buf,sizeof(recv_buf), 0, (struct sockaddr *)&source_addr, &socklen);
        if (recived_size > 0)
        {
            ESP_LOGD(TAG, "Received %i",recived_size);
            for (int i = 0; i < recived_size; i++)
            {
                c = recv_buf[i];
                if (mavlink_parse_char(MAVLINK_COMM_1,c,&msg[1],&status[1]))
                {
                    int size = mavlink_msg_to_send_buffer(buf, &msg[1]);
                    int err = write(uart_socket,buf,size);
                    if (err < 0) {
                        ESP_LOGE(TAG, "Error occurred during writing to UART: errno %d", errno);
                    }
                }
            }
        }
    }
    vTaskDelete(NULL);
}

void communication_module(void)
{
    xTaskCreate(&communication_module_server, "comm_server", 8192, NULL, 5, NULL);
}
