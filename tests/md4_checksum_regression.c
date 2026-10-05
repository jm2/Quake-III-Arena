/* md4.c computes retail MD4 on every host: RFC 1320's test vectors, and the
   pk3 header and pure checksums FS_LoadZipFile derives from it.  md4.c's word
   type used to be unsigned long, 64 bits on LP64 hosts, where the sums and
   rotations kept bits above bit 31 and every digest was wrong. */
#define main FixtureZipMain
#include "fs_zip_regression.c"
#undef main

/* Retail id pk3 header checksums (ioquake3's files.c pak_checksums[0] and
   missionpak_checksums[3]); pure checksums with this feed and every expected
   value below come from an independent Python MD4 checked against RFC 1320. */
#define FIXTURE_FEED 0x5eed1234

static void Pk3(char *dir, const char *name, unsigned checksum, unsigned pure) {
    char path[1024];
    pack_t *p;
    Begin();
    fs_checksumFeed = FIXTURE_FEED;
    Check(snprintf(path, sizeof(path), "%s/%s", dir, name) < (int)sizeof(path), "bounded fixture path");
    search.pack = p = FS_LoadZipFile(path, name);
    Check(p && p->numfiles == 6, "generated pk3 mounts");
    if ((unsigned)p->checksum != checksum || (unsigned)p->pure_checksum != pure) {
        fprintf(stderr, "%s: header checksum %u (want %u), pure checksum %u (want %u)\n", name,
                (unsigned)p->checksum, checksum, (unsigned)p->pure_checksum, pure);
        Check(0, "pk3 checksums are retail's");
    }
    End();
}

/* md4.c's own interface, linked from the real file: md4.c defines MD4_CTX
   itself, so this context only needs to be at least as large and aligned. */
typedef union { double align; void *pointer; unsigned long words[64]; } MD4_CTX;
void MD4Init (MD4_CTX *);
void MD4Update (MD4_CTX *, const unsigned char *, unsigned int);
void MD4Final (unsigned char [16], MD4_CTX *);

static void Digest(const char *input, const char *hex) {
    MD4_CTX ctx;
    unsigned char digest[16], expected[16];
    unsigned int i, length = strlen(input), step;
    for (i = 0; i < 16; i++) {
        Check(sscanf(hex + 2 * i, "%2hhx", &expected[i]) == 1, "expected digest");
    }
    /* Whole, and split at every chunk size so MD4Update's buffering runs. */
    for (step = 0; step <= length; step++) {
        MD4Init(&ctx);
        if (!step) {
            MD4Update(&ctx, (const unsigned char *)input, length);
        } else {
            for (i = 0; i < length; i += step) {
                MD4Update(&ctx, (const unsigned char *)input + i, length - i < step ? length - i : step);
            }
        }
        MD4Final(digest, &ctx);
        if (memcmp(digest, expected, 16)) {
            fprintf(stderr, "MD4(\"%s\") step %u: ", input, step);
            for (i = 0; i < 16; i++) fprintf(stderr, "%02x", digest[i]);
            fprintf(stderr, ", want %s\n", hex);
            Check(0, "RFC 1320 MD4 test vector");
        }
        if (!length) break;
    }
}

int main(int argc, char **argv) {
    static const struct { const char *input, *digest; unsigned checksum; } rfc[] = {
        { "", "31d6cfe0d16ae931b73c59d7e0c089c0", 3338027191u },
        { "a", "bde52cb31de33e46245e05fbdbd6fb24", 720146015u },
        { "abc", "a448017aaf21d8525fc10ae87aa6729d", 1570836014u },
        { "message digest", "d9130a8164549fe818874806e1c7014b", 618399556u },
        { "abcdefghijklmnopqrstuvwxyz", "d79e1c308aa5bbcdeea8ed63df412da9", 929550956u },
        { "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789",
          "043f8582f241db351ce627e153e7f0e4", 2995355577u },
        { "12345678901234567890123456789012345678901234567890123456789012345678901234567890",
          "e33b4ddc9c38f2199c3e7b164fcc0536", 3854692780u },
    };
    static unsigned char large[1000];
    unsigned i;
    Check(argc == 2, "fixture directory");
    for (i = 0; i < sizeof(rfc) / sizeof(rfc[0]); i++) {
        Digest(rfc[i].input, rfc[i].digest);
        /* The XOR of the four little-endian digest words, as a native value. */
        Check(LittleLong(Com_BlockChecksum((void *)rfc[i].input, strlen(rfc[i].input))) == (int)rfc[i].checksum,
              "Com_BlockChecksum of an RFC 1320 vector");
    }
    for (i = 0; i < sizeof(large); i++) large[i] = (unsigned char)(i * 7 + 3);
    Check(LittleLong(Com_BlockChecksum(large, sizeof(large))) == (int)3049532863u,
          "Com_BlockChecksum over many blocks");
    Check(LittleLong(Com_BlockChecksumKey(large, sizeof(large), LittleLong(-2))) == (int)2437577824u,
          "Com_BlockChecksumKey over many blocks");
    Pk3(argv[1], "pak0.pk3", 1566731103u, 1760374276u);
    Pk3(argv[1], "pak3.pk3", 1438664554u, 163063953u);
    return 0;
}
