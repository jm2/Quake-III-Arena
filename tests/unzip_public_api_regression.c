/* Public unzip arguments and failed filename lookup remain transactional. */
#define main FixtureZipMain
#ifndef Q3_ZIP_NATIVE_FIXTURE
#define Q3_ZIP_NATIVE_FIXTURE "fs_zip_regression.c"
#endif
#include Q3_ZIP_NATIVE_FIXTURE
#undef main

static unz_s *Start(char *path) {
    unz_s *archive;

    Begin();
    search.pack = FS_LoadZipFile(path, "native.pk3");
    Check(search.pack != NULL && search.pack->numfiles == 2,
          "actual two-entry native ZIP mounts");
    Check(unzGoToFirstFile(search.pack->handle) == UNZ_OK,
          "actual first native ZIP entry selects");
    archive = search.pack->handle;
    Check(archive->current_file_ok && archive->num_file == 0,
          "first selected-entry state is valid");
    return archive;
}

static void CheckSelected(unz_s *archive, const char *expected) {
    char name[UNZ_MAXFILENAMEINZIP + 1];

    Check(unzGetCurrentFileInfo(archive, NULL, name, sizeof(name),
                                NULL, 0, NULL, 0) == UNZ_OK &&
          !strcmp(name, expected), "selected native filename remains coherent");
}

static void NativeGolden(char *path) {
    unsigned char bytes[32];
    unz_global_info info;
    unsigned long position;
    unz_s *archive = Start(path);

    Check(unzGetGlobalInfo(archive, &info) == UNZ_OK && info.number_entry == 2,
          "ordinary global-info output remains native");
    Check(unzGetCurrentFileInfoPosition(archive, &position) == UNZ_OK &&
          position == archive->offset_central_dir,
          "ordinary selected-position output remains native");
    CheckSelected(archive, "first.txt");
    Check(unzLocateFile(archive, "second.txt", UNZ_CASESENSITIVE) == UNZ_OK,
          "ordinary exact filename lookup succeeds");
    CheckSelected(archive, "second.txt");
    Check(unzOpenCurrentFile(archive) == UNZ_OK &&
          unzReadCurrentFile(archive, bytes, sizeof(bytes)) == 15 &&
          !memcmp(bytes, "second payload\n", 15) &&
          !unzReadCurrentFile(archive, bytes, 1),
          "located native entry opens, reads and reaches EOF");
    Check(unzCloseCurrentFile(archive) == UNZ_OK,
          "ordinary located decoder closes");
    End();
}

static void MissingLookup(char *path, int active) {
    unsigned char bytes[32];
    unz_s saved;
    file_in_zip_read_info_s savedDecoder;
    long cursor;
    unz_s *archive = Start(path);

    if (active) {
        Check(unzOpenCurrentFile(archive) == UNZ_OK &&
              unzReadCurrentFile(archive, bytes, 3) == 3 &&
              !memcmp(bytes, "fir", 3),
              "active native first-entry decoder advances before lookup miss");
    }
    memcpy(&saved, archive, sizeof(saved));
    if (active)
        memcpy(&savedDecoder, archive->pfile_in_zip_read, sizeof(savedDecoder));
    cursor = ftell(archive->file);
    Check(cursor >= 0, "selected native archive has a physical cursor");

    Check(unzLocateFile(archive, "missing.txt", UNZ_CASESENSITIVE) ==
          UNZ_END_OF_LIST_OF_FILE, "missing native filename reports list end");
    Check(!memcmp(&saved, archive, sizeof(saved)) && ftell(archive->file) == cursor,
          "failed lookup preserves complete selected archive and physical cursor");
    if (active) {
        Check(!memcmp(&savedDecoder, archive->pfile_in_zip_read,
                      sizeof(savedDecoder)),
              "failed lookup preserves complete active decoder state");
        Check(unzReadCurrentFile(archive, bytes, sizeof(bytes)) == 11 &&
              !memcmp(bytes, "st payload\n", 11) &&
              !unzReadCurrentFile(archive, bytes, 1),
              "active first-entry decoder continues after lookup miss");
        Check(unzCloseCurrentFile(archive) == UNZ_OK,
              "continued decoder closes after lookup miss");
    } else {
        CheckSelected(archive, "first.txt");
        Check(unzOpenCurrentFile(archive) == UNZ_OK &&
              unzReadCurrentFile(archive, bytes, sizeof(bytes)) == 14 &&
              !memcmp(bytes, "first payload\n", 14),
              "restored first entry opens and reads after lookup miss");
        Check(unzCloseCurrentFile(archive) == UNZ_OK,
              "restored selected decoder closes");
    }
    End();
}

