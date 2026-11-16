#include <pico/cyw43_arch.h>
#include <FreeRTOS.h>
#include <queue.h>

#include "wireless.h"
#include "eeprom.h"
#include "access_point_mode.h"
#include "display.h"
#include "mini_12864_module.h"
#include "rest_endpoints.h"
#include "http_server.h"
#include "common.h"
#include "settings.h"
#include "version.h"
#include "update.h"
#include "update_poll.h"
#include "lwip/apps/mdns.h"
#include <string.h>
#include <strings.h>

#define WIRELESS_REJOIN_MS 60000            // a station set up and not joined tries again this often (ADR-004) p
#define WIRELESS_REJOIN_TIMEOUT_MS 15000    // each try's time to join

static void wireless_station_mdns(void);
static void wireless_rejoin(void);
static void srv_txt(struct mdns_service *service, void *txt_userdata);


#ifdef CYW43_HOST_NAME
#undef CYW43_HOST_NAME
#endif

// Overwrite the host name
#define CYW43_HOST_NAME "opentrickler"
#define LED_INTERFACE_MINIMUM_POLL_PERIOD_MS    20

// The hostname is used by STA mode to advertise mDNS. The value is also used for AP mode as the SSID
char host_name[18];

typedef enum {
    WIRELESS_STATE_NOT_INITIALIZED = 0,
    WIRELESS_STATE_IDLE,
    WIRELESS_STATE_AP_MODE_INIT,
    WIRELESS_STATE_AP_MODE_LISTEN,
    WIRELESS_STATE_STA_MODE_INIT,
    WIRELESS_STATE_STA_MODE_LISTEN,
} wireless_state_t;

typedef enum {
    WIRELESS_CTRL_NOP = 0,
    WIRELESS_CTRL_CYW43_INIT,
    WIRELESS_CTRL_CYW43_DEINIT,
    WIRELESS_CTRL_DISCONNECT,
    WRIELESS_CTRL_START_AP_MODE,
    WRIELESS_CTRL_START_STA_MODE,
    WIRELESS_CTRL_LED_ON,
    WIRELESS_CTRL_LED_OFF,
} wireless_ctrl_t;


typedef struct {
    eeprom_wireless_metadata_t eeprom_wireless_metadata;
    wireless_state_t current_wireless_state;
} wireless_config_t;


static wireless_config_t wireless_config;
const eeprom_wireless_metadata_t default_eeprom_wireless_metadata = {
    .wireless_data_rev = 0,
    .ssid = "",
    .pw = "",
    .auth = AUTH_WPA2_MIXED_PSK,
    .timeout_ms = 30000,    // 30s
    .enable = false,
};

static QueueHandle_t wireless_ctrl_queue;

// Render task
const char * wireless_state_strings[] = {
    "Not Initialized",
    "Idling",
    "AP Mode Init",
    "AP Mode Listen",
    "STA Mode Init",
    "STA Mode Listen"
};




/*
    The Wi-Fi's settings, described once (ADR-003, gate 3; api_v1.md, /api/v1/settings/wifi). Each applies at the
    next start, when the unit joins the network. The key is written only: never in a reply, nor in the export.
*/
static const setting_choice_t wifi_auth_choices[] = {
    {.held = AUTH_OPEN, .text = "open", .label = "Open"},
    {.held = AUTH_WPA_TKIP_PSK, .text = "wpa_tkip", .label = "WPA (TKIP)"},
    {.held = AUTH_WPA2_AES_PSK, .text = "wpa2_aes", .label = "WPA2 (AES)"},
    {.held = AUTH_WPA2_MIXED_PSK, .text = "wpa2_mixed", .label = "WPA/WPA2 mixed"},
};


#define WIFI_FIELD(member) SETTING_AT(eeprom_wireless_metadata_t, member)

