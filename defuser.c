#include "defuser.h"

#include <inttypes.h>
#include <stdio.h>

#define MAX_FIELDS 8

/*
 * 把 values 中的 count 个整数写成一行“空格分隔的无符号十进制整数”，存入 out。
 * 成功返回 0；count 为 0（还没有实现）、snprintf 出错或结果被截断时返回非零。
 */
static int write_answer(char *out, size_t cap, const uint32_t *values, size_t count) {
    size_t used = 0;
    for (size_t i = 0; i < count; ++i) {
        int size = snprintf(out + used, cap - used, "%s%" PRIu32, i ? " " : "", values[i]);
        if (size < 0 || (size_t)size >= cap - used) return 1;
        used += (size_t)size;
    }
    return count == 0;
}

int solve_phase_1(uint64_t challenge, char *out, size_t cap) {
    uint32_t values[MAX_FIELDS] = {0};
    size_t count = 1;

    uint32_t low = (uint32_t)challenge;
    uint32_t high = (uint32_t)(challenge >> 32);
    uint32_t middle = (uint32_t)(challenge >> 16);

    uint32_t t1 = (middle + 0x651a) ^ high;
    uint32_t t0 = (5 * low) ^ (high + 0x6d95);

    values[0] = 5 * (t0 - t1) + 0xf59a;

    return write_answer(out, cap, values, count);
}

int solve_phase_2(uint64_t challenge, char *out, size_t cap) {
    uint32_t values[MAX_FIELDS] = {0};
    size_t count = 6;

    values[0] = (uint32_t)((challenge >> 41) & 0x3f) + 2;

    for (uint32_t i = 1; i < 6; i++) {
        uint32_t part = (uint32_t)((challenge >> (i + 2)) & 0xf);
        part += i;
        part ^= values[i - 1];
        values[i] = part * 5;
    }

    return write_answer(out, cap, values, count);
}

int solve_phase_3(uint64_t challenge, char *out, size_t cap) {
    uint32_t values[MAX_FIELDS] = {0};
    size_t count = 2;

    uint32_t index = (uint32_t)(challenge & 0x7);
    uint32_t x = (uint32_t)(challenge >> 32) ^ (uint32_t)(challenge >> 3);
    uint32_t result = 0;

    switch (index) {
        case 0:  // p3_case_0
            result = (x ^ 0x1d03) + (x >> 2);
            break;
        case 1:  // p3_case_4
            result = x * 5 + 0x15c57;
            break;
        case 2:  // p3_case_6
            result = (x * 4) ^ 0x2446;
            break;
        case 3:  // p3_case_7
            result = x * 3 + 0x5d78;
            break;
        case 4:  // p3_case_3
            result = x * 5 + 0x2b1bf;
            break;
        case 5:  // p3_case_1
            result = (x * 4) ^ 0x38f6;
            break;
        case 6:  // p3_case_5
            result = (x ^ 0xe0fb) + (x >> 2);
            break;
        case 7:  // p3_case_2
            result = x * 5 + 0x82b8;
            break;
    }

    values[0] = index;
    values[1] = result;

    return write_answer(out, cap, values, count);
}

static uint32_t p4_walk(uint32_t target, uint32_t low, uint32_t high, uint32_t salt) {
    uint32_t mid = low + ((high - low) >> 1);

    if (target == mid) {
        return (mid + 0x9c) ^ salt;
    }

    uint32_t result;

    if (target < mid) {
        result = p4_walk(target, low, mid - 1, salt);
        return ((3u * result) ^ (mid + salt)) ^ 0x9;
    }

    result = p4_walk(target, mid + 1, high, salt);
    return ((3u * result) ^ (mid + salt)) ^ 0x1a;
}

int solve_phase_4(uint64_t challenge, char *out, size_t cap) {
    uint32_t values[MAX_FIELDS] = {0};
    size_t count = 1;

    uint32_t target = (uint32_t)(challenge & 0x7f);
    uint32_t salt = (uint32_t)(challenge >> 7) ^ (uint32_t)(challenge >> 32);

    values[0] = p4_walk(target, 0, 127, salt);

    return write_answer(out, cap, values, count);
}

typedef struct {
    uint32_t key;
    uint32_t value;
    int next;
} P5Node;

