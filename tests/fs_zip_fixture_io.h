#ifndef Q3_FS_ZIP_FIXTURE_IO_H
#define Q3_FS_ZIP_FIXTURE_IO_H
#include <unistd.h>

/* Actual stdio storage must use the disk-backed user scratch root. */
static FILE *FixtureTemporaryFile(void) {
    const char *root = getenv("TMPDIR");
    char path[1024];
    int length, fd;
    FILE *file;
    if (!root || !*root) root = "/var/tmp";
    length = snprintf(path, sizeof(path), "%s/q3-stdio-fixture.XXXXXX", root);
    Check(length > 0 && length < (int)sizeof(path), "bounded disk-backed stdio path");
    fd = mkstemp(path);
    Check(fd >= 0, "disk-backed native stdio descriptor");
    file = fdopen(fd, "w+b");
    if (!file) {
        close(fd);
        unlink(path);
        Check(0, "native stdio owner attaches to descriptor");
    }
    if (unlink(path)) {
        fclose(file);
        Check(0, "anonymous disk-backed stdio owner unlinks");
    }
    return file;
}
#endif
