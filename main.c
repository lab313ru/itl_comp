#include <stdlib.h>
#include <stdio.h>
#include <string.h>

typedef unsigned char byte;
typedef unsigned short word;
typedef unsigned int dword;

byte peek_byte(const char* data, int pos) {
    return data[pos] & 0xFF;
}

byte read_byte(const char* data, int* pos) {
    int b = data[*pos];
    *pos = *pos + 1;
    return b & 0xFF;
}

word peek_word(const char* data, int pos) {
    byte b1 = peek_byte(data, pos + 0);
    byte b2 = peek_byte(data, pos + 1);
    return (b1 << 8) | (b2 << 0);
}

word read_word(const char* data, int* pos) {
    byte b1 = read_byte(data, pos);
    byte b2 = read_byte(data, pos);
    return (b1 << 8) | (b2 << 0);
}

dword peek_dword(char* data, int pos) {
    word w1 = peek_word(data, pos + 0);
    word w2 = peek_word(data, pos + 2);
    return (w1 << 16) | (w2 << 0);
}

dword read_dword(char* data, int* pos) {
    word w1 = read_word(data, pos);
    word w2 = read_word(data, pos);
    return (w1 << 16) | (w2 << 0);
}

void write_byte(char* data, int* pos, byte v) {
    data[*pos] = v & 0xFF;
    *pos = *pos + 1;
}

void write_word(char* data, int* pos, word v) {
    write_byte(data, pos, (v >> 8) & 0xFF);
    write_byte(data, pos, (v >> 0) & 0xFF);
}

void write_dword(char* data, int* pos, dword v) {
    write_word(data, pos, (v >> 16) & 0xFFFF);
    write_word(data, pos, (v >> 0) & 0xFFFF);
}

void xor_long(char* data, int pos, dword val) {
    data[pos + 0] ^= (val >> 24) & 0xFF;
    data[pos + 1] ^= (val >> 16) & 0xFF;
    data[pos + 2] ^= (val >> 8) & 0xFF;
    data[pos + 3] ^= (val >> 0) & 0xFF;
}

int get_unpacked_size(const char* data, int size) {
    int rpos = 0, wpos = 0;

    while (rpos < size) {
        dword xor = 0;

        for (int c1 = 0; c1 < 4 && rpos < size; ++c1) {
            byte d2 = read_byte(data, &rpos);

            for (int c2 = 0; c2 < 8 && rpos < size; ++c2) {
                int bit = (d2 & 0x80);
                d2 <<= 1;

                byte b = bit ? read_byte(data, &rpos) : 0;
            }
        }

        wpos += 32;
    }

    return wpos;
}

void unpack(const char* data, int size, char* dest) {
    char tmp[32];

    int rpos = 0, wpos = 0;

    while (rpos < size) {
        dword xor = 0;
        int tpos = 0;

        for (int c1 = 0; c1 < 4 && rpos < size; ++c1) {
            byte d2 = read_byte(data, &rpos);

            for (int c2 = 0; c2 < 8 && rpos < size; ++c2) {
                int bit = (d2 & 0x80);
                d2 <<= 1;

                byte b = bit ? read_byte(data, &rpos) : 0;
                write_byte(tmp, &tpos, b);
            }
        }

        tpos = 0;

        for (int c1 = 0; c1 < 8; ++c1) {
            xor_long(tmp, tpos, xor);
            xor = read_dword(tmp, &tpos);
        }

        memcpy(&dest[wpos], tmp, sizeof(tmp));
        wpos += sizeof(tmp);
    }
}

