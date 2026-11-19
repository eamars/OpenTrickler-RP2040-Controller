#ifndef UNIT_ID_H_
#define UNIT_ID_H_

#include <stddef.h>

#define UNIT_ID_LENGTH          8   // the unit's id: eight capital hex digits
#define UNIT_HOTSPOT_KEY_LENGTH 8   // its hotspot's key: eight small letters and digits

#ifdef __cplusplus
extern "C" {
#endif

// The unit's id, made from the chip's own unique id: the same across a factory reset, and never two units alike.
// The host name and the hotspot's name show its first four characters (ADR-008, gate 4)
void unit_id_get(char *id, size_t room);

// The unit's hotspot's key, made from the chip's own unique id by another hash than the id's, so the id, which the
// network hears, gives nothing of it away. Shown only on the unit's screen: being at the unit is the password
void unit_id_hotspot_key(char *key, size_t room);

#ifdef __cplusplus
}
#endif

#endif  // UNIT_ID_H_
