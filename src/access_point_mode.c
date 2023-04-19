#include <FreeRTOS.h>
#include <task.h>
#include <queue.h>
#include "pico/cyw43_arch.h"
#include "dhcpserver.h"
#include "dnsserver.h"
#include "lwip/pbuf.h"
#include "lwip/tcp.h"

#include "app.h"
#include "rotary_button.h"
#include "u8g2.h"



#define TCP_PORT 80
#define POLL_TIME_S 5
#define HTTP_GET "GET"
#define HTTP_RESPONSE_HEADERS "HTTP/1.1 %d OK\nContent-Length: %d\nContent-Type: text/html; charset=utf-8\nConnection: close\n\n"
#define LED_TEST_BODY "<html><body><h1>Hello from Pico W.</h1><p>Led is %s</p><p><a href=\"?led=%d\">Turn led %s</a></body></html>"
#define LED_PARAM "led=%d"
#define LED_TEST "/ledtest"
#define LED_GPIO 0
#define HTTP_RESPONSE_REDIRECT "HTTP/1.1 302 Redirect\nLocation: http://%s" LED_TEST "\n\n"


typedef struct TCP_SERVER_T_ {
    struct tcp_pcb *server_pcb;
    bool complete;
    ip_addr_t gw;
} TCP_SERVER_T;

typedef struct TCP_CONNECT_STATE_T_ {
    struct tcp_pcb *pcb;
    int sent_len;
    char headers[128];
    char result[256];
    int header_len;
    int result_len;
    ip_addr_t *gw;
} TCP_CONNECT_STATE_T;


extern u8g2_t display_handler;
extern QueueHandle_t encoder_event_queue;

TCP_SERVER_T state;
dhcp_server_t dhcp_server;
dns_server_t dns_server;
TaskHandle_t ap_mode_display_render_handler = NULL;

char ap_ssid[] = "OpenTricker";
char ap_password[] = "1234567890";



void ap_mode_display_render_task(void *p) {
    char string_buf[30];

    while (true) {
        TickType_t last_render_tick = xTaskGetTickCount();

        u8g2_ClearBuffer(&display_handler);

        // Draw AP Info
        memset(string_buf, 0x0, sizeof(string_buf));
        snprintf(string_buf, sizeof(string_buf), "SSID: %s", ap_ssid);
        u8g2_SetFont(&display_handler, u8g2_font_6x12_tf);
        u8g2_DrawStr(&display_handler, 5, 10, string_buf);

        memset(string_buf, 0x0, sizeof(string_buf));
        snprintf(string_buf, sizeof(string_buf), "Pw: %s", ap_password);
        u8g2_SetFont(&display_handler, u8g2_font_6x12_tf);
        u8g2_DrawStr(&display_handler, 5, 20, string_buf);

        memset(string_buf, 0x0, sizeof(string_buf));
        snprintf(string_buf, sizeof(string_buf), "IP: 192.168.3.1");
        u8g2_SetFont(&display_handler, u8g2_font_6x12_tf);
        u8g2_DrawStr(&display_handler, 5, 30, string_buf);

        memset(string_buf, 0x0, sizeof(string_buf));
        snprintf(string_buf, sizeof(string_buf), "Press key to exit");
        u8g2_SetFont(&display_handler, u8g2_font_6x12_tf);
        u8g2_DrawStr(&display_handler, 5, 60, string_buf);

        u8g2_SendBuffer(&display_handler);

        vTaskDelayUntil(&last_render_tick, pdMS_TO_TICKS(20));
    }
}


static err_t tcp_close_client_connection(TCP_CONNECT_STATE_T *con_state, struct tcp_pcb *client_pcb, err_t close_err) {
    if (client_pcb) {
        assert(con_state && con_state->pcb == client_pcb);
        tcp_arg(client_pcb, NULL);
        tcp_poll(client_pcb, NULL, 0);
        tcp_sent(client_pcb, NULL);
        tcp_recv(client_pcb, NULL);
        tcp_err(client_pcb, NULL);
        err_t err = tcp_close(client_pcb);
        if (err != ERR_OK) {
            printf("close failed %d, calling abort\n", err);
            tcp_abort(client_pcb);
            close_err = ERR_ABRT;
        }
        if (con_state) {
            free(con_state);
        }
    }
    return close_err;
}

static void tcp_server_close(TCP_SERVER_T *state) {
    if (state->server_pcb) {
        tcp_arg(state->server_pcb, NULL);
        tcp_close(state->server_pcb);
        state->server_pcb = NULL;
    }
}