static const P5Node p5_nodes[] = {
    {0x6880, 0x044e,  4},  // 0: 0x405ba0
    {0x12bd, 0x016e,  2},  // 1: 0x405bb0
    {0x2fd6, 0x087b, -1},  // 2: 0x405bc0
    {0x520b, 0x0a15,  9},  // 3: 0x405bd0
    {0x57ed, 0x002f,  1},  // 4: 0x405be0
    {0xd9d6, 0x0756,  0},  // 5: 0x405bf0
    {0x1931, 0x0b1f,  8},  // 6: 0x405c00
    {0xd2a3, 0x0321,  6},  // 7: 0x405c10
    {0x7ab5, 0x0958,  3},  // 8: 0x405c20
    {0x82de, 0x054d, 11},  // 9: 0x405c30
    {0xbf23, 0x067a,  5},  // 10: 0x405c40
    {0xca1f, 0x0259, 10}   // 11: 0x405c50
};

static const int p5_heads[8] = {
    7, 6, 8, 3, 9, 11, 10, 5
};

int solve_phase_5(uint64_t challenge, char *out, size_t cap) {
    uint32_t values[MAX_FIELDS] = {0};
    size_t count = 2;

    uint32_t index = (uint32_t)(challenge & 7);
    uint32_t steps = (uint32_t)((challenge >> 3) & 3);
    int node_index = p5_heads[index];
    const P5Node *node = &p5_nodes[node_index];
    uint32_t result = (uint32_t)(challenge >> 6) ^ (uint32_t)(challenge >> 32);

    result ^= node->key;
    result ^= 0xec77;
    result += node->value;

    for (uint32_t i = 0; i < steps; i++) {
        node_index = node->next;

        if (node_index < 0 ||
            node_index >= (int)(sizeof(p5_nodes) / sizeof(p5_nodes[0]))) {
            return 1;
        }

        node = &p5_nodes[node_index];

        result ^= node->key;
        result += node->value;
    }

    values[0] = result;
    values[1] = node->value;

    return write_answer(out, cap, values, count);
}

static uint32_t p6_apply(uint32_t index, uint32_t x) {
    switch (index) {
        case 0: // p6_op_1
        case 2:
            return ((x << 1) | (x >> 31)) ^ 0x142a;

        case 1: // p6_op_0
        case 7:
            return ((x << 7) + 0x3ebb) ^ (x * 5);

        case 3: // p6_op_4
            return (x + 0x1719) ^ (x >> 5);

        case 4: // p6_op_3
            return (x ^ 0xde97) + (x << 5);

        case 5: // p6_op_5
            return ((x >> 7) ^ x) + 0xa5d8;

        case 6: // p6_op_2
            return (x >> 5) | (x << 27);

        default:
            return 0;
    }
}

int solve_phase_6(uint64_t challenge, char *out, size_t cap) {
    uint32_t values[MAX_FIELDS] = {0};
    size_t count = 1;

    uint32_t index1 = (uint32_t)(challenge & 7);
    uint32_t index2 = (uint32_t)((challenge >> 3) & 7);
    uint32_t x = (uint32_t)(challenge >> 6) ^ (uint32_t)(challenge >> 32);
    uint32_t result = p6_apply(index1, x);
    result ^= 0xc8a2;
    result = p6_apply(index2, result);
    values[0] = result + 0xad5c;

    return write_answer(out, cap, values, count);
}

int solve_secret(uint64_t challenge, uint64_t context, char *out, size_t cap) {
    uint32_t a = (uint32_t)(context & 3u);
    uint32_t s = (uint32_t)(challenge >> 32) ^ (uint32_t)challenge;
    uint32_t t = (uint32_t)(context >> 32) ^ (uint32_t)context;
    uint32_t x = s ^ t;
    uint32_t mixed = ((7u * x + 0x8e53u) ^ 0x2227u);
    uint32_t b = mixed ^ ((a * 0x8e53u) ^ (uint32_t)(context >> 16));
    uint32_t c = b ^ (t + a + 0x2227u + (s >> 5));

    int size = snprintf(
        out,
        cap,
        "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA"
        "OPEN"
        "%" PRIu32 " %" PRIu32 " %" PRIu32,
        a, b, c
    );

    return size < 0 || (size_t)size >= cap;
}
