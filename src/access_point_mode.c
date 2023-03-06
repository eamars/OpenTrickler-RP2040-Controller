#include <FreeRTOS.h>
#include <task.h>
#include <queue.h>
#include "pico/cyw43_arch.h"
#include "dhcpserver.h"
#include "dnsserver.h"
#include "app.h"
#include "rotary_button.h"
#include "u8g2.h"



typedef struct TCP_SERVER_T_ {
    struct tcp_pcb *server_pcb;
    bool complete;
    ip_addr_t gw;
} TCP_SERVER_T;

TCP_SERVER_T state;


extern u8g2_t display_handler;
extern QueueHandle_t encoder_event_queue;

TaskHandle_t ap_mode_display_render_handler = NULL;

char ap_ssid[] = "OpenTricker";
char ap_password[] = "1234567890";


// static bool tcp_server_open(void *arg) {
//     TCP_SERVER_T *state = (TCP_SERVER_T*)arg;
//     printf("starting server on port %u\n", TCP_PORT);

//     struct tcp_pcb *pcb = tcp_new_ip_type(IPADDR_TYPE_ANY);
//     if (!pcb) {
//         printf("failed to create pcb\n");
//         return false;
//     }

//     err_t err = tcp_bind(pcb, IP_ANY_TYPE, TCP_PORT);
//     if (err) {
//         printf("failed to bind to port %d\n");
//         return false;
//     }

//     state->server_pcb = tcp_listen_with_backlog(pcb, 1);
//     if (!state->server_pcb) {
//         printf("failed to listen\n");
//         if (pcb) {
//             tcp_close(pcb);
//         }
//         return false;
//     }

//     tcp_arg(state->server_pcb, state);
//     tcp_accept(state->server_pcb, tcp_server_accept);

//     return true;
// }


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


void access_point_mode_init() {
    
    cyw43_arch_enable_ap_mode(ap_ssid, ap_password, CYW43_AUTH_WPA2_AES_PSK);

    ip4_addr_t mask;
    IP4_ADDR(ip_2_ip4(&state.gw), 192, 168, 3, 1);
    IP4_ADDR(ip_2_ip4(&mask), 255, 255, 255, 0);

    // Start DHCP server
    dhcp_server_t dhcp_server;
    dhcp_server_init(&dhcp_server, &state.gw, &mask);

    // Start the DNS server
    dns_server_t dns_server;
    dns_server_init(&dns_server, &state.gw);

    // if (!tcp_server_open(state)) {
    //     printf("failed to open server\n");
    //     exit(-1);
    // }
}




AppState_t access_point_mode_menu(AppState_t prev_state)
{
    AppState_t exit_state = APP_STATE_ENTER_CONFIG_MENU_PAGE;

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

    vTaskSuspend(ap_mode_display_render_handler);

    return exit_state;
}