package org.develz.crawl;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertTrue;

import org.junit.Test;

import java.io.File;
import java.io.FileWriter;
import java.util.Map;

public class DCSSXlogReaderTest {
    @Test
    public void ignoresAmbiguousTimestamps() throws Exception {
        File logfile = new File(System.getProperty("java.io.tmpdir"), "morgue-stats-logfile-ambiguous");
        try (FileWriter writer = new FileWriter(logfile)) {
            writer.write("ktyp=winning:end=20260723120000S\n");
            writer.write("ktyp=quitting:end=20260723120000S\n");
        }

        assertTrue(DCSSXlogReader.readByMorgueTimestamp(logfile).isEmpty());
    }

    @Test
    public void indexesAllStructuredResultsInOnePass() throws Exception {
        File logfile = new File(System.getProperty("java.io.tmpdir"), "morgue-stats-logfile-index");
        try (FileWriter writer = new FileWriter(logfile)) {
            writer.write("ktyp=winning:end=20260723120000S\n");
            writer.write("ktyp=constriction:end=20260723120100S\n");
        }

        Map<String, Map<String, String>> records = DCSSXlogReader.readByMorgueTimestamp(logfile);

        assertEquals(2, records.size());
        assertEquals("winning", records.get("20260823120000").get("ktyp"));
        assertEquals("constriction", records.get("20260823120100").get("ktyp"));
    }
}
