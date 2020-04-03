// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.base;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertTrue;

import android.net.Uri;

import org.junit.Ignore;
import org.junit.Rule;
import org.junit.Test;
import org.junit.rules.TemporaryFolder;
import org.junit.runner.RunWith;
import org.robolectric.annotation.Config;

import org.chromium.base.test.BaseRobolectricTestRunner;

import java.io.ByteArrayInputStream;
import java.io.ByteArrayOutputStream;
import java.io.File;
import java.io.FileInputStream;
import java.io.IOException;
import java.io.InputStream;
import java.nio.file.FileVisitResult;
import java.nio.file.Files;
import java.nio.file.Path;
import java.nio.file.SimpleFileVisitor;
import java.nio.file.attribute.BasicFileAttributes;
import java.util.ArrayList;
import java.util.Arrays;
import java.util.Collections;

/** Unit tests for {@link Log}. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class FileUtilsTest {
    @Rule
    public final TemporaryFolder temporaryFolder = new TemporaryFolder();

    /**
     * Recursively lists all paths under a directory as relative paths, rendered as a string.
     *
     * @param rootDir The directory {@link Path}.
     * @return A "; "-deliminated string of relative paths of all files stirctly under |rootDir|,
     *         lexicographically by path segments. Directories have "/" as suffix.
     */
    private String listAllPaths(Path rootDir) {
        ArrayList<String> pathList = new ArrayList<String>();
        try {
            Files.walkFileTree(rootDir, new SimpleFileVisitor<Path>() {
                @Override
                public FileVisitResult preVisitDirectory(Path path, BasicFileAttributes attrs)
                        throws IOException {
                    String relPathString = rootDir.relativize(path).toString();
                    if (!relPathString.isEmpty()) { // Exclude |rootDir|.
                        pathList.add(relPathString + "/");
                    }
                    return FileVisitResult.CONTINUE;
                }
                @Override
                public FileVisitResult visitFile(Path path, BasicFileAttributes attrs)
                        throws IOException {
                    pathList.add(rootDir.relativize(path).toString());
                    return FileVisitResult.CONTINUE;
                }
            });
        } catch (IOException e) {
        }

        // Sort paths lexicographically by path segments. For example, "foo.bar/file" and "foo/sub"
        // are treated as ["foo.bar", "file"] and ["foo", "sub"], then compared lexicographically
        // element-by-element. Since "foo.bar" < "foo" (String comparison), so the order is
        // "foo/sub" < "foo.bar/file". Instead of actually splitting the strings into lists, we
        // simply replace '/' with |kSep| as ASCII character 1 for sorting...
        final char kSep = (char) 1;
        for (int i = 0; i < pathList.size(); ++i) {
            pathList.set(i, pathList.get(i).replace('/', kSep));
        }
        Collections.sort(pathList);
        // Then restore '/'.
        for (int i = 0; i < pathList.size(); ++i) {
            pathList.set(i, pathList.get(i).replace(kSep, '/'));
        }
        return String.join("; ", pathList);
    }

    /**
     * Helper to check the current list of temp files and directories matches expectation.
     *
     * @param expectedFileList A string representation of the expected list of temp files and
     *        directories. See listAllPaths() for format.
     */
    private void assertFileList(String expectedFileList) {
        Path rootDir = temporaryFolder.getRoot().toPath();
        assertEquals(expectedFileList, listAllPaths(rootDir));
    }

    /**
     * Helper to get the absolute path strings of multiple temp paths created for testing.
     *
     * @param relPathnames Relative names of temp files or directories (does not need to exist).
     */
    private ArrayList<String> getPathNames(String... relPathNames) {
        Path rootDir = temporaryFolder.getRoot().toPath();
        ArrayList<String> ret = new ArrayList<String>();
        for (String relPathName : relPathNames) {
            ret.add(rootDir.resolve(relPathName).toString());
        }
        return ret;
    }

    /**
     * Helper to get the {@link File} object of a temp paths created for testing.
     *
     * @param relPathname The relative name of a temp file or directory (does not need to exist).
     */
    private File getFile(String relPathName) {
        Path rootDir = temporaryFolder.getRoot().toPath();
        return rootDir.resolve(relPathName).toFile();
    }

    /**
     * Helper to create a mix of test files and directories. Can be called multiple times per test,
     * but requires the temp file to be empty.
     */
    private void prepareMixedFilesTestCase() throws IOException {
        assertFileList("");
        temporaryFolder.newFolder("a1");
        temporaryFolder.newFolder("a1", "b1");
        temporaryFolder.newFile("a1/b1/c");
        temporaryFolder.newFile("a1/b1/c2");
        temporaryFolder.newFolder("a1", "b2");
        temporaryFolder.newFolder("a1", "b2", "c");
        temporaryFolder.newFile("a1/b3");
        temporaryFolder.newFolder("a2");
        temporaryFolder.newFile("c");
    }

    @Test
    public void testRecursivelyDeleteFileBasic() throws IOException {
        // Test file deletion.
        temporaryFolder.newFile("some_File");
        temporaryFolder.newFile("some");
        temporaryFolder.newFile(".dot-config1");
        temporaryFolder.newFile("some_File.txt");
        assertFileList(".dot-config1; some; some_File; some_File.txt");
        assertTrue(FileUtils.recursivelyDeleteFile(getFile("some_File"), null));
        assertFileList(".dot-config1; some; some_File.txt");
        assertTrue(FileUtils.recursivelyDeleteFile(getFile("some"), null));
        assertFileList(".dot-config1; some_File.txt");
        assertTrue(FileUtils.recursivelyDeleteFile(getFile("ok_to_delete_nonexistent"), null));
        assertFileList(".dot-config1; some_File.txt");
        assertTrue(FileUtils.recursivelyDeleteFile(getFile(".dot-config1"), null));
        assertFileList("some_File.txt");
        assertTrue(FileUtils.recursivelyDeleteFile(getFile("some_File.txt"), null));
        assertFileList("");

        // Test directory deletion.
        temporaryFolder.newFolder("some_Dir");
        temporaryFolder.newFolder("some");
        temporaryFolder.newFolder(".dot-dir2");
        temporaryFolder.newFolder("some_Dir.ext");
        assertFileList(".dot-dir2/; some/; some_Dir/; some_Dir.ext/");
        assertTrue(FileUtils.recursivelyDeleteFile(getFile("some_Dir"), null));
        assertFileList(".dot-dir2/; some/; some_Dir.ext/");
        assertTrue(FileUtils.recursivelyDeleteFile(getFile("some"), null));
        assertFileList(".dot-dir2/; some_Dir.ext/");
        assertTrue(FileUtils.recursivelyDeleteFile(getFile("ok/to/delete/nonexistent"), null));
        assertFileList(".dot-dir2/; some_Dir.ext/");
        assertTrue(FileUtils.recursivelyDeleteFile(getFile(".dot-dir2"), null));
        assertFileList("some_Dir.ext/");
        assertTrue(FileUtils.recursivelyDeleteFile(getFile("some_Dir.ext"), null));
        assertFileList("");

        // Test recursive deletion of mixed files and directories.
        for (int i = 0; i < 2; ++i) {
            Function<String, Boolean> canDelete = (i == 0) ? null : FileUtils.DELETE_ALL;
            prepareMixedFilesTestCase();
            assertFileList("a1/; a1/b1/; a1/b1/c; a1/b1/c2; a1/b2/; a1/b2/c/; a1/b3; a2/; c");
            assertTrue(FileUtils.recursivelyDeleteFile(getFile("c"), canDelete));
            assertFileList("a1/; a1/b1/; a1/b1/c; a1/b1/c2; a1/b2/; a1/b2/c/; a1/b3; a2/");
            assertTrue(FileUtils.recursivelyDeleteFile(getFile("a1/b1"), canDelete));
            assertFileList("a1/; a1/b2/; a1/b2/c/; a1/b3; a2/");
            assertTrue(FileUtils.recursivelyDeleteFile(getFile("a1"), canDelete));
            assertFileList("a2/");
            assertTrue(FileUtils.recursivelyDeleteFile(getFile("a2"), canDelete));
            assertFileList("");
        }
    }

    // Enable or delete once https://crbug.com/1066733 is fixed.
    @Ignore
    @Test
    public void testRecursivelyDeleteFileWithCanDelete() throws IOException {
        Function<String, Boolean> canDeleteIfEndsWith1 = (String filepath) -> {
            return filepath.endsWith("1");
        };
        Function<String, Boolean> canDeleteIfEndsWith2 = (String filepath) -> {
            return filepath.endsWith("2");
        };

        prepareMixedFilesTestCase();
        assertFileList("a1/; a1/b1/; a1/b1/c; a1/b1/c2; a1/b2/; a1/b2/c/; a1/b3; a2/; c");
        assertTrue(FileUtils.recursivelyDeleteFile(getFile("a1"), canDeleteIfEndsWith1));
        assertFileList("a2/; c");
        assertTrue(FileUtils.recursivelyDeleteFile(getFile("a2"), canDeleteIfEndsWith1));
        assertFileList("a2/; c");
        assertTrue(FileUtils.recursivelyDeleteFile(getFile("a2"), canDeleteIfEndsWith2));
        assertFileList("c");
        assertTrue(FileUtils.recursivelyDeleteFile(getFile("a1"), null));
        assertFileList("");

        prepareMixedFilesTestCase();
        assertFileList("a1/; a1/b1/; a1/b1/c; a1/b1/c2; a1/b2/; a1/b2/c/; a1/b3; a2/; c");
        assertTrue(FileUtils.recursivelyDeleteFile(getFile("a1"), canDeleteIfEndsWith2));
        assertFileList("a1/; a1/b1/; a1/b1/c; a1/b1/c2; a1/b3; a2/; c");
        assertTrue(FileUtils.recursivelyDeleteFile(getFile("c"), canDeleteIfEndsWith2));
        assertFileList("a1/; a1/b1/; a1/b1/c; a1/b1/c2; a1/b3; a2/; c");
        assertTrue(FileUtils.recursivelyDeleteFile(getFile("c"), null));
        assertFileList("a1/; a1/b1/; a1/b1/c; a1/b1/c2; a1/b3; a2/");
        assertTrue(FileUtils.recursivelyDeleteFile(getFile("a2"), canDeleteIfEndsWith2));
        assertFileList("a1/; a1/b1/; a1/b1/c; a1/b1/c2; a1/b3");
        assertTrue(FileUtils.recursivelyDeleteFile(getFile("a1"), null));
        assertFileList("");
    }

    @Test
    public void testBatchDeleteFiles() throws IOException {
        // Batch delete files specified as path names.
        prepareMixedFilesTestCase();
        assertFileList("a1/; a1/b1/; a1/b1/c; a1/b1/c2; a1/b2/; a1/b2/c/; a1/b3; a2/; c");
        FileUtils.batchDeleteFiles(getPathNames("a1/b1", "c", "nonexistent"), null);
        assertFileList("a1/; a1/b2/; a1/b2/c/; a1/b3; a2/");
        // Note that "b2" is not "a1/b2".
        FileUtils.batchDeleteFiles(getPathNames("b2", "a1/b2/c"), null);
        assertFileList("a1/; a1/b2/; a1/b3; a2/");
        FileUtils.batchDeleteFiles(getPathNames("a1/b3", "a1", "a2", "a2", "a1", "a1/b2"), null);
        assertFileList("");

        // Omit testing content URL deletion.
    }

    // TOOD(huangs): Implement testExtractAsset().

    /**
     * Helper to create a byte array filled with arbitrary, non-repeating data.
     *
     * @param size Size of returned array.
     */
    private byte[] createBigByteArray(int size) {
        byte[] ret = new byte[size];
        for (int i = 0; i < size; ++i) {
            int t = i ^ (i >> 8) ^ (i >> 16) ^ (i >> 24); // Prevents repeats.
            ret[i] = (byte) (t & 0xFF);
        }
        return ret;
    }

    @Test
    public void testCopyStream() {
        Function<byte[], Boolean> runCase = (byte[] inputBytes) -> {
            ByteArrayInputStream inputStream = new ByteArrayInputStream(inputBytes);
            ByteArrayOutputStream outputStream = new ByteArrayOutputStream();
            try {
                FileUtils.copyStream(inputStream, outputStream);
            } catch (IOException e) {
                return false;
            }
            byte[] outputBytes = outputStream.toByteArray();
            return Arrays.equals(inputBytes, outputBytes);
        };

        assertTrue(runCase.apply(new byte[] {}));
        assertTrue(runCase.apply(new byte[] {3, 1, 4, 1, 5, 9, 2, 6, 5}));
        assertTrue(runCase.apply("To be or not to be".getBytes()));
        assertTrue(runCase.apply(createBigByteArray(131072))); // 1 << 17.
        assertTrue(runCase.apply(createBigByteArray(119993))); // Prime.
    }

    @Test
    public void testCopyStreamToFile() {
        Function<byte[], Boolean> runCase = (byte[] inputBytes) -> {
            ByteArrayInputStream inputStream = new ByteArrayInputStream(inputBytes);
            ByteArrayOutputStream verifyStream = new ByteArrayOutputStream();
            byte[] fileBytes;
            try {
                File tempFile = temporaryFolder.newFile();
                FileUtils.copyStreamToFile(inputStream, tempFile);
                byte[] buffer = new byte[6543]; // Use weird size.
                try (InputStream is = new FileInputStream(tempFile)) {
                    int amountRead;
                    while ((amountRead = is.read(buffer)) != -1) {
                        verifyStream.write(buffer, 0, amountRead);
                    }
                }
            } catch (IOException e) {
                return false;
            }
            byte[] outputBytes = verifyStream.toByteArray();
            return Arrays.equals(inputBytes, outputBytes);
        };

        assertTrue(runCase.apply(new byte[] {}));
        assertTrue(runCase.apply(new byte[] {3, 1, 4, 1, 5, 9, 2, 6, 5}));
        assertTrue(runCase.apply("To be or not to be".getBytes()));
        assertTrue(runCase.apply(createBigByteArray(131072))); // 1 << 17.
        assertTrue(runCase.apply(createBigByteArray(119993))); // Prime.
    }

    @Test
    public void testReadStream() {
        Function<byte[], Boolean> runCase = (byte[] inputBytes) -> {
            ByteArrayInputStream inputStream = new ByteArrayInputStream(inputBytes);
            byte[] verifyBytes;
            try {
                verifyBytes = FileUtils.readStream(inputStream);
            } catch (IOException e) {
                return false;
            }
            return Arrays.equals(inputBytes, verifyBytes);
        };

        assertTrue(runCase.apply(new byte[] {}));
        assertTrue(runCase.apply(new byte[] {3, 1, 4, 1, 5, 9, 2, 6, 5}));
        assertTrue(runCase.apply("To be or not to be".getBytes()));
        assertTrue(runCase.apply(createBigByteArray(131072))); // 1 << 17.
        assertTrue(runCase.apply(createBigByteArray(119993))); // Prime.
    }

    @Test
    public void testGetUriForFileWithContentUri() {
        // ContentUriUtils needs to be initialized for "content://" URL to work. Use a fake
        // version to avoid dealing with Android innards, and to provide consistent results.
        ContentUriUtils.setFileProviderUtil(new ContentUriUtils.FileProviderUtil() {
            @Override
            public Uri getContentUriFromFile(File file) {
                Uri.Builder builder = new Uri.Builder();
                String fileString = file.toString();
                if (fileString.startsWith("/")) {
                    fileString = fileString.substring(1);
                }
                builder.scheme("content").authority("org.chromium.test");
                for (String path : fileString.split("/")) {
                    builder.appendPath(path);
                }
                return builder.build();
            }
        });

        assertEquals(
                "content://org.chromium.test/", FileUtils.getUriForFile(new File("/")).toString());
        assertEquals("content://org.chromium.test/foo.bar",
                FileUtils.getUriForFile(new File("/foo.bar")).toString());
        assertEquals("content://org.chromium.test/path1/path2/filename.ext",
                FileUtils.getUriForFile(new File("/path1/path2/filename.ext")).toString());
        assertEquals("content://org.chromium.test/../../..",
                FileUtils.getUriForFile(new File("/../../..")).toString());
    }

    @Test
    public void testGetUriForFileWithoutContentUri() {
        // Assumes contentUriUtils.setFileProviderUtil() is not called yet.
        // Only test using absolute path. Otherwise cwd would be included into results.
        assertEquals("file:///", FileUtils.getUriForFile(new File("/")).toString());
        assertEquals("file:///foo.bar", FileUtils.getUriForFile(new File("/foo.bar")).toString());
        assertEquals("file:///path1/path2/filename.ext",
                FileUtils.getUriForFile(new File("/path1/path2/filename.ext")).toString());
        assertEquals("file:///../../..", FileUtils.getUriForFile(new File("/../../..")).toString());
    }

    @Test
    public void testGetExtension() {
        assertEquals("txt", FileUtils.getExtension("foo.txt"));
        assertEquals("txt", FileUtils.getExtension("fOo.TxT"));
        assertEquals("", FileUtils.getExtension(""));
        assertEquals("", FileUtils.getExtension("No_extension"));
        assertEquals("foo_config", FileUtils.getExtension(".foo_conFIG"));
        assertEquals("6", FileUtils.getExtension("a.1.2.3.4.5.6"));
        assertEquals("a1z2_a8z9", FileUtils.getExtension("a....a1z2_A8Z9"));
        assertEquals("", FileUtils.getExtension("dotAtEnd."));
        assertEquals("ext", FileUtils.getExtension("/Full/PATH/To/File.Ext"));
        assertEquals("", FileUtils.getExtension("/Full.PATH/To.File/Extra"));
        assertEquals("", FileUtils.getExtension("../../file"));
        assertEquals("", FileUtils.getExtension("./etc/passwd"));
        assertEquals("", FileUtils.getExtension("////////"));
        assertEquals("", FileUtils.getExtension("........"));
        assertEquals("", FileUtils.getExtension("././././"));
        assertEquals("", FileUtils.getExtension("/./././."));
    }

    // TOOD(huangs): Implement testQueryBitmapFromContentProvider().
}