static const setting_field_t wifi_fields[] = {
    {.name = "enable", .label = "Wi-Fi", .group = "Wi-Fi",
     .about = "The unit joins this network at its start; off, it is its own access point",
     .type = SETTING_BOOLEAN, WIFI_FIELD(enable), .applies = "at_restart"},
    {.name = "ssid", .label = "Network name", .group = "Wi-Fi",
     .about = "The network's name (SSID)",
     .type = SETTING_STRING, WIFI_FIELD(ssid), .length_min = 1, .applies = "at_restart"},
    {.name = "key", .label = "Key", .group = "Wi-Fi",
     .about = "The network's key; never sent back",
     .type = SETTING_STRING, WIFI_FIELD(pw), .access = SETTING_WRITE_ONLY, .applies = "at_restart"},
    {.name = "auth", .label = "Security", .group = "Wi-Fi",
     .about = "The network's security",
     .type = SETTING_CHOICE, WIFI_FIELD(auth), SETTING_CHOICES(wifi_auth_choices), .applies = "at_restart"},
    {.name = "timeout_ms", .label = "Join timeout", .group = "Wi-Fi",
     .about = "How long the unit tries to join before it gives up",
     .type = SETTING_INTEGER, WIFI_FIELD(timeout_ms), .unit = "ms", .min = 1000.0f, .max = 120000.0f,
     .applies = "at_restart"},
};


static void *wifi_settings_live(int32_t id) {
    (void) id;
    return &wireless_config.eeprom_wireless_metadata;
}


static bool wifi_settings_is_saved(void) {
    return config_is_saved(EEPROM_WIRELESS_CONFIG_BASE_ADDR, &wireless_config.eeprom_wireless_metadata,
                           sizeof(eeprom_wireless_metadata_t));
}


static const settings_resource_t wifi_settings = {
    .path = "/api/v1/settings/wifi",
    .save_path = "/api/v1/settings/wifi/save",
    .label = "Wi-Fi",
    .about = "The network the unit joins",
    SETTING_FIELDS(wifi_fields),
    .refused_while = SETTINGS_REFUSED_NEVER,
    .size = sizeof(eeprom_wireless_metadata_t),
    .live = wifi_settings_live,
    .save = wireless_config_save,
    .is_saved = wifi_settings_is_saved,
};


bool wireless_init() {
    bool is_ok = true;

    memset(&wireless_config, 0x00, sizeof(wireless_config_t));
    is_ok = load_config(EEPROM_WIRELESS_CONFIG_BASE_ADDR, &wireless_config.eeprom_wireless_metadata, &default_eeprom_wireless_metadata, sizeof(wireless_config.eeprom_wireless_metadata), EEPROM_WIRELESS_CONFIG_METADATA_REV);
    if (!is_ok) {
        printf("Unable to read WiFi configuration\n");
        return false;
    }

    /*
        The name and the key are used as strings everywhere, and are stored in 32 and 64 bytes: a name of 32
        characters or a key of 64 had no ending, and what read them ran on into what lies behind (ADR-012; audit W2).
        One stored that long before this is cut by its last character here; it must be entered again.
    */
    wireless_config.eeprom_wireless_metadata.ssid[sizeof(wireless_config.eeprom_wireless_metadata.ssid) - 1] = '\0';
    wireless_config.eeprom_wireless_metadata.pw[sizeof(wireless_config.eeprom_wireless_metadata.pw) - 1] = '\0';

    // Create Wireless handler task
    xTaskCreate(wireless_task, "Wireless Task", 512, NULL, 3, NULL);

    // Register to eeprom save all
    eeprom_register_handler(wireless_config_save);
    settings_register(&wifi_settings);

    // Generate the hostname: the id's first four characters, with their ending (ADR-003, gate 4). The id was printed
    // with %s from four bytes and no ending, and whatever lay after them on the stack went into the name
    char id[5];
    eeprom_get_board_id(id, 4);
    id[4] = '\0';
    snprintf(host_name, sizeof(host_name), "opentrickler-%s", id);


    return is_ok;
}


