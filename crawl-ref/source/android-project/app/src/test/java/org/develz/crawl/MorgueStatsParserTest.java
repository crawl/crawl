package org.develz.crawl;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertTrue;

import org.junit.Test;

import java.io.File;
import java.io.FileWriter;

public class MorgueStatsParserTest {
    private static final String MORGUE =
            "Dungeon Crawl Stone Soup version 0.35-a0 (tiles) character file.\n"
            + "1234 Eivin the Cleaver (level 8, -1/69 HPs)\n"
            + "             Began as a Minotaur Fighter on August 13, 2026.\n"
            + "             Was a Follower of Okawaru.\n"
            + "             Slain by a two-headed ogre\n"
            + "             ... on level 12 of the Dungeon.\n"
            + "             The game lasted 01:02:03 (12345 turns).\n"
            + "Health: -1/69      AC: 9    Str: 24    XL: 8\n";

    @Test
    public void recognizesOnlyTimestampedFinalMorgueNames() {
        assertTrue(DCSSMorgueStatsParser.isFinalMorgueName(
                "morgue-name-with-dashes-20260813-185351.txt"));
        assertFalse(DCSSMorgueStatsParser.isFinalMorgueName("morgue-Eivin.txt"));
        assertFalse(DCSSMorgueStatsParser.isFinalMorgueName("morgue-Eivin-20260813-185351.lst"));
        assertFalse(DCSSMorgueStatsParser.isFinalMorgueName("crash-Eivin-20260813-185351.txt"));
    }

    @Test
    public void usesLogfileOutcomeAndKeepsUnknownCombos() throws Exception {
        String morgue = MORGUE.replace("Minotaur Fighter", "Aerial Yak Fighter")
                .replace("Slain by a two-headed ogre", "New terminal prose");
        File dir = new File(System.getProperty("java.io.tmpdir"), "morgue-stats-xlog");
        dir.mkdirs();
        morgueFile(dir, "morgue-Eivin-20260823-120000.txt", morgue);
        File logfile = new File(dir, "logfile");
        try (FileWriter writer = new FileWriter(logfile)) {
            writer.write("ktyp=constriction:end=20260723120000S\n");
        }

        DCSSMorgueLoadResult result = DCSSMorgueStatsParser.loadFinalMorguesWithAudit(
                dir, logfile);

        assertEquals(1, result.getGames().size());
        DCSSMorgueGame game = result.getGames().get(0);
        assertEquals("Aerial Yak", game.getSpecies());
        assertEquals("Fighter", game.getBackground());
        assertEquals(DCSSMorgueGame.Outcome.LOSS, game.getOutcome());
        assertEquals("constriction", game.getEndText());
    }

    @Test
    public void keepsQuitMorguesCompletedWithoutALogfile() throws Exception {
        String morgue = MORGUE.replace("Slain by a two-headed ogre\n"
                + "             ... on level 12 of the Dungeon.",
                "Quit the game on level 12 of the Dungeon.");

        DCSSMorgueGame game = DCSSMorgueStatsParser.parse(
                morgueFile(new File(System.getProperty("java.io.tmpdir"), "morgue-stats-quit"),
                        "morgue-Eivin-20260813-185355.txt", morgue));

        assertTrue(game != null);
        assertEquals(DCSSMorgueGame.Outcome.QUIT, game.getOutcome());
    }

    @Test
    public void auditListsMalformedTimestampedMorgues() throws Exception {
        File dir = new File(System.getProperty("java.io.tmpdir"), "morgue-stats-audit");
        dir.mkdirs();
        morgueFile(dir, "morgue-Eivin-20260813-185359.txt", MORGUE);
        morgueFile(dir, "morgue-Eivin-20260813-185400.txt", "incomplete");

        DCSSMorgueLoadResult result = DCSSMorgueStatsParser.loadFinalMorguesWithAudit(dir);

        assertEquals(2, result.getFinalMorgueFiles());
        assertEquals(1, result.getGames().size());
        assertEquals(1, result.getSkippedFiles().size());
    }

    private File morgueFile(File dir, String name, String contents) throws Exception {
        dir.mkdirs();
        File file = new File(dir, name);
        try (FileWriter writer = new FileWriter(file)) {
            writer.write(contents);
        }
        file.deleteOnExit();
        return file;
    }
}
