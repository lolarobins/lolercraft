// lolercraft
// login configuration packets

#include <lolercraft/protocol.h>

// mutual packets from proto_config.c
extern bool sb_client_information (s_client *client); // 0xE
extern bool sb_custom_payload (s_client *client);     // 0x16
extern bool sb_keep_alive (s_client *client);         // 0x1C
extern bool sb_pong (s_client *client);               // 0x2D

p_sb_cb p_play_cb[] = { [0xE]  = &sb_client_information,
                        [0x16] = &sb_custom_payload,
                        [0x1C] = &sb_keep_alive,
                        [0x2D] = &sb_pong };
