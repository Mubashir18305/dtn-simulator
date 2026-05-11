#include <stdio.h>
#include <string.h>
#include <assert.h>
#include "../bundle.h"

// Test runner helper
int tests_run = 0;
int tests_failed = 0;

#define RUN_TEST(test) do { \
    printf("Running %s... ", #test); \
    if (test()) { \
        printf("PASSED\n"); \
    } else { \
        printf("FAILED\n"); \
        tests_failed++; \
    } \
    tests_run++; \
} while (0)

// Test cases
int test_basic_next_hop() {
    char next_hop[32];
    const char *route = "sat_a,sat_b,sat_c";
    int result = parse_next_hop(route, "sat_a", next_hop);
    return (result == 0 && strcmp(next_hop, "sat_b") == 0);
}

int test_middle_next_hop() {
    char next_hop[32];
    const char *route = "sat_a,sat_b,sat_c";
    int result = parse_next_hop(route, "sat_b", next_hop);
    return (result == 0 && strcmp(next_hop, "sat_c") == 0);
}

int test_last_node_no_next_hop() {
    char next_hop[32];
    const char *route = "sat_a,sat_b,sat_c";
    int result = parse_next_hop(route, "sat_c", next_hop);
    return (result == -1);
}

int test_node_not_in_route() {
    char next_hop[32];
    const char *route = "sat_a,sat_b,sat_c";
    int result = parse_next_hop(route, "gs", next_hop);
    return (result == -1);
}

int test_single_node_route() {
    char next_hop[32];
    const char *route = "sat_a";
    int result = parse_next_hop(route, "sat_a", next_hop);
    return (result == -1);
}

int test_empty_route() {
    char next_hop[32];
    const char *route = "";
    int result = parse_next_hop(route, "sat_a", next_hop);
    return (result == -1);
}

int test_similar_node_names() {
    char next_hop[32];
    const char *route = "sat_a,sat_aa,sat_aaa";
    int result = parse_next_hop(route, "sat_a", next_hop);
    return (result == 0 && strcmp(next_hop, "sat_aa") == 0);
}

int test_longer_route() {
    char next_hop[32];
    const char *route = "a,b,c,d,e,f,g,h,i,j";
    int result = parse_next_hop(route, "i", next_hop);
    return (result == 0 && strcmp(next_hop, "j") == 0);
}

int main() {
    printf("--- Testing parse_next_hop ---\n");
    RUN_TEST(test_basic_next_hop);
    RUN_TEST(test_middle_next_hop);
    RUN_TEST(test_last_node_no_next_hop);
    RUN_TEST(test_node_not_in_route);
    RUN_TEST(test_single_node_route);
    RUN_TEST(test_empty_route);
    RUN_TEST(test_similar_node_names);
    RUN_TEST(test_longer_route);

    printf("\nTests run: %d\n", tests_run);
    printf("Tests failed: %d\n", tests_failed);

    return tests_failed > 0;
}
