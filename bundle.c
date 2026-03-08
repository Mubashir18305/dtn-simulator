#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <sys/time.h>
#include <sys/stat.h>
#include <unistd.h>
#include "bundle.h"
#include "socket_util.h"

void safe_sleep_ms(long long ms) {
    if (ms <= 0) return;
    struct timespec req;
    req.tv_sec = ms / 1000;
    req.tv_nsec = (ms % 1000) * 1000000;
    nanosleep(&req, NULL);
}

long long get_global_time(void) {
    struct timeval tv;
    gettimeofday(&tv, NULL);
    time_t now = tv.tv_sec;
    struct tm *t = localtime(&now);
    return (t->tm_hour * 3600000LL) + (t->tm_min * 60000LL) + (t->tm_sec * 1000LL) + (tv.tv_usec / 1000);
}

void format_time(long long t_ms, char *buf, size_t max_size) {
    long long abs_ms = (t_ms < 0) ? 0 : t_ms;
    int h = (abs_ms / 3600000) % 24;
    int m = (abs_ms / 60000) % 60;
    int s = (abs_ms / 1000) % 60;
    int ms = abs_ms % 1000;
    snprintf(buf, max_size, "%02d:%02d:%02d:%03d", h, m, s, ms);
}

long long parse_vis_time(const char *time_str) {
    int h = 0, m = 0, s = 0, xx = 0;
    sscanf(time_str, "%d:%d:%d:%d", &h, &m, &s, &xx);
    return (h * 3600000LL) + (m * 60000LL) + (s * 1000LL) + (xx * 10LL);
}

int get_port_for_node(const char *node_id) {
    if (strcmp(node_id, "sat_a") == 0) return 5001;
    if (strcmp(node_id, "sat_b") == 0) return 5002;
    if (strcmp(node_id, "sat_c") == 0) return 5003;
    if (strcmp(node_id, "gs") == 0 || strcmp(node_id, "ground_station") == 0) return 6000;
    return -1;
}

void bp_set_state(Bundle *b, BundleState s) {
    b->state = s;
    safe_sleep_ms(500); 
}

int bp_is_expired(const Bundle *b) { return 0; }
void bp_get_source(const Bundle *b, char *out) { strcpy(out, b->primary.dictionary + b->primary.src_ssp_offset); }
void bp_get_dest(const Bundle *b, char *out) { strcpy(out, b->primary.dictionary + b->primary.dest_ssp_offset); }
void bp_get_custodian(const Bundle *b, char *out) { strcpy(out, b->primary.dictionary + b->primary.custodian_ssp_offset); }

static int add_to_dict(PrimaryBlock *p, const char *str) {
    int len = strlen(str) + 1;
    if (p->dictionary_length + len > 512) return p->dictionary_length; 
    int offset = p->dictionary_length;
    strcpy(p->dictionary + offset, str);
    p->dictionary_length += len;
    return offset;
}

int parse_next_hop(const char *route, const char *current_node, char *next_hop) {
    char r_copy[512]; 
    strncpy(r_copy, route, 511); r_copy[511] = '\0';
    char *token = strtok(r_copy, ",");
    while (token != NULL) {
        if (strcmp(token, current_node) == 0) {
            token = strtok(NULL, ",");
            if (token != NULL) { strcpy(next_hop, token); return 0; }
            return -1; 
        }
        token = strtok(NULL, ",");
    }
    return -1; 
}

void reverse_route(const char *forward_route, char *reversed_route) {
    char r_copy[512]; char *nodes[64]; int count = 0;
    strncpy(r_copy, forward_route, 511); r_copy[511] = '\0';
    char *token = strtok(r_copy, ",");
    while (token != NULL && count < 64) { nodes[count++] = token; token = strtok(NULL, ","); }
    reversed_route[0] = '\0';
    for (int i = count - 1; i >= 0; i--) {
        strcat(reversed_route, nodes[i]);
        if (i > 0) strcat(reversed_route, ",");
    }
}

void extract_reverse_path_to_source(const char *forward_route, const char *current_node, char *rev_route) {
    char r_copy[512]; char *nodes[64]; int count = 0;
    strncpy(r_copy, forward_route, 511); r_copy[511] = '\0';
    char *token = strtok(r_copy, ",");
    while (token != NULL && count < 64) {
        nodes[count++] = token;
        if (strcmp(token, current_node) == 0) break;
        token = strtok(NULL, ",");
    }
    rev_route[0] = '\0';
    for (int i = count - 1; i >= 0; i--) {
        strcat(rev_route, nodes[i]);
        if (i > 0) strcat(rev_route, ",");
    }
}