static void NullOutput(char *path, int kind) {
    unz_s *archive = Start(path);
    unz_s saved;
    long cursor;
    int result;

    memcpy(&saved, archive, sizeof(saved));
    cursor = ftell(archive->file);
    Check(cursor >= 0, "required-pointer fixture has a physical cursor");
    if (kind == 0)
        result = unzGetGlobalInfo(archive, NULL);
    else if (kind == 1)
        result = unzGetCurrentFileInfoPosition(archive, NULL);
    else
        result = unzLocateFile(archive, NULL, UNZ_CASESENSITIVE);
    Check(result == UNZ_PARAMERROR,
          "required public ZIP pointer rejects with parameter error");
    Check(!memcmp(&saved, archive, sizeof(saved)) && ftell(archive->file) == cursor,
          "required-pointer rejection occurs before archive state or I/O");
    CheckSelected(archive, "first.txt");
    End();
}

static void NullRead(char *path) {
    unsigned char bytes[32];
    unz_s *archive = Start(path);
    file_in_zip_read_info_s saved;
    long cursor;

    Check(unzOpenCurrentFile(archive) == UNZ_OK,
          "native decoder opens before invalid destination");
    memcpy(&saved, archive->pfile_in_zip_read, sizeof(saved));
    cursor = ftell(archive->file);
    Check(cursor >= 0 && unzReadCurrentFile(archive, NULL, 1) == UNZ_PARAMERROR,
          "positive native read rejects a null destination");
    Check(!memcmp(&saved, archive->pfile_in_zip_read, sizeof(saved)) &&
          ftell(archive->file) == cursor,
          "rejected null destination preserves decoder and physical cursor");
    Check(!unzReadCurrentFile(archive, NULL, 0),
          "zero-length null destination retains legacy no-op behavior");
    Check(unzReadCurrentFile(archive, bytes, sizeof(bytes)) == 14 &&
          !memcmp(bytes, "first payload\n", 14) &&
          !unzReadCurrentFile(archive, bytes, 1),
          "decoder reads complete native payload after rejected destination");
    Check(unzCloseCurrentFile(archive) == UNZ_OK,
          "decoder after rejected destination closes");
    End();
}

static void EmbeddedName(char *path) {
    unz_s saved;
    unz_s *archive;
    long cursor;

    Begin();
    archive = (unz_s *)unzOpen(path);
    Check(archive != NULL && unzGoToFirstFile(archive) == UNZ_OK,
          "actual embedded-NUL central filename opens directly");
    memcpy(&saved, archive, sizeof(saved));
    cursor = ftell(archive->file);
    Check(cursor >= 0 &&
          unzLocateFile(archive, "evil", UNZ_CASESENSITIVE) ==
          UNZ_END_OF_LIST_OF_FILE,
          "embedded-NUL archive name cannot impersonate its visible prefix");
    Check(!memcmp(&saved, archive, sizeof(saved)) && ftell(archive->file) == cursor,
          "rejected embedded-NUL lookup preserves direct archive selection");
    Check(unzClose(archive) == UNZ_OK,
          "direct malformed-name archive closes every owner");
    End();
}

int main(int argc, char **argv) {
    int kind;

    Check(argc >= 2, "actual public unzip API fixture supplied");
    if (argc == 3) {
        kind = atoi(argv[2]);
        if (kind < 3) NullOutput(argv[1], kind);
        else if (kind == 3) NullRead(argv[1]);
        else if (kind == 4) MissingLookup(argv[1], 0);
        else if (kind == 5) MissingLookup(argv[1], 1);
        else if (kind == 6) EmbeddedName(argv[1]);
        else NativeGolden(argv[1]);
        return 0;
    }
    Check(argc == 5 && !strcmp(argv[4], "all"),
          "stored, deflated and malformed-name unzip fixtures supplied");
    for (kind = 1; kind < 3; kind++) {
        NativeGolden(argv[kind]);
        MissingLookup(argv[kind], 0);
        MissingLookup(argv[kind], 1);
        NullOutput(argv[kind], 0);
        NullOutput(argv[kind], 1);
        NullOutput(argv[kind], 2);
        NullRead(argv[kind]);
    }
    EmbeddedName(argv[3]);
    puts("Public unzip pointers, failed lookup state and native reads pass");
    return 0;
}