bool wireless_config_save() {
    bool is_ok = save_config(EEPROM_WIRELESS_CONFIG_BASE_ADDR, &wireless_config.eeprom_wireless_metadata, sizeof(eeprom_wireless_metadata_t));
    return is_ok;
}


uint32_t get_cyw43_auth(cyw43_auth_t auth) {
    uint32_t cyw43_auth = 0;

    switch (auth) {
        case AUTH_OPEN:
            cyw43_auth = CYW43_AUTH_OPEN;
            break;
        case AUTH_WPA_TKIP_PSK:
            cyw43_auth = CYW43_AUTH_WPA_TKIP_PSK;
            break;
        case AUTH_WPA2_AES_PSK:
            cyw43_auth = CYW43_AUTH_WPA2_AES_PSK;
            break;
        case AUTH_WPA2_MIXED_PSK:
            cyw43_auth = CYW43_AUTH_WPA2_MIXED_PSK;
            break;
        default:
            break;
    }

    return cyw43_auth;
}


void led_interface_task(void *p) {
    wireless_ctrl_t wireless_ctrl;
    uint32_t blink_interval_ms = 0;
    bool prev_led_state = false;
    bool led_state = prev_led_state;

    while (true) {
        prev_led_state = led_state;

        switch (wireless_config.current_wireless_state) {
            case WIRELESS_STATE_NOT_INITIALIZED:
            case WIRELESS_STATE_IDLE:
                blink_interval_ms = 0;
                led_state = false;
                break;
            case WIRELESS_STATE_AP_MODE_INIT:
                led_state = !led_state;
                blink_interval_ms = 20;
                break;
            case WIRELESS_STATE_AP_MODE_LISTEN:
                led_state = !led_state;
                blink_interval_ms = 1000;
                break;
            case WIRELESS_STATE_STA_MODE_INIT:
                led_state = !led_state;
                blink_interval_ms = 20;
                break;
            case WIRELESS_STATE_STA_MODE_LISTEN:
                led_state = true;
                blink_interval_ms = 0;
                break;
            default:
                break;
        }

        if (led_state != prev_led_state) {
            if (led_state) {
                wireless_ctrl = WIRELESS_CTRL_LED_ON;
            }
            else {
                wireless_ctrl = WIRELESS_CTRL_LED_OFF;
            }
            xQueueSend(wireless_ctrl_queue, &wireless_ctrl, 0);
        }

        
        vTaskDelay(pdMS_TO_TICKS(MAX(blink_interval_ms, LED_INTERFACE_MINIMUM_POLL_PERIOD_MS)));
    }
}


// The unit's address on the network it is on: its station's, or its own access point's
static bool wireless_own_address(char *buf, size_t len) {
    int itf = wireless_config.current_wireless_state == WIRELESS_STATE_AP_MODE_LISTEN ? CYW43_ITF_AP : CYW43_ITF_STA;
    const ip4_addr_t *addr = netif_ip4_addr(&cyw43_state.netif[itf]);
    if (addr == NULL || ip4_addr_isany(addr)) {
        return false;
    }
    return ip4addr_ntoa_r(addr, buf, (int) len) != NULL;
}


/*
 * The Wi-Fi settings the unit started with. A write to them applies at the restart, as their table says: the retry and
 * the trial's confirmation read this copy, never a write not yet saved, nor one half made (review C2)
 */
static eeprom_wireless_metadata_t wireless_started_with;


// mDNS on the station, once: the unit's name, and its two services
static void wireless_station_mdns(void) {
    static bool announced = false;
    if (announced) {
        return;
    }
    announced = true;
    cyw43_arch_lwip_begin();
    mdns_resp_init();

    // The opentrickler can be accessed via hostname.local
    mdns_resp_add_netif(&cyw43_state.netif[CYW43_ITF_STA], host_name);

    // Add service HTTP, allowing user to access the web interface with hostname.local format
    mdns_resp_add_service(&cyw43_state.netif[CYW43_ITF_STA], "rest_httpd", "_http", DNSSD_PROTO_TCP, 80, srv_txt, NULL);

    // Add secondary service to allow client to discover with service _opentrickler._tcp.local
    mdns_resp_add_service(&cyw43_state.netif[CYW43_ITF_STA], "app_httpd", "_opentrickler", DNSSD_PROTO_TCP, 80, srv_txt, NULL);
    cyw43_arch_lwip_end();
}