int load_visibility(const char *from, const char *to, long long current_time, Visibility *vis, int bps) {
    FILE *f = fopen("visibility.txt", "r");
    char f_node[32], t_node[32], start_str[32], end_str[32];
    long long best_start = -1, best_end = -1;
    int found = 0;

    if (f) {
        char line[256]; fgets(line, sizeof(line), f);
        while (fgets(line, sizeof(line), f)) {
            if (sscanf(line, "%31s %31s %31s %31s", f_node, t_node, start_str, end_str) >= 4) {
                if (strcmp(from, f_node) == 0 && strcmp(to, t_node) == 0) {
                    long long start_t = parse_vis_time(start_str);
                    long long end_t = parse_vis_time(end_str);
                    if (end_t > current_time) {
                        if (!found || start_t < best_start) {
                            best_start = start_t; best_end = end_t; found = 1;
                        }
                    }
                }
            }
        }
        fclose(f);
    }
    
    if (!found) {
        best_start = current_time + 2000;
        best_end = best_start + 60000;
    }
    
    strncpy(vis->from_node, from, 31);
    strncpy(vis->to_node, to, 31);
    vis->aos_time = best_start;
    vis->los_time = vis->aos_time - 2000; 
    vis->fov_time = vis->aos_time - 4000; 
    vis->end_time = best_end;
    vis->bandwidth_bps = bps;
    return 0;
}

void wait_until_visibility(Visibility *v, Bundle *b) {
    char ts[32], aos_ts[32], end_ts[32]; 
    long long now = get_global_time();
    
    format_time(v->aos_time, aos_ts, sizeof(aos_ts));
    format_time(v->end_time, end_ts, sizeof(end_ts));
    
    if (now < v->aos_time) {
        bp_set_state(b, STATE_WAIT);
        double wait_sec = (v->aos_time - now) / 1000.0;
        printf("   [WAITING] %s -> %s | window in visibility.txt: [%s to %s] | Wait: %.3f sec\n", 
               v->from_node, v->to_node, aos_ts, end_ts, wait_sec);
    } else if (now <= v->end_time) {
        printf("   [ACTIVE] %s -> %s window is currently OPEN! [%s to %s]\n", v->from_node, v->to_node, aos_ts, end_ts);
        return; 
    }

    now = get_global_time();
    if (now < v->fov_time) safe_sleep_ms(v->fov_time - now);
    if (get_global_time() >= v->fov_time && now < v->fov_time + 1000) {
        format_time(get_global_time(), ts, 32);
        printf("[%s] > FOV Achieved (Target in sensor range)\n", ts);
    }

    now = get_global_time();
    if (now < v->los_time) safe_sleep_ms(v->los_time - now);
    if (get_global_time() >= v->los_time && now < v->los_time + 1000) {
        format_time(get_global_time(), ts, 32);
        printf("[%s] > LOS Confirmed (Obstruction cleared)\n", ts);
    }

    now = get_global_time();
    if (now < v->aos_time) safe_sleep_ms(v->aos_time - now);
    if (get_global_time() >= v->aos_time) {
        format_time(get_global_time(), ts, 32);
        printf("[%s] > AOS Established! Window OPEN.\n", ts);
    }
}

void serialize_bundle(const Bundle *b, char *buffer) {
    char dict_encoded[512];
    memcpy(dict_encoded, b->primary.dictionary, b->primary.dictionary_length);
    for(int i = 0; i < b->primary.dictionary_length; i++) {
        if(dict_encoded[i] == '\0') dict_encoded[i] = '#';
    }
    dict_encoded[b->primary.dictionary_length] = '\0';
    
    snprintf(buffer, 4096, "%d|%s|%d|%d|%d|%d|%d|%d|%d|%d|%d|%d|%d|%d|%lld|%d|%d|%s|%d|%d|%d|%d|%d|%s|%d|%d|%d|%s|%lld|%d|%d",
        b->bundle_id, b->current_node, (int)b->state, b->primary.version, b->primary.flags, b->primary.block_length,
        b->primary.dest_scheme_offset, b->primary.dest_ssp_offset, b->primary.src_scheme_offset, b->primary.src_ssp_offset,
        b->primary.report_to_scheme_offset, b->primary.report_to_ssp_offset, b->primary.custodian_scheme_offset, b->primary.custodian_ssp_offset,
        b->primary.creation_timestamp, b->primary.lifetime, b->primary.dictionary_length, dict_encoded,
        b->payload_block.block_type, b->payload_block.control_flags, b->payload_block.fragment_offset, b->payload_block.total_adu_length, b->payload_block.fragment_length,
        b->payload_block.payload, b->ext_block.block_type, b->ext_block.control_flags, b->ext_block.block_length, b->ext_block.explicit_route,
        b->ext_block.total_queue_time, b->ext_block.node_count, b->ext_block.link_bps);
}