static err_t tcp_server_sent(void *arg, struct tcp_pcb *pcb, u16_t len) {
    TCP_CONNECT_STATE_T *con_state = (TCP_CONNECT_STATE_T*)arg;
    printf("tcp_server_sent %u\n", len);
    con_state->sent_len += len;
    if (con_state->sent_len >= con_state->header_len + con_state->result_len) {
        printf("all done\n");
        return tcp_close_client_connection(con_state, pcb, ERR_OK);
    }
    return ERR_OK;
}

static int test_server_content(const char *request, const char *params, char *result, size_t max_result_len) {
    int len = 0;
    if (strncmp(request, LED_TEST, sizeof(LED_TEST) - 1) == 0) {
        // // Get the state of the led
        // bool value;
        // cyw43_gpio_get(&cyw43_state, LED_GPIO, &value);
        // int led_state = value;

        // // See if the user changed it
        // if (params) {
        //     int led_param = sscanf(params, LED_PARAM, &led_state);
        //     if (led_param == 1) {
        //         if (led_state) {
        //             // Turn led on
        //             cyw43_gpio_set(&cyw43_state, 0, true);
        //         } else {
        //             // Turn led off
        //             cyw43_gpio_set(&cyw43_state, 0, false);
        //         }
        //     }
        // }
        // // Generate result
        // if (led_state) {
        //     len = snprintf(result, max_result_len, LED_TEST_BODY, "ON", 0, "OFF");
        // } else {
        //     len = snprintf(result, max_result_len, LED_TEST_BODY, "OFF", 1, "ON");
        // }
    }
    return len;
}

err_t tcp_server_recv(void *arg, struct tcp_pcb *pcb, struct pbuf *p, err_t err) {
    TCP_CONNECT_STATE_T *con_state = (TCP_CONNECT_STATE_T*)arg;
    if (!p) {
        printf("connection closed\n");
        return tcp_close_client_connection(con_state, pcb, ERR_OK);
    }
    assert(con_state && con_state->pcb == pcb);
    if (p->tot_len > 0) {
        printf("tcp_server_recv %d err %d\n", p->tot_len, err);
#if 0
        for (struct pbuf *q = p; q != NULL; q = q->next) {
            printf("in: %.*s\n", q->len, q->payload);
        }
#endif
        // Copy the request into the buffer
        pbuf_copy_partial(p, con_state->headers, p->tot_len > sizeof(con_state->headers) - 1 ? sizeof(con_state->headers) - 1 : p->tot_len, 0);

        // Handle GET request
        if (strncmp(HTTP_GET, con_state->headers, sizeof(HTTP_GET) - 1) == 0) {
            char *request = con_state->headers + sizeof(HTTP_GET); // + space
            char *params = strchr(request, '?');
            if (params) {
                if (*params) {
                    char *space = strchr(request, ' ');
                    *params++ = 0;
                    if (space) {
                        *space = 0;
                    }
                } else {
                    params = NULL;
                }
            }

            // Generate content
            con_state->result_len = test_server_content(request, params, con_state->result, sizeof(con_state->result));
            printf("Request: %s?%s\n", request, params);
            printf("Result: %d\n", con_state->result_len);

            // Check we had enough buffer space
            if (con_state->result_len > sizeof(con_state->result) - 1) {
                printf("Too much result data %d\n", con_state->result_len);
                return tcp_close_client_connection(con_state, pcb, ERR_CLSD);
            }

            // Generate web page
            if (con_state->result_len > 0) {
                con_state->header_len = snprintf(con_state->headers, sizeof(con_state->headers), HTTP_RESPONSE_HEADERS,
                    200, con_state->result_len);
                if (con_state->header_len > sizeof(con_state->headers) - 1) {
                    printf("Too much header data %d\n", con_state->header_len);
                    return tcp_close_client_connection(con_state, pcb, ERR_CLSD);
                }
            } else {
                // Send redirect
                con_state->header_len = snprintf(con_state->headers, sizeof(con_state->headers), HTTP_RESPONSE_REDIRECT,
                    ipaddr_ntoa(con_state->gw));
                printf("Sending redirect %s", con_state->headers);
            }

            // Send the headers to the client
            con_state->sent_len = 0;
            err_t err = tcp_write(pcb, con_state->headers, con_state->header_len, 0);
            if (err != ERR_OK) {
                printf("failed to write header data %d\n", err);
                return tcp_close_client_connection(con_state, pcb, err);
            }

            // Send the body to the client
            if (con_state->result_len) {
                err = tcp_write(pcb, con_state->result, con_state->result_len, 0);
                if (err != ERR_OK) {
                    printf("failed to write result data %d\n", err);
                    return tcp_close_client_connection(con_state, pcb, err);
                }
            }
        }
        tcp_recved(pcb, p->tot_len);
    }
    pbuf_free(p);
    return ERR_OK;
}