/*
 * A station set up and not joined tries again (ADR-004; handed on by ADR-012): the unit that started before its
 * router, as its own access point, and the one whose router went away. The access point stays up while it tries, so a
 * person can still reach the unit; once the station joins, it stops, and the root page is the portal again
 */
static void wireless_rejoin(void) {
    if (!wireless_started_with.enable || cyw43_tcpip_link_status(&cyw43_state, CYW43_ITF_STA) == CYW43_LINK_UP) {
        return;
    }
    const char *password = wireless_started_with.auth != AUTH_OPEN ? wireless_started_with.pw : NULL;
    printf("Wi-Fi: not joined; trying %s again\n", wireless_started_with.ssid);
    int joined = cyw43_arch_wifi_connect_timeout_ms(wireless_started_with.ssid, password,
                                                    get_cyw43_auth(wireless_started_with.auth),
                                                    WIRELESS_REJOIN_TIMEOUT_MS);
    if (joined != PICO_OK) {
        printf("Wi-Fi: not joined (%d)\n", joined);
        return;
    }
    if (wireless_config.current_wireless_state == WIRELESS_STATE_AP_MODE_LISTEN) {
        access_point_mode_stop();
        cyw43_arch_disable_ap_mode();
        rest_endpoints_set_root_wizard(false);
    }
    wireless_config.current_wireless_state = WIRELESS_STATE_STA_MODE_LISTEN;
    wireless_station_mdns();
    printf("Wi-Fi: joined %s\n", wireless_started_with.ssid);
    // An image on trial is on its network now; the start's poll, if it has not run (ADR-010, gate 5)
    update_network_ready(true);
    update_poll_station_joined();
}


// The console's `wifi leave` (ADR-004): the station leaves its network, as when its router goes away
bool wireless_station_leave(void) {
    cyw43_arch_lwip_begin();
    int left = cyw43_wifi_leave(&cyw43_state, CYW43_ITF_STA);
    cyw43_arch_lwip_end();
    return left == 0;
}


// The server listens (ADR-004): an image on trial confirms itself once the unit is on the network it is set up
// for, a station when Wi-Fi is set up, its own access point when it is not. A unit set up for Wi-Fi that could not
// join, and fell back to its own access point, leaves its trial, and goes back
static void wireless_server_listening(void) {
    bool on_its_network = wireless_started_with.enable
                              ? wireless_config.current_wireless_state == WIRELESS_STATE_STA_MODE_LISTEN
                              : wireless_config.current_wireless_state == WIRELESS_STATE_AP_MODE_LISTEN;
    update_network_ready(on_its_network);
    // The update's poll, once each start, as a station only: its own access point has no internet (ADR-010, gate 5)
    if (wireless_config.current_wireless_state == WIRELESS_STATE_STA_MODE_LISTEN) {
        update_poll_station_joined();
    }
}


// Whether a request's Host is the unit's own: its address, or its host name with or without ".local", any port
// (ADR-003: a page that has pointed its own name at the unit is refused)
static bool wireless_host_is_own(const char *host) {
    char name[64];
    size_t len = strcspn(host, ":");
    if (len >= sizeof(name)) {
        return false;
    }
    memcpy(name, host, len);
    name[len] = '\0';
    if (len > 0 && name[len - 1] == '.') {
        name[--len] = '\0';     // a fully qualified name's trailing dot
    }

    char address[24];
    if (wireless_own_address(address, sizeof(address)) && strcmp(name, address) == 0) {
        return true;
    }
    size_t host_len = strlen(host_name);
    if (strncasecmp(name, host_name, host_len) == 0 &&
        (name[host_len] == '\0' || strcasecmp(name + host_len, ".local") == 0)) {
        return true;
    }
    return false;
}


