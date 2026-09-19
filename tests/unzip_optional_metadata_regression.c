/* Actual ZIP optional metadata reads respect caller buffers and archive prefixes. */
#define main FixtureZipMain
#include "fs_zip_regression.c"
#undef main
#include <fcntl.h>
#include <unistd.h>

static const unsigned char extraBytes[] = { 0xfe, 0xca, 16, 0,
    '0','1','2','3','4','5','6','7','8','9','a','b','c','d','e','f' };
static const char commentBytes[] = "native comment!";

static void Start(char *path) {
    char bytes[3];
    Begin();
    search.pack = FS_LoadZipFile(path, "native.pk3");
    Check(search.pack && unzOpenCurrentFile(search.pack->handle) == UNZ_OK &&
          unzReadCurrentFile(search.pack->handle, bytes, sizeof(bytes)) == sizeof(bytes) &&
          !memcmp(bytes, "nat", sizeof(bytes)), "real active native ZIP metadata entry");
}
static void Finish(void) {
    char bytes[16];
    Check(unztell(search.pack->handle) == 3 &&
          unzReadCurrentFile(search.pack->handle, bytes, sizeof(bytes)) == 9 &&
          !memcmp(bytes, "ive data\n", 9), "optional metadata retains native payload cursor and continuation");
    Check(unzCloseCurrentFile(search.pack->handle) == UNZ_OK, "actual metadata decoder physically closes");
    End();
}
static void Local(char *path, int size) {
    unsigned char *bytes = malloc(size);
    unsigned char *guarded;
    int i;
    Check(bytes != NULL, "exact caller buffer physically allocated");
    Start(path);
    Check(unzGetLocalExtrafield(search.pack->handle, NULL, UINT_MAX) == sizeof(extraBytes),
          "native local extra query retains complete size");
    memset(bytes, 0xa5, size);
    Check(unzGetLocalExtrafield(search.pack->handle, bytes, size) == size &&
          !memcmp(bytes, extraBytes, size), "local extra read respects exact destination and real prefix offset");
    free(bytes);
    guarded = malloc(size + sizeof(extraBytes) + 8);
    Check(guarded != NULL, "actual caller metadata tail allocated");
    memset(guarded, 0xa5, size + sizeof(extraBytes) + 8);
    Check(unzGetLocalExtrafield(search.pack->handle, guarded, size) == size &&
          !memcmp(guarded, extraBytes, size), "counted local extra payload remains native");
    for (i = size; i < size + sizeof(extraBytes) + 8; i++)
        Check(guarded[i] == 0xa5, "local extra respects every caller destination tail byte");
    free(guarded);
    Finish();
}
static void MissingComment(char *path, unsigned size) {
    long cursor;
    Start(path);
    cursor = ftell(((unz_s *)search.pack->handle)->file);
    Check(unzGetGlobalComment(search.pack->handle, NULL, size) == UNZ_PARAMERROR &&
          ftell(((unz_s *)search.pack->handle)->file) == cursor,
          "positive comment request rejects NULL before FILE seek or access");
    Finish();
}
static void NativeGolden(char *path) {
    unsigned char bytes[64];
    long cursor;
    Start(path);
    memset(bytes, 0xa5, sizeof(bytes));
    Check(unzGetLocalExtrafield(search.pack->handle, bytes, sizeof(bytes)) == sizeof(extraBytes) &&
          !memcmp(bytes, extraBytes, sizeof(extraBytes)) && bytes[sizeof(extraBytes)] == 0xa5,
          "native complete local extra bytes and destination tail golden");
    Check(unzGetLocalExtrafield(search.pack->handle, NULL, 0) == sizeof(extraBytes) &&
          unzGetLocalExtrafield(search.pack->handle, bytes, sizeof(extraBytes)) == sizeof(extraBytes) &&
          !memcmp(bytes, extraBytes, sizeof(extraBytes)), "native repeated local extra read retains legacy query semantics");
    memset(bytes, 0xa5, sizeof(bytes));
    Check(unzGetGlobalComment(search.pack->handle, (char *)bytes, 4) == 4 &&
          !memcmp(bytes, commentBytes, 4) && bytes[4] == 0xa5,
          "native short global comment returns counted unterminated bytes");
    memset(bytes, 0xa5, sizeof(bytes));
    Check(unzGetGlobalComment(search.pack->handle, (char *)bytes, sizeof(bytes)) == sizeof(commentBytes)-1 &&
          !memcmp(bytes, commentBytes, sizeof(commentBytes)) && bytes[sizeof(commentBytes)] == 0xa5,
          "native complete global comment returns count and terminator golden");
    cursor = ftell(((unz_s *)search.pack->handle)->file);
    Check(!unzGetLocalExtrafield(search.pack->handle, bytes, 0) &&
          ftell(((unz_s *)search.pack->handle)->file) == cursor &&
          !unzGetGlobalComment(search.pack->handle, NULL, 0), "native zero-length optional reads remain safe");
    Finish();
}
static void Truncated(char *path) {
    unsigned char bytes[4];
    unz_s *archive;
    int writer;
    off_t offset;
    Start(path);
    archive = search.pack->handle;
    offset = archive->pfile_in_zip_read->offset_local_extrafield + archive->byte_before_the_zipfile;
    Check(!fflush(archive->file), "native input buffering discarded before actual truncation");
    writer = open(path, O_WRONLY);
    Check(writer >= 0 && !ftruncate(writer, offset + 2) && !close(writer),
          "real local extra is truncated below requested bytes");
    memset(bytes, 0xa5, sizeof(bytes));
    Check(unzGetLocalExtrafield(search.pack->handle, bytes, sizeof(bytes)) == UNZ_ERRNO &&
          bytes[2] == 0xa5 && bytes[3] == 0xa5, "truncated local metadata reports I/O failure within destination");
    Check(unzCloseCurrentFile(search.pack->handle) == UNZ_OK, "truncated metadata decoder closes");
    End();
}
int main(int argc, char **argv) {
    int i;
    Check(argc >= 2, "real optional metadata ZIP input");
    if (argc == 3) {
        i = atoi(argv[2]);
        if (i == 0) Local(argv[1], 1);
        else if (i == 1) Local(argv[1], 4);
        else if (i == 2) Local(argv[1], sizeof(extraBytes));
        else if (i == 3) Local(argv[1], 1);
        else if (i == 4) MissingComment(argv[1], 1);
        else if (i == 5) MissingComment(argv[1], UINT_MAX);
        else if (i == 8) Truncated(argv[1]);
        else NativeGolden(argv[1]);
        return 0;
    }
    Check(argc == 5, "both native methods and archive prefixes supplied");
    for (i = 1; i < argc; i++) {
        NativeGolden(argv[i]);
        Local(argv[i], 1);
        Local(argv[i], 4);
        Local(argv[i], sizeof(extraBytes));
        MissingComment(argv[i], 1);
        MissingComment(argv[i], UINT_MAX);
    }
    Truncated(argv[3]);
    Truncated(argv[4]);
    puts("Actual optional ZIP metadata respects destination lengths, prefixes and native payloads");
    return 0;
}
