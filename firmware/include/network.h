#pragma once
#include <stdint.h>

void network_begin();
// These calls never wait for sockets. false reports queue full/unavailable.
bool network_submit_shot(const char *json, int64_t captured_us, bool timestamp_ready);
void network_submit_status(const char *json);
bool network_timestamp(char out[21], int64_t captured_us);
