#ifndef FIRMWARE_UPDATE_H_
#define FIRMWARE_UPDATE_H_

#include <lwip/apps/fs.h>

#include "firmware_footer.h"

#ifdef __cplusplus
extern "C" {
#endif

void firmware_update_init(void);

// Reads and validates the fixed footer slot at the end of the OTA staging
// region directly from flash. Returns false if no valid image is staged
// (magic mismatch or footer_crc32 mismatch).
bool firmware_update_get_staged_info(firmware_update_footer_t *out);

// Marks the validated staged image as pending install (a flag in the
// footer's reserved bytes, staging region only -- never touches the app
// region itself) and reboots. The second-stage bootloader
// (bootloader/main.c) is what actually copies the image into the app
// region on the next boot. Returns false if there is no valid staged image
// or the flag couldn't be written; does not return on success.
bool firmware_update_install_and_reboot(void);

// REST interface
bool http_rest_firmware_update_status(struct fs_file *file, int num_params, char *params[], char *values[]);
bool http_rest_firmware_install(struct fs_file *file, int num_params, char *params[], char *values[]);

#ifdef __cplusplus
}
#endif

#endif  // FIRMWARE_UPDATE_H_
