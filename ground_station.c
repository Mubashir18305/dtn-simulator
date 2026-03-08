#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include "socket_util.h"
#include "bundle.h"

#define NODE_ID "gs"
#define STORE   "./store_gs"
#define TRACKER "gs.txt"

struct ReassemblyBuffer reassembly[5];

int main() {
    setvbuf(stdout, NULL, _IONBF, 0);
    char dest[32], user_route[200], route[256], payload_input[2048], next_hop[32];
    int bundle_id_counter = 1; 
    int local_port = get_port_for_node(NODE_ID);
    int server_fd = create_server(local_port);
    static long long next_search_time = 0; 
    
    for(int i=0; i<5; i++) reassembly[i].active = 0;

    while (1) {
        long long cycle_start_time = 0;
        
        if (scanf(" %31[^\n]", dest) <= 0) break;
        scanf(" %199[^\n]", user_route);
        int total_size; scanf("%d", &total_size);
        scanf(" %2047[^\n]", payload_input);
        int link_bps; scanf("%d", &link_bps);
        char start_time_str[32]; scanf(" %31[^\n]", start_time_str);

        snprintf(route, sizeof(route), "gs,%s", user_route);
        
        size_t current_len = strlen(payload_input);
        size_t t_size = (size_t)total_size;
        if (t_size >= sizeof(payload_input)) { t_size = sizeof(payload_input) - 1; }
        if (current_len < t_size) {
            for(size_t i = current_len; i < t_size; i++) { payload_input[i] = '.'; }
        }
        payload_input[t_size] = '\0';
        
        long long req_start_ms = parse_vis_time(start_time_str);
        cycle_start_time = get_global_time(); 
        long long shared_creation_ts = get_global_time();

        int offset = 0, remaining = total_size, frag_count = 0;
        Bundle frags[20];

        while(remaining > 0 && frag_count < 20) {
            int chunk = (remaining > MTU_SIZE) ? MTU_SIZE : remaining;
            char chunk_data[256];
            strncpy(chunk_data, payload_input + offset, chunk);
            chunk_data[chunk] = '\0';
            
            int is_frag = (total_size > MTU_SIZE) ? 1 : 0;
            bp_create_bundle(&frags[frag_count], bundle_id_counter++, NODE_ID, dest, NODE_ID, NODE_ID, 
                             3600, chunk_data, chunk, offset, total_size, is_frag, route, link_bps, shared_creation_ts);
            
            bp_store_bundle(&frags[frag_count], STORE);
            write_bundle_tracker(&frags[frag_count], TRACKER);
            offset += chunk; remaining -= chunk; frag_count++;
        }

        if (parse_next_hop(route, NODE_ID, next_hop) == 0) {
            long long search_time = req_start_ms; 
            for(int i = 0; i < frag_count; i++) {
                Visibility vis;
                long long current_t = get_global_time();
                long long eval_time = (current_t > search_time) ? current_t : search_time;

                if (load_visibility(NODE_ID, next_hop, eval_time, &vis, link_bps) == 0) {
                    long long q_start = get_global_time();
                    wait_until_visibility(&vis, &frags[i]);
                    long long q_time = get_global_time() - q_start;

                    frags[i].ext_block.total_queue_time += q_time;
                    frags[i].ext_block.node_count += 1;

                    bp_forward_bundle(&frags[i], "127.0.0.1", get_port_for_node(next_hop), next_hop);
                    bp_delete_bundle(frags[i].bundle_id, STORE);
                    
                    search_time = get_global_time(); 
                }
            }
        }

        int cycle_active = 1;
        while (cycle_active) {
            char buffer[4096] = {0};
            int client = accept_client(server_fd);
            
            // FIX: Prevents infinite CPU spinning if port breaks
            if (client < 0) { safe_sleep_ms(100); continue; } 
            
            receive_message(client, buffer);
            close_socket(client);

            if (strlen(buffer) < 15) continue;

            Bundle b;
            deserialize_bundle(&b, buffer);
            if (b.primary.version != 7) continue;

            bp_set_state(&b, STATE_RECEIVE);
            
            char b_dest[32]; bp_get_dest(&b, b_dest);
            if (bp_custody_transfer(&b, NODE_ID, STORE) != 0) continue;

            if (strcmp(b_dest, NODE_ID) == 0) {
                bp_deliver_bundle(&b, NODE_ID, STORE);
                write_bundle_tracker(&b, TRACKER);
                
                if ((strncmp(b.payload_block.payload, "ACK", 3) == 0 || 
                     strncmp(b.payload_block.payload, "RETRANSMIT", 10) == 0) && cycle_start_time > 0) {
                    
                    double tat_sec = (get_global_time() - cycle_start_time) / 1000.0;
                    double total_q_sec = b.ext_block.total_queue_time / 1000.0;
                    double avg_q_sec = 0;
                    if (b.ext_block.node_count > 0) avg_q_sec = total_q_sec / b.ext_block.node_count;
                    
                    printf("\n=======================================================\n");
                    printf("                *** CYCLE COMPLETE *** \n");
                    printf("=======================================================\n");
                    printf("   Total Turn Around Time (TTAT) : %.3f s\n", tat_sec);
                    printf("   Total Queue Time              : %.3f s\n", total_q_sec);
                    printf("   Total Nodes Queued            : %d\n", b.ext_block.node_count);
                    printf("   Avg Queue Time                : %.3f s\n", avg_q_sec);
                    printf("=======================================================\n\n");
                    cycle_active = 0; 
                }
            } else {
                if (parse_next_hop(b.ext_block.explicit_route, NODE_ID, next_hop) == 0) {
                    Visibility vis;
                    long long search_time = (get_global_time() > next_search_time) ? get_global_time() : next_search_time;
                    if (load_visibility(NODE_ID, next_hop, search_time, &vis, b.ext_block.link_bps) == 0) {
                        long long q_start = get_global_time();
                        wait_until_visibility(&vis, &b);
                        long long q_time = get_global_time() - q_start;
                        
                        b.ext_block.total_queue_time += q_time; b.ext_block.node_count += 1;
                        bp_forward_bundle(&b, "127.0.0.1", get_port_for_node(next_hop), next_hop);
                        bp_delete_bundle(b.bundle_id, STORE);
                        next_search_time = get_global_time();
                    }
                }
            }
        }
    }
    return 0;
}