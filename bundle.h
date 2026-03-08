#ifndef BUNDLE_H
#define BUNDLE_H

#include <stddef.h>

#define BP_FLAG_IS_FRAGMENT 0x01
#define MTU_SIZE 50

typedef enum { STATE_WAIT, STATE_CREATE, STATE_PROCESS, STATE_SEND, STATE_RECEIVE } BundleState;

typedef struct {
    int version; unsigned char flags; int block_length;
    int dest_scheme_offset; int dest_ssp_offset;
    int src_scheme_offset; int src_ssp_offset;
    int report_to_scheme_offset; int report_to_ssp_offset;
    int custodian_scheme_offset; int custodian_ssp_offset;
    long long creation_timestamp; int lifetime;
    int dictionary_length; char dictionary[512];
} PrimaryBlock;

typedef struct {
    int block_type; int control_flags; int block_length;
    char explicit_route[256];
    long long total_queue_time; int node_count; int link_bps;
} ExtensionBlock;

typedef struct {
    int block_type; int control_flags;
    int fragment_offset; int total_adu_length; int fragment_length;
    char payload[1024];
} PayloadBlock;

typedef struct {
    int bundle_id; char current_node[32]; BundleState state;
    PrimaryBlock primary; ExtensionBlock ext_block; PayloadBlock payload_block;
} Bundle;

struct ReassemblyBuffer {
    int active; long long creation_ts; int total_len; int recv_len;
    char full_payload[4096]; long long queue_time_sum; int node_count_max;
};

typedef struct {
    char from_node[32]; char to_node[32];
    long long fov_time; long long los_time; long long aos_time; long long end_time;
    int bandwidth_bps;
} Visibility;

void safe_sleep_ms(long long ms); // <--- FIX: Added missing declaration here
long long get_global_time(void);
void format_time(long long t_ms, char *buf, size_t max_size);
long long parse_vis_time(const char *time_str);
int get_port_for_node(const char *node_id);
void bp_set_state(Bundle *b, BundleState s);
int bp_is_expired(const Bundle *b);
void bp_get_source(const Bundle *b, char *out);
void bp_get_dest(const Bundle *b, char *out);
void bp_get_custodian(const Bundle *b, char *out);
int parse_next_hop(const char *route, const char *current_node, char *next_hop);
void reverse_route(const char *forward_route, char *reversed_route);
void extract_reverse_path_to_source(const char *forward_route, const char *current_node, char *rev_route);
int load_visibility(const char *from, const char *to, long long current_time, Visibility *vis, int bps);
void wait_until_visibility(Visibility *v, Bundle *b);
void serialize_bundle(const Bundle *b, char *buffer);
void deserialize_bundle(Bundle *b, char *buffer);
int bp_get_bdu_size(const Bundle *b);
void print_hop_metrics(const Bundle *b, const Visibility *vis, long long q_time);

void bp_create_bundle(Bundle *b, int id, const char *src, const char *dest, const char *custodian, const char *current_node, int lifetime_sec, const char *data, int frag_length, int frag_offset, int total_adu_len, int is_fragment, const char *route, int link_bps, long long creation_ts);
int bp_store_bundle(const Bundle *b, const char *storage_dir);
void bp_delete_bundle(int bundle_id, const char *storage_dir);
int bp_custody_transfer(Bundle *b, const char *new_custodian, const char *storage_dir);
int bp_forward_bundle(Bundle *b, const char *next_hop_ip, int next_hop_port, const char *next_node_id);
int bp_deliver_bundle(Bundle *b, const char *local_node_id, const char *storage_dir);
void write_bundle_tracker(const Bundle *b, const char *filename);

#endif