void deserialize_bundle(Bundle *b, char *buffer) {
    char dict_encoded[512]; long long ct, t_qt; int st, nc, bps, p_flags = 0; 
    memset(b, 0, sizeof(Bundle));
    
    sscanf(buffer, "%d|%31[^|]|%d|%d|%d|%d|%d|%d|%d|%d|%d|%d|%d|%d|%lld|%d|%d|%511[^|]|%d|%d|%d|%d|%d|%1023[^|]|%d|%d|%d|%255[^|]|%lld|%d|%d",
        &b->bundle_id, b->current_node, &st, &b->primary.version, &p_flags, &b->primary.block_length,
        &b->primary.dest_scheme_offset, &b->primary.dest_ssp_offset, &b->primary.src_scheme_offset, &b->primary.src_ssp_offset,
        &b->primary.report_to_scheme_offset, &b->primary.report_to_ssp_offset, &b->primary.custodian_scheme_offset, &b->primary.custodian_ssp_offset,
        &ct, &b->primary.lifetime, &b->primary.dictionary_length, dict_encoded,
        &b->payload_block.block_type, &b->payload_block.control_flags, &b->payload_block.fragment_offset, &b->payload_block.total_adu_length, &b->payload_block.fragment_length,
        b->payload_block.payload, &b->ext_block.block_type, &b->ext_block.control_flags, &b->ext_block.block_length, b->ext_block.explicit_route,
        &t_qt, &nc, &bps);
        
    b->primary.flags = (unsigned char)p_flags; 
    b->state = (BundleState)st; 
    b->primary.creation_timestamp = ct;
    b->ext_block.total_queue_time = t_qt; b->ext_block.node_count = nc; b->ext_block.link_bps = bps;
    
    for(int i = 0; i < b->primary.dictionary_length; i++) {
        if(dict_encoded[i] == '#') b->primary.dictionary[i] = '\0'; else b->primary.dictionary[i] = dict_encoded[i];
    }
}

int bp_get_bdu_size(const Bundle *b) {
    char temp[4096]; serialize_bundle(b, temp); return strlen(temp);
}

void print_hop_metrics(const Bundle *b, const Visibility *vis, long long q_time) { }

void bp_create_bundle(Bundle *b, int id, const char *src, const char *dest,
                      const char *custodian, const char *current_node,
                      int lifetime_sec, const char *data, int frag_length, int frag_offset, int total_adu_len,
                      int is_fragment, const char *route, int link_bps, long long creation_ts) {
    memset(b, 0, sizeof(Bundle));
    b->bundle_id = id;
    strncpy(b->current_node, current_node, sizeof(b->current_node) - 1);
    b->primary.version = 7; 
    b->primary.flags = (is_fragment) ? BP_FLAG_IS_FRAGMENT : 0x00; 
    b->primary.dictionary_length = 0;
    
    b->primary.src_scheme_offset = add_to_dict(&b->primary, "dtn");
    b->primary.src_ssp_offset    = add_to_dict(&b->primary, src);
    b->primary.dest_scheme_offset = add_to_dict(&b->primary, "dtn");
    b->primary.dest_ssp_offset    = add_to_dict(&b->primary, dest);
    b->primary.custodian_scheme_offset = add_to_dict(&b->primary, "dtn");
    b->primary.custodian_ssp_offset    = add_to_dict(&b->primary, custodian);
    b->primary.report_to_scheme_offset = add_to_dict(&b->primary, "dtn");
    b->primary.report_to_ssp_offset    = add_to_dict(&b->primary, "none");
    
    b->primary.creation_timestamp = creation_ts;
    b->primary.lifetime = lifetime_sec;
    b->primary.block_length = b->primary.dictionary_length; 
    
    b->payload_block.block_type = 1; b->payload_block.control_flags = 0; 
    b->payload_block.fragment_offset = frag_offset;
    b->payload_block.total_adu_length = total_adu_len;
    b->payload_block.fragment_length = frag_length;
    
    const char *p_data = (data && strlen(data) > 0) ? data : "EMPTY";
    strncpy(b->payload_block.payload, p_data, sizeof(b->payload_block.payload) - 1);
    
    b->ext_block.block_type = 2; b->ext_block.control_flags = 0; b->ext_block.block_length = strlen(route);
    strncpy(b->ext_block.explicit_route, route, sizeof(b->ext_block.explicit_route) - 1);
    b->ext_block.total_queue_time = 0; b->ext_block.node_count = 0; b->ext_block.link_bps = link_bps;
    
    bp_set_state(b, STATE_CREATE);
}