static void srv_txt(struct mdns_service *service, void *txt_userdata)
{
  err_t res;
  LWIP_UNUSED_ARG(txt_userdata);

  res = mdns_resp_add_service_txtitem(service, "path=/", 6);
  LWIP_ERROR("mdns add service txt failed\n", (res == ERR_OK), return);

  // The API's version and the firmware's, so a client finds which unit runs what before it asks (ADR-003, gate 4)
  res = mdns_resp_add_service_txtitem(service, "api=1", 5);
  LWIP_ERROR("mdns add service txt failed\n", (res == ERR_OK), return);
  char firmware[64];
  int len = snprintf(firmware, sizeof(firmware), "firmware=%s", version_string);
  if (len > 0 && len < (int) sizeof(firmware)) {
      res = mdns_resp_add_service_txtitem(service, firmware, (u8_t) len);
      LWIP_ERROR("mdns add service txt failed\n", (res == ERR_OK), return);
  }
}


void wireless_get_link(wireless_info_t *info) {
    memset(info, 0, sizeof(*info));
    info->hostname = host_name;
    switch (wireless_config.current_wireless_state) {
        case WIRELESS_STATE_STA_MODE_INIT:
        case WIRELESS_STATE_STA_MODE_LISTEN:
            info->mode = "station";
            snprintf(info->ssid, sizeof(info->ssid), "%s", wireless_config.eeprom_wireless_metadata.ssid);
            break;
        case WIRELESS_STATE_AP_MODE_INIT:
        case WIRELESS_STATE_AP_MODE_LISTEN:
            info->mode = "access_point";
            snprintf(info->ssid, sizeof(info->ssid), "%s", host_name);     // the access point is named so
            break;
        default:
            info->mode = "none";
            break;
    }
    if (!wireless_own_address(info->address, sizeof(info->address))) {
        info->address[0] = '\0';
    }
}


void wireless_get_info(wireless_info_t *info) {
    wireless_get_link(info);
    if (wireless_config.current_wireless_state == WIRELESS_STATE_STA_MODE_LISTEN) {
        int32_t rssi = 0;
        cyw43_arch_lwip_begin();
        info->has_rssi = cyw43_wifi_get_rssi(&cyw43_state, &rssi) == 0;
        cyw43_arch_lwip_end();
        info->rssi = rssi;
    }
}

