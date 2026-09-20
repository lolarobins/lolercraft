// lolercraft
// login configuration packets

#include "lolercraft/logging.h"
#include <lolercraft/protocol.h>

// config (0x0) play (0xE)
bool sb_client_information (s_client *client) {
    client->locale          = p_read_string (client, 0);
    client->render_distance = p_read_uint8 (client);
    p_read_uint8 (client); // useless enum varint
    client->chat_mode  = p_read_uint8 (client);
    client->chat_col   = p_read_uint8 (client);
    client->skin_parts = p_read_uint8 (client);
    p_read_uint8 (client); // useless enum varint
    client->main_hand         = p_read_uint8 (client);
    client->chat_swear_filter = p_read_uint8 (client);
    client->show_in_list      = p_read_uint8 (client);
    p_read_uint8 (client); // useless enum varint
    client->particle_status = p_read_uint8 (client);

    if(client->state == 4) {
        // TODO
    }

    return true;
}

static bool sb_cookie_response (s_client *client) {
    log_debug ("unimplemented cookie_response packet");
    return true;
}

// config (0x2) play (0x16)
bool sb_custom_payload (s_client *client) {
    char *id = p_read_string (client, 0);

    // TODO: event handler system for this

    if (!strcmp (id, "minecraft:brand")) {
        client->brand = p_read_string (client, 0);

        if (!client->brand) {
            free (id);
            return false;
        }
    }

    free (id);
    return true;
}

extern bool sb_finish_configuration (s_client *client) {
    log_debug ("unimplemented finish_configuration packet");
    return true;
}

// config (0x4) play (0x1c)
bool sb_keep_alive (s_client *client) { return true; }

// config (0x5) play (0x2d)
bool sb_pong (s_client *client) { return true; }

static bool sb_resource_pack (s_client *client) {
    log_debug ("unimplemented resource_pack packet");
    return true;
}
static bool sb_select_known_packs (s_client *client) {
    log_debug ("unimplemented select_known_packs packet");
    return true;
}
static bool sb_custom_click_action (s_client *client) {
    log_debug ("unimplemented custom_click_action packet");
    return true;
}
static bool sb_accept_code_of_conduct (s_client *client) {
    log_debug ("unimplemented code_of_conduct packet");
    return true;
}

p_sb_cb p_config_sb[] = { &sb_client_information,  &sb_cookie_response,
                          &sb_custom_payload,      &sb_finish_configuration,
                          &sb_keep_alive,          &sb_pong,
                          &sb_resource_pack,       &sb_select_known_packs,
                          &sb_custom_click_action, &sb_accept_code_of_conduct };