static err_t tcp_server_poll(void *arg, struct tcp_pcb *pcb) {
    TCP_CONNECT_STATE_T *con_state = (TCP_CONNECT_STATE_T*)arg;
    printf("tcp_server_poll_fn\n");
    return tcp_close_client_connection(con_state, pcb, ERR_OK); // Just disconnect clent?
}

static void tcp_server_err(void *arg, err_t err) {
    TCP_CONNECT_STATE_T *con_state = (TCP_CONNECT_STATE_T*)arg;
    if (err != ERR_ABRT) {
        printf("tcp_client_err_fn %d\n", err);
        tcp_close_client_connection(con_state, con_state->pcb, err);
    }
}

static err_t tcp_server_accept(void *arg, struct tcp_pcb *client_pcb, err_t err) {
    TCP_SERVER_T *state = (TCP_SERVER_T*)arg;
    if (err != ERR_OK || client_pcb == NULL) {
        printf("failure in accept\n");
        return ERR_VAL;
    }
    printf("client connected\n");

    // Create the state for the connection
    TCP_CONNECT_STATE_T *con_state = calloc(1, sizeof(TCP_CONNECT_STATE_T));
    if (!con_state) {
        printf("failed to allocate connect state\n");
        return ERR_MEM;
    }
    con_state->pcb = client_pcb; // for checking
    con_state->gw = &state->gw;

    // setup connection to client
    tcp_arg(client_pcb, con_state);
    tcp_sent(client_pcb, tcp_server_sent);
    tcp_recv(client_pcb, tcp_server_recv);
    tcp_poll(client_pcb, tcp_server_poll, POLL_TIME_S * 2);
    tcp_err(client_pcb, tcp_server_err);

    return ERR_OK;
}

static bool tcp_server_open(void *arg) {
    TCP_SERVER_T *state = (TCP_SERVER_T*)arg;
    printf("starting server on port %u\n", TCP_PORT);

    struct tcp_pcb *pcb = tcp_new_ip_type(IPADDR_TYPE_ANY);
    if (!pcb) {
        printf("failed to create pcb\n");
        return false;
    }

    err_t err = tcp_bind(pcb, IP_ANY_TYPE, TCP_PORT);
    if (err) {
        printf("failed to bind to port %d\n");
        return false;
    }

    state->server_pcb = tcp_listen_with_backlog(pcb, 1);
    if (!state->server_pcb) {
        printf("failed to listen\n");
        if (pcb) {
            tcp_close(pcb);
        }
        return false;
    }

    tcp_arg(state->server_pcb, state);
    tcp_accept(state->server_pcb, tcp_server_accept);

    return true;
}


void access_point_mode_init() {
    
    cyw43_arch_enable_ap_mode(ap_ssid, ap_password, CYW43_AUTH_WPA2_AES_PSK);

    ip4_addr_t mask;
    IP4_ADDR(ip_2_ip4(&state.gw), 192, 168, 3, 1);
    IP4_ADDR(ip_2_ip4(&mask), 255, 255, 255, 0);

    // Start DHCP server
    dhcp_server_init(&dhcp_server, &state.gw, &mask);

    // Start the DNS server
    dns_server_init(&dns_server, &state.gw);

    if (!tcp_server_open(&state)) {
        printf("failed to open server\n");
        exit(-1);
    }
}


void access_point_mode_deinit() {
    dns_server_deinit(&dns_server);
    dhcp_server_deinit(&dhcp_server);
}




uint8_t access_point_mode_menu()
{
    if (ap_mode_display_render_handler == NULL) {
        // The render task shall have lower priority than the current one
        UBaseType_t current_task_priority = uxTaskPriorityGet(xTaskGetCurrentTaskHandle());
        xTaskCreate(ap_mode_display_render_task, "AP Mode Display Render Task", configMINIMAL_STACK_SIZE, NULL, current_task_priority - 1, &ap_mode_display_render_handler);
    }
    else {
        vTaskResume(ap_mode_display_render_handler);
    }

    access_point_mode_init();

    bool quit = false;
    while (quit == false) {
        // Wait if quit is pressed
        ButtonEncoderEvent_t button_encoder_event;
        while (xQueueReceive(encoder_event_queue, &button_encoder_event, 0)){
            if (button_encoder_event == BUTTON_RST_PRESSED || button_encoder_event == BUTTON_ENCODER_PRESSED) {
                quit = true;
                break;
            }
        }
    }

    access_point_mode_deinit();

    vTaskSuspend(ap_mode_display_render_handler);

    return 36;  // Returns the AP mode page
}