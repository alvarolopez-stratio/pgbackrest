/***********************************************************************************************************************************
Test Storage File
***********************************************************************************************************************************/
#include "storage/storage.h"

/***********************************************************************************************************************************
Get the mode of a file on local storage
***********************************************************************************************************************************/
mode_t
storageStatMode(const String *path)
{
    // Attempt to stat the file
    struct stat statFile;

    if (stat(strPtr(path), &statFile) == -1)                                                // {uncovered - error should not happen}
        THROW_SYS_ERROR(FileOpenError, "unable to stat '%s'", strPtr(path));                // {uncovered+}

    return statFile.st_mode & 0777;
}

/***********************************************************************************************************************************
Test Run
***********************************************************************************************************************************/
void
testRun()
{
    // Create default storage object for testing
    Storage *storageTest = storageNewP(strNew(testPath()), .write = true, .bufferSize = 2);

    // Create a directory and file that cannot be accessed to test permissions errors
    String *fileNoPerm = strNewFmt("%s/noperm/noperm", testPath());
    String *pathNoPerm = strPath(fileNoPerm);

    TEST_RESULT_INT(
        system(
            strPtr(strNewFmt("sudo mkdir -m 700 %s && sudo touch %s && sudo chmod 600 %s", strPtr(pathNoPerm), strPtr(fileNoPerm),
            strPtr(fileNoPerm)))),
        0, "create no perm path/file");

    // *****************************************************************************************************************************
    if (testBegin("StorageFile"))
    {
        TEST_ERROR_FMT(
            storageFilePosixOpen(pathNoPerm, O_RDONLY, 0, false, &PathOpenError, "test"), PathOpenError,
            "unable to open '%s' for test: [13] Permission denied", strPtr(pathNoPerm));

        // -------------------------------------------------------------------------------------------------------------------------
        String *fileName = strNewFmt("%s/test.file", testPath());

        TEST_ERROR_FMT(
            storageFilePosixOpen(fileName, O_RDONLY, 0, false, &FileOpenError, "read"), FileOpenError,
            "unable to open '%s' for read: [2] No such file or directory", strPtr(fileName));

        TEST_RESULT_INT(storageFilePosixOpen(fileName, O_RDONLY, 0, true, &FileOpenError, "read"), -1, "missing file ignored");

        // -------------------------------------------------------------------------------------------------------------------------
        int handle = -1;

        TEST_RESULT_INT(system(strPtr(strNewFmt("touch %s", strPtr(fileName)))), 0, "create read file");
        TEST_ASSIGN(handle, storageFilePosixOpen(fileName, O_RDONLY, 0, false, &FileOpenError, "read"), "open read file");

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_ERROR_FMT(
            storageFilePosixSync(-99, fileName, &PathSyncError, false), PathSyncError,
            "unable to sync '%s': [9] Bad file descriptor", strPtr(fileName));
        TEST_ERROR_FMT(
            storageFilePosixSync(-99, fileName, &FileSyncError, true), FileSyncError,
            "unable to sync '%s': [9] Bad file descriptor", strPtr(fileName));

        TEST_RESULT_VOID(storageFilePosixSync(handle, fileName, &FileSyncError, false), "sync file");

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_ERROR_FMT(
            storageFilePosixClose(-99, fileName, &FileCloseError), FileCloseError,
            "unable to close '%s': [9] Bad file descriptor", strPtr(fileName));

        TEST_RESULT_VOID(storageFilePosixClose(handle, fileName, &FileSyncError), "close file");

        TEST_RESULT_INT(system(strPtr(strNewFmt("rm %s", strPtr(fileName)))), 0, "remove read file");
    }

    // *****************************************************************************************************************************
    if (testBegin("StorageFileRead"))
    {
        StorageFileRead *file = NULL;

        TEST_ASSIGN(file, storageNewReadP(storageTest, fileNoPerm, .ignoreMissing = true), "new read file");
        TEST_RESULT_PTR(storageFileReadFileDriver(file), file->fileDriver, "    check file driver");
        TEST_RESULT_BOOL(storageFileReadIgnoreMissing(file), true, "    check ignore missing");
        TEST_RESULT_STR(strPtr(storageFileReadName(file)), strPtr(fileNoPerm), "    check name");

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_ASSIGN(file, storageNewReadNP(storageTest, fileNoPerm), "new no perm read file");
        TEST_ERROR_FMT(
            storageFileReadOpen(file), FileOpenError,
            "unable to open '%s' for read: [13] Permission denied", strPtr(fileNoPerm));

        // -------------------------------------------------------------------------------------------------------------------------
        String *fileName = strNewFmt("%s/test.file", testPath());

        TEST_ASSIGN(file, storageNewReadNP(storageTest, fileName), "new missing read file");
        TEST_ERROR_FMT(
            storageFileReadOpen(file), FileOpenError,
            "unable to open '%s' for read: [2] No such file or directory", strPtr(fileName));

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_ASSIGN(file, storageNewReadP(storageTest, fileName, .ignoreMissing = true), "new missing read file");
        TEST_RESULT_BOOL(storageFileReadOpen(file), false, "   missing file ignored");

        // -------------------------------------------------------------------------------------------------------------------------
        Buffer *expectedBuffer = bufNewStr(strNew("TESTFILE\n"));
        TEST_RESULT_VOID(storagePutNP(storageNewWriteNP(storageTest, fileName), expectedBuffer), "write test file");

        TEST_ASSIGN(file, storageNewReadNP(storageTest, fileName), "new read file");
        TEST_RESULT_BOOL(storageFileReadOpen(file), true, "   open file");

        // Close the file handle so operations will fail
        close(file->fileDriver->handle);

        TEST_ERROR_FMT(
            storageFileRead(file), FileReadError,
            "unable to read '%s': [9] Bad file descriptor", strPtr(fileName));
        TEST_ERROR_FMT(
            storageFileReadClose(file), FileCloseError,
            "unable to close '%s': [9] Bad file descriptor", strPtr(fileName));

        // Set file handle to -1 so the close on free with not fail
        file->fileDriver->handle = -1;

        // -------------------------------------------------------------------------------------------------------------------------
        Buffer *buffer = bufNew(0);

        MEM_CONTEXT_TEMP_BEGIN()
        {
            TEST_ASSIGN(file, storageFileReadMove(storageNewReadNP(storageTest, fileName), MEM_CONTEXT_OLD()), "new read file");
        }
        MEM_CONTEXT_TEMP_END();

        TEST_RESULT_BOOL(storageFileReadOpen(file), true, "   open file");
        TEST_RESULT_STR(strPtr(storageFileReadName(file)), strPtr(fileName), "    check file name");

        TEST_RESULT_VOID(bufCat(buffer, storageFileRead(file)), "    load data");
        TEST_RESULT_VOID(bufCat(buffer, storageFileRead(file)), "    load data");
        TEST_RESULT_VOID(bufCat(buffer, storageFileRead(file)), "    load data");
        TEST_RESULT_VOID(bufCat(buffer, storageFileRead(file)), "    load data");
        TEST_RESULT_BOOL(bufEq(buffer, expectedBuffer), false, "    check file contents (not all loaded yet)");

        TEST_RESULT_VOID(bufCat(buffer, storageFileRead(file)), "    load data");
        TEST_RESULT_BOOL(bufEq(buffer, expectedBuffer), true, "    check file contents (all loaded)");

        TEST_RESULT_PTR(storageFileRead(file), NULL, "    eof");
        TEST_RESULT_PTR(storageFileRead(file), NULL, "    still eof");

        TEST_RESULT_VOID(storageFileReadClose(file), "    close file");
        TEST_RESULT_VOID(storageFileReadClose(file), "    close again");

        TEST_RESULT_VOID(storageFileReadFree(file), "   free file");
        TEST_RESULT_VOID(storageFileReadFree(NULL), "   free null file");
        TEST_RESULT_VOID(storageFileReadPosixFree(NULL), "   free null file");

        TEST_RESULT_VOID(storageFileReadMove(NULL, NULL), "   move null file");
    }

    // *****************************************************************************************************************************
    if (testBegin("StorageFileWrite"))
    {
        StorageFileWrite *file = NULL;

        TEST_ASSIGN(
            file,
            storageNewWriteP(
                storageTest, fileNoPerm, .modeFile = 0444, .modePath = 0555, .noCreatePath = true, .noSyncFile = true,
                .noSyncPath = true, .noAtomic = true),
            "new write file");

        TEST_RESULT_BOOL(storageFileWriteAtomic(file), false, "    check atomic");
        TEST_RESULT_BOOL(storageFileWriteCreatePath(file), false, "    check create path");
        TEST_RESULT_PTR(storageFileWriteFileDriver(file), file->fileDriver, "    check file driver");
        TEST_RESULT_INT(storageFileWriteModeFile(file), 0444, "    check mode file");
        TEST_RESULT_INT(storageFileWriteModePath(file), 0555, "    check mode path");
        TEST_RESULT_STR(strPtr(storageFileWriteName(file)), strPtr(fileNoPerm), "    check name");
        TEST_RESULT_STR(strPtr(storageFileWritePath(file)), strPtr(strPath(fileNoPerm)), "    check path");
        TEST_RESULT_BOOL(storageFileWriteSyncPath(file), false, "    check sync path");
        TEST_RESULT_BOOL(storageFileWriteSyncFile(file), false, "    check sync file");

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_ASSIGN(file, storageNewWriteP(storageTest, fileNoPerm, .noAtomic = true), "new write file");
        TEST_ERROR_FMT(
            storageFileWriteOpen(file), FileOpenError,
            "unable to open '%s' for write: [13] Permission denied", strPtr(fileNoPerm));

        // -------------------------------------------------------------------------------------------------------------------------
        String *fileName = strNewFmt("%s/sub1/test.file", testPath());

        TEST_ASSIGN(file, storageNewWriteP(storageTest, fileName, .noCreatePath = true, .noAtomic = true), "new write file");
        TEST_ERROR_FMT(
            storageFileWriteOpen(file), FileOpenError,
            "unable to open '%s' for write: [2] No such file or directory", strPtr(fileName));

        // -------------------------------------------------------------------------------------------------------------------------
        String *fileTmp = strNewFmt("%s.pgbackrest.tmp", strPtr(fileName));
        Buffer *buffer = bufNewStr(strNew("TESTFILE\n"));

        TEST_ASSIGN(file, storageNewWriteNP(storageTest, fileName), "new write file");
        TEST_RESULT_STR(strPtr(storageFileWriteName(file)), strPtr(fileName), "    check file name");
        TEST_RESULT_VOID(storageFileWriteOpen(file), "    open file");

        // Close the file handle so operations will fail
        close(file->fileDriver->handle);
        storageRemoveP(storageTest, fileTmp, .errorOnMissing = true);

        TEST_ERROR_FMT(
            storageFileWrite(file, buffer), FileWriteError,
            "unable to write '%s': [9] Bad file descriptor", strPtr(fileName));
        TEST_ERROR_FMT(
            storageFileWriteClose(file), FileSyncError,
            "unable to sync '%s': [9] Bad file descriptor", strPtr(fileName));

        // Disable file sync so the close can be reached
        file->fileDriver->noSyncFile = true;

        TEST_ERROR_FMT(
            storageFileWriteClose(file), FileCloseError,
            "unable to close '%s': [9] Bad file descriptor", strPtr(fileName));

        // Set file handle to -1 so the close on free with not fail
        file->fileDriver->handle = -1;

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_ASSIGN(file, storageNewWriteNP(storageTest, fileName), "new write file");
        TEST_RESULT_STR(strPtr(storageFileWriteName(file)), strPtr(fileName), "    check file name");
        TEST_RESULT_VOID(storageFileWriteOpen(file), "    open file");

        // Rename the file back to original name from tmp -- this will cause the rename in close to fail
        TEST_RESULT_INT(rename(strPtr(fileTmp), strPtr(fileName)), 0, " rename tmp file");
        TEST_ERROR_FMT(
            storageFileWriteClose(file), FileMoveError,
            "unable to move '%s' to '%s': [2] No such file or directory", strPtr(fileTmp), strPtr(fileName));

        // Set file handle to -1 so the close on free with not fail
        file->fileDriver->handle = -1;

        storageRemoveP(storageTest, fileName, .errorOnMissing = true);

        // -------------------------------------------------------------------------------------------------------------------------
        MEM_CONTEXT_TEMP_BEGIN()
        {
            TEST_ASSIGN(file, storageFileWriteMove(storageNewWriteNP(storageTest, fileName), MEM_CONTEXT_OLD()), "new write file");
        }
        MEM_CONTEXT_TEMP_END();

        TEST_RESULT_VOID(storageFileWriteOpen(file), "    open file");
        TEST_RESULT_VOID(storageFileWrite(file, NULL), "   write null buffer to file");
        TEST_RESULT_VOID(storageFileWrite(file, bufNew(0)), "   write zero buffer to file");
        TEST_RESULT_VOID(storageFileWrite(file, buffer), "   write to file");
        TEST_RESULT_VOID(storageFileWriteClose(file), "   close file");
        TEST_RESULT_VOID(storageFileWriteFree(file), "   free file");
        TEST_RESULT_VOID(storageFileWriteFree(NULL), "   free null file");
        TEST_RESULT_VOID(storageFileWritePosixFree(NULL), "   free null posix file");
        TEST_RESULT_VOID(storageFileWriteMove(NULL, NULL), "   move null file");

        Buffer *expectedBuffer = storageGetNP(storageNewReadNP(storageTest, fileName));
        TEST_RESULT_BOOL(bufEq(buffer, expectedBuffer), true, "    check file contents");
        TEST_RESULT_INT(storageStatMode(storagePath(storageTest, strPath(fileName))), 0750, "    check path mode");
        TEST_RESULT_INT(storageStatMode(storagePath(storageTest, fileName)), 0640, "    check file mode");

        storageRemoveP(storageTest, fileName, .errorOnMissing = true);

        // -------------------------------------------------------------------------------------------------------------------------
        fileName = strNewFmt("%s/sub2/test.file", testPath());

        TEST_ASSIGN(
            file,
            storageNewWriteP(
                storageTest, fileName, .modePath = 0700, .modeFile = 0600, .noSyncPath = true, .noSyncFile = true,
                .noAtomic = true),
            "new write file");
        TEST_RESULT_VOID(storageFileWriteOpen(file), "    open file");
        TEST_RESULT_VOID(storageFileWrite(file, buffer), "   write to file");
        TEST_RESULT_VOID(storageFileWriteClose(file), "   close file");

        expectedBuffer = storageGetNP(storageNewReadNP(storageTest, fileName));
        TEST_RESULT_BOOL(bufEq(buffer, expectedBuffer), true, "    check file contents");
        TEST_RESULT_INT(storageStatMode(storagePath(storageTest, strPath(fileName))), 0700, "    check path mode");
        TEST_RESULT_INT(storageStatMode(storagePath(storageTest, fileName)), 0600, "    check file mode");

        storageRemoveP(storageTest, fileName, .errorOnMissing = true);
    }
}
