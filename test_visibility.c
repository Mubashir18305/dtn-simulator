#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include "bundle.h"

void create_test_file(const char *content) {
    FILE *f = fopen("visibility.txt", "w");
    if (f) {
        fputs(content, f);
        fclose(f);
    }
}

void remove_test_file() {
    remove("visibility.txt");
}

void test_file_not_found() {
    remove_test_file();
    Visibility vis;
    long long current_time = 10000;
    int ret = load_visibility("nodeA", "nodeB", current_time, &vis, 500);

    assert(ret == 0);
    assert(strcmp(vis.from_node, "nodeA") == 0);
    assert(strcmp(vis.to_node, "nodeB") == 0);
    assert(vis.aos_time == current_time + 2000);
    assert(vis.end_time == current_time + 62000); // aos_time + 60000
    assert(vis.bandwidth_bps == 500);
    printf("test_file_not_found passed.\n");
}

void test_happy_path() {
    create_test_file("Header line\n"
                     "nodeA nodeB 01:00:00:00 02:00:00:00 100.0\n");
    Visibility vis;
    long long current_time = 0;
    int ret = load_visibility("nodeA", "nodeB", current_time, &vis, 1000);

    assert(ret == 0);
    long long expected_start = 3600000LL; // 1 hour in ms
    long long expected_end = 2 * 3600000LL; // 2 hours in ms
    assert(vis.aos_time == expected_start);
    assert(vis.end_time == expected_end);
    assert(vis.los_time == expected_start - 2000);
    assert(vis.fov_time == expected_start - 4000);
    printf("test_happy_path passed.\n");
}

void test_multiple_matches_chooses_earliest_future() {
    create_test_file("Header line\n"
                     "nodeA nodeB 03:00:00:00 04:00:00:00 100.0\n"
                     "nodeA nodeB 01:00:00:00 02:00:00:00 100.0\n"
                     "nodeA nodeB 02:00:00:00 03:00:00:00 100.0\n");
    Visibility vis;
    long long current_time = 1.5 * 3600000LL;
    int ret = load_visibility("nodeA", "nodeB", current_time, &vis, 1000);

    assert(ret == 0);
    long long expected_start = 3600000LL;
    assert(vis.aos_time == expected_start);
    printf("test_multiple_matches_chooses_earliest_future passed.\n");
}

void test_no_matches_fallback() {
    create_test_file("Header line\n"
                     "nodeA nodeC 01:00:00:00 02:00:00:00 100.0\n");
    Visibility vis;
    long long current_time = 5000;
    int ret = load_visibility("nodeA", "nodeB", current_time, &vis, 1000);

    assert(ret == 0);
    assert(vis.aos_time == current_time + 2000);
    printf("test_no_matches_fallback passed.\n");
}

void test_malformed_lines_skipped() {
    create_test_file("Header line\n"
                     "nodeA nodeB 01:00:00:00\n" // Malformed: missing end time
                     "nodeA nodeB 02:00:00:00 03:00:00:00 100.0\n");
    Visibility vis;
    long long current_time = 0;
    int ret = load_visibility("nodeA", "nodeB", current_time, &vis, 1000);

    assert(ret == 0);
    long long expected_start = 2 * 3600000LL;
    assert(vis.aos_time == expected_start);
    printf("test_malformed_lines_skipped passed.\n");
}

int main() {
    // Backup existing visibility.txt using rename (POSIX/C standard)
    int backup_exists = (rename("visibility.txt", "visibility_backup.txt") == 0);

    printf("Running load_visibility tests...\n");

    test_file_not_found();
    test_happy_path();
    test_multiple_matches_chooses_earliest_future();
    test_no_matches_fallback();
    test_malformed_lines_skipped();

    printf("All tests passed!\n");

    // Clean up test file
    remove("visibility.txt");

    // Restore backup if it existed
    if (backup_exists) {
        rename("visibility_backup.txt", "visibility.txt");
    }
    return 0;
}