void wireless_task(void *p) {
    static TaskHandle_t led_interface_task_handler = NULL;


    wireless_config.current_wireless_state = WIRELESS_STATE_NOT_INITIALIZED;
    wireless_ctrl_queue = xQueueCreate(5, sizeof(wireless_ctrl_t));

    if (cyw43_arch_init()) {
        exit(-1);
    }

    wireless_config.current_wireless_state = WIRELESS_STATE_IDLE;

    // Create LED task
    if (led_interface_task_handler == NULL) {
        // The render task shall have lower priority than the current one
        UBaseType_t current_task_priority = uxTaskPriorityGet(xTaskGetCurrentTaskHandle());
        xTaskCreate(led_interface_task, "LED Interface Task", configMINIMAL_STACK_SIZE, NULL, current_task_priority - 1, &led_interface_task_handler);
    }
    else {
        vTaskResume(led_interface_task_handler);
    }

    // What the unit starts with is what it keeps until its restart (the settings' at_restart)
    wireless_started_with = wireless_config.eeprom_wireless_metadata;

    // Start default initialize pattern
    if (wireless_config.eeprom_wireless_metadata.enable) {
        wireless_config.current_wireless_state = WIRELESS_STATE_STA_MODE_INIT;
        cyw43_arch_enable_sta_mode();

        // If the authentication method is open then the password is NULL
        const char * wifi_password = NULL;
        if (wireless_config.eeprom_wireless_metadata.auth != AUTH_OPEN) {
            wifi_password = wireless_config.eeprom_wireless_metadata.pw;
        }

        // Retry within timeframe. A test build skips it, as if the router were down at the start (ADR-004)
        TickType_t stop_tick = xTaskGetTickCount() + pdMS_TO_TICKS(wireless_config.eeprom_wireless_metadata.timeout_ms);
#if RC_TEST_WIFI_FIRST_JOIN_FAILS
        stop_tick = xTaskGetTickCount();
        printf("Wi-Fi: a test build: the start's join skipped\n");
#endif
        while (xTaskGetTickCount() < stop_tick) {
            int resp;
            resp = cyw43_arch_wifi_connect_timeout_ms(wireless_config.eeprom_wireless_metadata.ssid,
                                                      wifi_password,
                                                      get_cyw43_auth(wireless_config.eeprom_wireless_metadata.auth),
                                                      wireless_config.eeprom_wireless_metadata.timeout_ms);
            if (resp == PICO_OK) {
                wireless_config.current_wireless_state = WIRELESS_STATE_STA_MODE_LISTEN;
                break;
            }
            else {

            }
        }
    }

    // If the state didn't change (connection failed) then we shall put it back to idle
    if (wireless_config.current_wireless_state == WIRELESS_STATE_STA_MODE_INIT) {
         wireless_config.current_wireless_state = WIRELESS_STATE_IDLE;
    }


    // If not configured, or failed to connect existing wifi then start the AP mode
    if (wireless_config.current_wireless_state == WIRELESS_STATE_IDLE) {
        // If previous configured, then start STA mode by default
        wireless_config.current_wireless_state = WIRELESS_STATE_AP_MODE_INIT;
        access_point_mode_start();
        wireless_config.current_wireless_state = WIRELESS_STATE_AP_MODE_LISTEN;
    }
    else {
        wireless_station_mdns();
    }

    // Initialize REST endpoints
    // If the current wireless state is AP mode then we will map / to the wifi configuration
    rest_endpoints_init(wireless_config.current_wireless_state == WIRELESS_STATE_AP_MODE_LISTEN);

    // Start our own HTTP server (ADR-003, gate 2), in its own task, on lwIP's sockets
    static const http_server_network_t http_network = {
        .host_is_own = wireless_host_is_own,
        .own_address = wireless_own_address,
        .listening = wireless_server_listening,
    };
    http_server_start(&http_network);

    // A station set up and not joined tries again every WIRELESS_REJOIN_MS (ADR-004), by its own clock: as its own
    // access point the unit blinks its LED through this queue, which is then never quiet for that long
    TickType_t rejoin_at = xTaskGetTickCount() + pdMS_TO_TICKS(WIRELESS_REJOIN_MS);
    while (true) {
        wireless_ctrl_t wireless_ctrl;

        TickType_t now = xTaskGetTickCount();
        if ((int32_t) (now - rejoin_at) >= 0) {
            wireless_rejoin();
            rejoin_at = xTaskGetTickCount() + pdMS_TO_TICKS(WIRELESS_REJOIN_MS);
            continue;
        }
        if (xQueueReceive(wireless_ctrl_queue, &wireless_ctrl, rejoin_at - now) != pdTRUE) {
            continue;
        }

        switch (wireless_ctrl) {
            case WIRELESS_CTRL_LED_ON:
                cyw43_arch_gpio_put(CYW43_WL_GPIO_LED_PIN, true);
                break;
            case WIRELESS_CTRL_LED_OFF:
                cyw43_arch_gpio_put(CYW43_WL_GPIO_LED_PIN, false);
                break;
            default:
                break;
        }
    }

    mdns_resp_remove_netif(&cyw43_state.netif[CYW43_ITF_STA]);
    cyw43_arch_deinit();

    vTaskSuspend(led_interface_task_handler);
}


