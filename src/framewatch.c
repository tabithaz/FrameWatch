#include <errno.h>
#include <inttypes.h>
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Fixed 28-byte wire format; every integer is big-endian. */
enum { FRAME_SIZE = 28, PAYLOAD_SIZE = 24 };

static uint16_t u16be(const unsigned char *p) {
    return (uint16_t)((uint16_t)p[0] << 8 | p[1]);
}

static uint32_t u32be(const unsigned char *p) {
    return (uint32_t)p[0] << 24 | (uint32_t)p[1] << 16 |
           (uint32_t)p[2] << 8 | p[3];
}

static uint64_t u64be(const unsigned char *p) {
    return (uint64_t)u32be(p) << 32 | u32be(p + 4);
}

static uint32_t crc32(const unsigned char *data, size_t length) {
    uint32_t crc = UINT32_MAX;
    for (size_t i = 0; i < length; ++i) {
        crc ^= data[i];
        for (unsigned bit = 0; bit < 8; ++bit) {
            crc = (crc >> 1) ^ (0xedb88320u & (0u - (crc & 1u)));
        }
    }
    return crc ^ UINT32_MAX;
}

static int parse_bound(const char *text, double *value) {
    char *end;
    errno = 0;
    *value = strtod(text, &end);
    return end != text && *end == '\0' && errno != ERANGE && isfinite(*value);
}

static void usage(const char *program) {
    fprintf(stderr, "Usage: %s INPUT.bin MIN_VALUE MAX_VALUE\n", program);
}

int main(int argc, char **argv) {
    double minimum, maximum;
    if (argc != 4 || !parse_bound(argv[2], &minimum) ||
        !parse_bound(argv[3], &maximum) || minimum > maximum) {
        usage(argv[0]);
        return 3;
    }

    FILE *input = fopen(argv[1], "rb");
    if (!input) {
        perror(argv[1]);
        return 3;
    }

    unsigned char frame[FRAME_SIZE];
    uint64_t total = 0, valid = 0, corrupt = 0, alerts = 0;
    uint64_t gaps = 0, regressions = 0, missing = 0;
    uint32_t previous_sequence = 0;
    uint64_t previous_timestamp = 0;
    int have_previous = 0;
    int read_error = 0;
    for (;;) {
        size_t bytes = fread(frame, 1, FRAME_SIZE, input);
        if (bytes == 0) {
            if (ferror(input)) read_error = 1;
            break;
        }
        ++total;
        if (bytes < FRAME_SIZE) {
            if (ferror(input)) read_error = 1;
            fprintf(stderr, "Truncated frame at record %" PRIu64 " (%zu bytes)\n", total, bytes);
            ++corrupt;
            break;
        }
        if (frame[0] != 'F' || frame[1] != 'W' || frame[2] != 1 ||
            frame[3] != FRAME_SIZE || u32be(frame + PAYLOAD_SIZE) != crc32(frame, PAYLOAD_SIZE)) {
            fprintf(stderr, "Invalid header or CRC at record %" PRIu64 "\n", total);
            ++corrupt;
            continue;
        }

        uint32_t sequence = u32be(frame + 4);
        uint64_t timestamp = u64be(frame + 8);
        uint16_t sensor = u16be(frame + 16);
        uint32_t bits = u32be(frame + 18);
        float value;
        memcpy(&value, &bits, sizeof value);
        if (!isfinite(value) || frame[22] != 0 || frame[23] != 0) {
            fprintf(stderr, "Invalid payload at record %" PRIu64 "\n", total);
            ++corrupt;
            continue;
        }

        ++valid;
        if (have_previous) {
            if (sequence <= previous_sequence) ++regressions;
            else if (sequence > previous_sequence + 1u && previous_sequence != UINT32_MAX)
                missing += (uint64_t)sequence - previous_sequence - 1u;
            if (timestamp < previous_timestamp) ++regressions;
        }
        previous_sequence = sequence;
        previous_timestamp = timestamp;
        have_previous = 1;
        if (value < minimum || value > maximum) {
            ++alerts;
            printf("ALERT sensor=%u sequence=%" PRIu32 " value=%g\n", sensor, sequence, value);
        }
    }
    if (fclose(input) != 0) read_error = 1;
    if (read_error) {
        fprintf(stderr, "Input read failed\n");
        return 3;
    }
    gaps = missing;
    printf("frames=%" PRIu64 " valid=%" PRIu64 " corrupt=%" PRIu64
           " missing_sequences=%" PRIu64 " regressions=%" PRIu64
           " threshold_alerts=%" PRIu64 "\n",
           total, valid, corrupt, gaps, regressions, alerts);
    return corrupt || gaps || regressions || alerts ? 2 : 0;
}