int bp_store_bundle(const Bundle *b, const char *storage_dir) {
    char path[256], buf[4096]; FILE *f;
    mkdir(storage_dir, 0755);
    snprintf(path, sizeof(path), "%s/bundle_%d.dat", storage_dir, b->bundle_id);
    f = fopen(path, "w");
    if (!f) return -1;
    serialize_bundle(b, buf);
    fprintf(f, "%s\n", buf);
    fclose(f);
    return 0;
}

void bp_delete_bundle(int bundle_id, const char *storage_dir) {
    char path[256];
    snprintf(path, sizeof(path), "%s/bundle_%d.dat", storage_dir, bundle_id);
    remove(path);
}

int bp_custody_transfer(Bundle *b, const char *new_custodian, const char *storage_dir) {
    char old_cust[32]; bp_get_custodian(b, old_cust);
    b->primary.custodian_ssp_offset = add_to_dict(&b->primary, new_custodian);
    bp_set_state(b, STATE_PROCESS);
    if (bp_store_bundle(b, storage_dir) != 0) return -1;
    return 0;
}

int bp_forward_bundle(Bundle *b, const char *next_hop_ip, int next_hop_port, const char *next_node_id) {
    char buf[4096], ts[32]; int sock;
    bp_set_state(b, STATE_SEND);
    strncpy(b->current_node, next_node_id, sizeof(b->current_node) - 1);
    serialize_bundle(b, buf);
    
    sock = create_client(next_hop_ip, next_hop_port);
    if (sock < 0) return -1;
    send_message(sock, buf);
    close_socket(sock);
    
    format_time(get_global_time(), ts, sizeof(ts));
    printf("[%s] > Data beamed to %s (%s:%d)\n", ts, next_node_id, next_hop_ip, next_hop_port);
    return 0;
}

int bp_deliver_bundle(Bundle *b, const char *local_node_id, const char *storage_dir) {
    char dest[32]; bp_get_dest(b, dest); 
    if (strcmp(dest, local_node_id) != 0) return -1;
    bp_set_state(b, STATE_RECEIVE);
    bp_delete_bundle(b->bundle_id, storage_dir);
    return 0;
}

// FIX: ULTIMATE SECURITY LOCK AGAINST DUMMY LOGS!
void write_bundle_tracker(const Bundle *b, const char *filename) {
    if (b == NULL || b->primary.version != 7) return; // physically refuses non-DTN data

    FILE *f = fopen(filename, "a");
    if (!f) return;

    char dest[32], src[32], cust[32], report[32];
    bp_get_dest(b, dest); bp_get_source(b, src);
    bp_get_custodian(b, cust);
    strcpy(report, b->primary.dictionary + b->primary.report_to_ssp_offset);

    char dict_display[2048] = {0}; int pos = 0;
    for(int i = 0; i < b->primary.dictionary_length; i++) {
        if(b->primary.dictionary[i] == '\0') {
            dict_display[pos++] = '\\'; dict_display[pos++] = '0';
        } else {
            dict_display[pos++] = b->primary.dictionary[i];
        }
    }
    dict_display[pos] = '\0';

    char ts_str[32]; format_time(b->primary.creation_timestamp, ts_str, sizeof(ts_str));

    fprintf(f, "Primary Block:\n  Version: %d\n  Flags: 0x%02X\n  Dest: dtn:%s\n  Src: dtn:%s\n  Report-to: dtn:%s\n  Custodian: dtn:%s\n  Creation TS: %s\n  Lifetime: %d\n  Dictionary: \"dtn\\0%s\"\n\n", 
            b->primary.version, b->primary.flags, dest, src, report, cust, ts_str, b->primary.lifetime, dict_display);
    fprintf(f, "Extension Block:\n  Block Type: 0x%02X\n  Flags: 0x%02X\n  Hop Used: %d\n  Path: [%s]\n\n", 
            b->ext_block.block_type, b->ext_block.control_flags, b->ext_block.node_count, b->ext_block.explicit_route);
    fprintf(f, "Payload Block:\n  Block Type: %d\n  Flags: 0x%02X\n  Total ADU: %d\n  Frag Offset: %d\n  Frag Len: %d\n  Payload: \"%s\"\n\n*** End Record ***\n\n", 
            b->payload_block.block_type, b->payload_block.control_flags, b->payload_block.total_adu_length, b->payload_block.fragment_offset, b->payload_block.fragment_length, b->payload_block.payload);
    fclose(f);
}