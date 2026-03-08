#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include "socket_util.h"
#include "bundle.h"

// ==========================================
#define NODE_ID "sat_a"
#define STORE   "./store_a"
#define TRACKER "a.txt"
int bundle_id_counter = 2000; 
// ==========================================

struct ReassemblyBuffer reassembly[5];

int main() {
    setvbuf(stdout, NULL, _IONBF, 0);
    char next_hop[32];
    Bundle b, ack;
    int local_port = get_port_for_node(NODE_ID);
    for(int i=0; i<5; i++) reassembly[i].active = 0;
    static long long next_search_time = 0;

    int server_fd = create_server(local_port);

    while (1) {
        char buffer[4096] = {0};
        int client = accept_client(server_fd);
        
        // FIX: CPU Guard
        if (client < 0) { safe_sleep_ms(100); continue; }
        
        receive_message(client, buffer);
        close_socket(client);

        if (strlen(buffer) < 15) continue;
        
        deserialize_bundle(&b, buffer);
        
        // FIX: Reject Dummy Ping Logs
        if (b.primary.version != 7) continue;

        bp_set_state(&b, STATE_RECEIVE);
        
        if (bp_is_expired(&b)) {
            char retr_payload[256], rev_route[256];
            snprintf(retr_payload, sizeof(retr_payload), "RETRANSMIT:%d", b.bundle_id);
            extract_reverse_path_to_source(b.ext_block.explicit_route, NODE_ID, rev_route);
            char b_src[32]; bp_get_source(&b, b_src);
            bp_create_bundle(&ack, bundle_id_counter++, NODE_ID, b_src, NODE_ID, NODE_ID, 3600, retr_payload, 0, 0, strlen(retr_payload), 0, rev_route, b.ext_block.link_bps, get_global_time());
            ack.ext_block.total_queue_time = b.ext_block.total_queue_time; ack.ext_block.node_count = b.ext_block.node_count;
            bp_store_bundle(&ack, STORE);
            
            if (parse_next_hop(rev_route, NODE_ID, next_hop) == 0) {
                Visibility vis; long long search_time = (get_global_time() > next_search_time) ? get_global_time() : next_search_time;
                if (load_visibility(NODE_ID, next_hop, search_time, &vis, ack.ext_block.link_bps) == 0) {
                    long long q_start = get_global_time(); wait_until_visibility(&vis, &ack);
                    long long q_time = get_global_time() - q_start;
                    ack.ext_block.total_queue_time += q_time; ack.ext_block.node_count += 1;
                    bp_forward_bundle(&ack, "127.0.0.1", get_port_for_node(next_hop), next_hop);
                    bp_delete_bundle(ack.bundle_id, STORE);
                    next_search_time = get_global_time();
                }
            }
            continue; 
        }
        
        char b_dest[32]; bp_get_dest(&b, b_dest);
        if (bp_custody_transfer(&b, NODE_ID, STORE) != 0) continue;
        write_bundle_tracker(&b, TRACKER);

        if (strcmp(b_dest, NODE_ID) == 0) {
            bp_deliver_bundle(&b, NODE_ID, STORE);
            
            if (b.primary.flags & BP_FLAG_IS_FRAGMENT) {
                int r_idx = -1;
                for(int i=0; i<5; i++) { if (reassembly[i].active && reassembly[i].creation_ts == b.primary.creation_timestamp) { r_idx = i; break; } }
                if (r_idx == -1) {
                    for(int i=0; i<5; i++) {
                        if (!reassembly[i].active) {
                            r_idx = i; reassembly[i].active = 1; reassembly[i].creation_ts = b.primary.creation_timestamp;
                            reassembly[i].total_len = b.payload_block.total_adu_length; reassembly[i].recv_len = 0;
                            memset(reassembly[i].full_payload, 0, sizeof(reassembly[i].full_payload));
                            reassembly[i].queue_time_sum = 0; reassembly[i].node_count_max = 0; break;
                        }
                    }
                }

                if (r_idx != -1) {
                    int frag_end = b.payload_block.fragment_offset + b.payload_block.fragment_length;
                    if (frag_end <= sizeof(reassembly[r_idx].full_payload)) {
                        memcpy(reassembly[r_idx].full_payload + b.payload_block.fragment_offset, b.payload_block.payload, b.payload_block.fragment_length);
                        reassembly[r_idx].recv_len += b.payload_block.fragment_length;
                        reassembly[r_idx].queue_time_sum += b.ext_block.total_queue_time;
                        if (b.ext_block.node_count > reassembly[r_idx].node_count_max) reassembly[r_idx].node_count_max = b.ext_block.node_count;
                    }

                    if (reassembly[r_idx].recv_len >= reassembly[r_idx].total_len) {
                        char b_src[32]; bp_get_source(&b, b_src);

                        Bundle tracker_b = b;
                        int max_len = sizeof(tracker_b.payload_block.payload) - 1;
                        strncpy(tracker_b.payload_block.payload, reassembly[r_idx].full_payload, max_len);
                        tracker_b.payload_block.payload[max_len] = '\0';
                        tracker_b.payload_block.fragment_length = reassembly[r_idx].total_len;
                        tracker_b.primary.flags = 0x00; 
                        write_bundle_tracker(&tracker_b, TRACKER);

                        char ack_payload[256], rev_route[256];
                        snprintf(ack_payload, sizeof(ack_payload), "ACK:bundle delivered to %s", NODE_ID);
                        reverse_route(b.ext_block.explicit_route, rev_route);
                        
                        bp_create_bundle(&ack, bundle_id_counter++, NODE_ID, b_src, NODE_ID, NODE_ID, 3600, ack_payload, strlen(ack_payload), 0, strlen(ack_payload), 0, rev_route, b.ext_block.link_bps, get_global_time());
                        ack.ext_block.total_queue_time = reassembly[r_idx].queue_time_sum / (reassembly[r_idx].total_len / MTU_SIZE + 1);
                        ack.ext_block.node_count = reassembly[r_idx].node_count_max;
                        bp_store_bundle(&ack, STORE);
                        
                        if (parse_next_hop(rev_route, NODE_ID, next_hop) == 0) {
                            Visibility vis; long long search_time = (get_global_time() > next_search_time) ? get_global_time() : next_search_time;
                            if (load_visibility(NODE_ID, next_hop, search_time, &vis, ack.ext_block.link_bps) == 0) {
                                long long q_start = get_global_time(); wait_until_visibility(&vis, &ack);
                                long long q_time = get_global_time() - q_start;
                                ack.ext_block.total_queue_time += q_time; ack.ext_block.node_count += 1;
                                bp_forward_bundle(&ack, "127.0.0.1", get_port_for_node(next_hop), next_hop);
                                bp_delete_bundle(ack.bundle_id, STORE);
                                next_search_time = get_global_time(); 
                            }
                        }
                        reassembly[r_idx].active = 0; 
                    }
                }
            } else {
                char ack_payload[256], rev_route[256], b_src[32];
                bp_get_source(&b, b_src);
                snprintf(ack_payload, sizeof(ack_payload), "ACK:bundle delivered to %s", NODE_ID);
                reverse_route(b.ext_block.explicit_route, rev_route);
                bp_create_bundle(&ack, bundle_id_counter++, NODE_ID, b_src, NODE_ID, NODE_ID, 3600, ack_payload, strlen(ack_payload), 0, strlen(ack_payload), 0, rev_route, b.ext_block.link_bps, get_global_time());
                ack.ext_block.total_queue_time = b.ext_block.total_queue_time; ack.ext_block.node_count = b.ext_block.node_count;
                bp_store_bundle(&ack, STORE);
                
                if (parse_next_hop(rev_route, NODE_ID, next_hop) == 0) {
                    Visibility vis; long long search_time = (get_global_time() > next_search_time) ? get_global_time() : next_search_time;
                    if (load_visibility(NODE_ID, next_hop, search_time, &vis, ack.ext_block.link_bps) == 0) {
                        long long q_start = get_global_time(); wait_until_visibility(&vis, &ack);
                        long long q_time = get_global_time() - q_start;
                        ack.ext_block.total_queue_time += q_time; ack.ext_block.node_count += 1;
                        bp_forward_bundle(&ack, "127.0.0.1", get_port_for_node(next_hop), next_hop);
                        bp_delete_bundle(ack.bundle_id, STORE);
                        next_search_time = get_global_time(); 
                    }
                }
            }
        } else {
            if (parse_next_hop(b.ext_block.explicit_route, NODE_ID, next_hop) == 0) {
                Visibility vis; long long search_time = (get_global_time() > next_search_time) ? get_global_time() : next_search_time;
                if (load_visibility(NODE_ID, next_hop, search_time, &vis, b.ext_block.link_bps) == 0) {
                    long long q_start = get_global_time(); wait_until_visibility(&vis, &b);
                    long long q_time = get_global_time() - q_start;
                    b.ext_block.total_queue_time += q_time; b.ext_block.node_count += 1;
                    bp_forward_bundle(&b, "127.0.0.1", get_port_for_node(next_hop), next_hop);
                    bp_delete_bundle(b.bundle_id, STORE);
                    next_search_time = get_global_time(); 
                }
            }
        }
    }
    return 0;
}