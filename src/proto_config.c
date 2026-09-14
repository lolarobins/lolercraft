// lolercraft
// login configuration packets

#include <lolercraft/protocol.h>

static bool sb_client_information (s_client *client) { return true; }


static bool sb_cookie_response (s_client *client) { return true; }
static bool sb_custom_payload (s_client *client) { return true; }
static bool sb_finish_configuration (s_client *client) { return true; }
static bool sb_keep_alive (s_client *client) { return true; }
static bool sb_pong (s_client *client) { return true; }
static bool sb_resource_pack (s_client *client) { return true; }
static bool sb_select_known_packs (s_client *client) { return true; }
static bool sb_custom_click_action (s_client *client) { return true; }
static bool sb_accept_code_of_conduct (s_client *client) { return true; }

p_sb_cb p_config_sb[]
   = { &sb_client_information,  &sb_cookie_response,
       &sb_custom_payload,      &sb_finish_configuration,
       &sb_keep_alive,          &sb_pong,
       &sb_resource_pack,       &sb_select_known_packs,
       &sb_custom_click_action, &sb_accept_code_of_conduct };
