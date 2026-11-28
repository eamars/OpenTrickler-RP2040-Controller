/*
 * The settings on API v1 (ADR-003, gate 3; api_v1.md): the routes of every module's table, and of the settings' own
 * (the export, a save of all, the schema). The answers are the settings' (settings.h), made as JSON in the reply's
 * buffer; this file has the routes and the web server (ADR-006, gate 6: the settings name no web).
 */
#include <stdio.h>
#include <string.h>

#include "api_v1.h"
#include "http_server.h"
#include "settings.h"
#include "unit_id.h"
#include "version.h"

static char api_settings_etag[48];


typedef void (*api_settings_answer_fn)(const settings_request_t *request, settings_answer_t *answer);

// A request answered by the settings: its JSON sent, or its plain error worded as the server's others
static void api_settings_answer(const http_server_request_t *request, http_server_reply_t *reply,
                                api_settings_answer_fn answer_fn) {
    settings_request_t asked = {.path = request->path, .body = request->body, .body_len = request->body_len};
    settings_answer_t answer;
    memset(&answer, 0, sizeof(answer));
    answer.out = http_server_reply_buffer(reply, &answer.room);
    answer.job_state = api_v1_job_state();
    answer_fn(&asked, &answer);
    if (answer.error_code != NULL) {
        http_server_reply_error(reply, answer.status, answer.error_code, answer.error_message);
        return;
    }
    http_server_reply_send(reply, answer.status, "application/json", answer.len);
}


static void api_settings_get(const http_server_request_t *request, http_server_reply_t *reply) {
    api_settings_answer(request, reply, settings_api_get);
}


static void api_settings_patch(const http_server_request_t *request, http_server_reply_t *reply) {
    api_settings_answer(request, reply, settings_api_patch);
}


static void api_settings_save(const http_server_request_t *request, http_server_reply_t *reply) {
    api_settings_answer(request, reply, settings_api_save);
}


static void api_settings_save_all(const http_server_request_t *request, http_server_reply_t *reply) {
    api_settings_answer(request, reply, settings_api_save_all);
}


static void api_settings_revert(const http_server_request_t *request, http_server_reply_t *reply) {
    api_settings_answer(request, reply, settings_api_revert);
}


// GET /api/v1/settings: every resource in one document, with the unit's id
static void api_settings_export(const http_server_request_t *request, http_server_reply_t *reply) {
    (void) request;
    char unit_id[UNIT_ID_LENGTH + 1];
    unit_id_get(unit_id, sizeof(unit_id));
    settings_export_t *export_context = http_server_reply_context(reply, sizeof(settings_export_t));
    settings_export_begin(export_context, unit_id);
    http_server_reply_stream(reply, 200, "application/json", settings_export_more, export_context);
}


// The commands' share of the schema, by the settings' numbering of its parts (settings.h)
static const char *api_settings_schema_extra(unsigned part, size_t idx) {
    static const api_v1_schema_part_t parts[] = {API_V1_SCHEMA_ACTIONS, API_V1_SCHEMA_STATES, API_V1_SCHEMA_EVENTS,
                                                  API_V1_SCHEMA_THROW};
    return part < sizeof(parts) / sizeof(parts[0]) ? api_v1_schema_item(parts[part], idx) : NULL;
}


// GET /api/v1/schema
static void api_settings_schema(const http_server_request_t *request, http_server_reply_t *reply) {
    (void) request;
    settings_schema_t *schema = http_server_reply_context(reply, sizeof(settings_schema_t));
    settings_schema_begin(schema, api_settings_schema_extra);
    http_server_reply_stream(reply, 200, "application/json", settings_schema_more, schema);
    http_server_reply_etag(reply, api_settings_etag);
}


void api_v1_settings_init(void) {
    snprintf(api_settings_etag, sizeof(api_settings_etag), "%s-%s", version_string, vcs_hash);
    const settings_resource_t *resource;
    for (size_t idx = 0; (resource = settings_resource_at(idx)) != NULL; idx += 1) {
        http_server_add_route(HTTP_SERVER_GET, resource->path, api_settings_get);
        http_server_add_route(HTTP_SERVER_PATCH, resource->path, api_settings_patch);
        // Resources stored as one block share their save and their revert: a route once
        bool save_before = false;
        for (size_t earlier = 0; earlier < idx; earlier += 1) {
            save_before = save_before || strcmp(settings_resource_at(earlier)->save_path, resource->save_path) == 0;
        }
        if (!save_before) {
            http_server_add_route(HTTP_SERVER_POST, resource->save_path, api_settings_save);
            http_server_add_route(HTTP_SERVER_POST, resource->revert_path, api_settings_revert);
        }
    }
    http_server_add_route(HTTP_SERVER_GET, "/api/v1/settings", api_settings_export);
    http_server_add_route(HTTP_SERVER_POST, "/api/v1/settings/save", api_settings_save_all);
    http_server_add_route(HTTP_SERVER_GET, "/api/v1/schema", api_settings_schema);
}