int get_packed_size(const char* data, int size) {
    char tmp[32];

    int rpos = 0, wpos = 1, token_pos = 0;

    while (rpos < size) {
        memcpy(tmp, &data[rpos], sizeof(tmp));
        rpos += sizeof(tmp);

        int tpos = sizeof(tmp) - 4;

        for (int c1 = 0; c1 < 7; ++c1) {
            dword x0 = peek_dword(tmp, tpos - 0);
            dword x1 = peek_dword(tmp, tpos - 4);
            write_dword(tmp, &tpos, x0 ^ x1);
            tpos -= 8;
        }

        tpos = 0;

        for (int c1 = 0; c1 < 4 && rpos < size; ++c1) {
            for (int c2 = 0; c2 < 8 && rpos < size; ++c2) {
                if (read_byte(tmp, &tpos) != 0x00) {
                    wpos++;
                }
            }

            wpos++;
        }
    }

    return wpos;
}

void pack(const char* data, int size, char* dest) {
    char tmp[32];

    int rpos = 0, wpos = 1, token_pos = 0;

    while (rpos < size) {
        memcpy(tmp, &data[rpos], sizeof(tmp));
        rpos += sizeof(tmp);

        int tpos = sizeof(tmp) - 4;

        for (int c1 = 0; c1 < 7; ++c1) {
            dword x0 = peek_dword(tmp, tpos - 0);
            dword x1 = peek_dword(tmp, tpos - 4);
            write_dword(tmp, &tpos, x0 ^ x1);
            tpos -= 8;
        }

        tpos = 0;

        for (int c1 = 0; c1 < 4 && rpos < size; ++c1) {
            byte token = 0;

            for (int c2 = 0; c2 < 8 && rpos < size; ++c2) {
                byte b = read_byte(tmp, &tpos);
                int bit = (b != 0x00);
                token |= bit << (7 - c2);

                if (bit) {
                    write_byte(dest, &wpos, b);
                }
            }

            write_byte(dest, &token_pos, token);
            token_pos = wpos;
            wpos++;
        }
    }
}

void print_usage()
{
    printf("Unpack: [filename] [hex_offset]\n");
    printf("Example: bonanza_bros.bin 5935E\n");
    printf("Pack: [filename]\n");
    printf("Example: data_5935E_dec.bin\n\n");
}

int main(int argc, char* argv[]) {
    printf("-= I.T.L. compressor v1.0 [Dr. MefistO] =-\n");
    printf("-----------------------------\n");

    if (argc < 2) {
        printf("Compression type: Non-zero-bytes + XOR\n");
        printf("Coding: Dr. MefistO\n");
        printf("Info: Compressor for Sega I.T.L. games\n\n");
        print_usage();
        printf("-----------------------------\n\n");
        return;
    }

    FILE* f = fopen(argv[1], "rb");
    if (f == NULL) {
        printf("Cannot open input file!\n");
        return -1;
    }

    int offset = 0;
    if (argc == 3) {
        if (sscanf(argv[2], "%x", &offset) == 1) {
            fseek(f, offset, SEEK_SET);
        }
    }

    fseek(f, 0, SEEK_END);
    long size = ftell(f) - offset;

    fseek(f, offset, SEEK_SET);
    char* data = malloc(size);
    fread(data, size, 1, f);
    fclose(f);

    char mode = (argc == 2) ? 'c' : 'd';

    int dest_size = 0;
    char* dest = NULL;

    switch (mode) {
    case 'd': {
        dest_size = get_unpacked_size(data, size);
        dest = malloc(dest_size);
        unpack(data, size, dest);
    } break;
    case 'c': {
        dest_size = get_packed_size(data, size);

        dest = malloc(dest_size);
        memset(dest, 0x7D, dest_size);
        pack(data, size, dest);
    } break;
    }

    char tmp[260];
    sprintf(tmp, "data_%06X_%s.bin", offset, (mode == 'c') ? "enc" : "dec");
    f = fopen(tmp, "wb");

    if (f == NULL) {
        printf("Cannot open output file!\n");
        free(data);
        return -1;
    }

    if (dest != NULL) {
        fwrite(dest, dest_size, 1, f);
        free(dest);
    }

    fclose(f);
    free(data);

    printf("input:  %d bytes\n", size);
    printf("output: %d bytes\n", dest_size);

    return 0;
}