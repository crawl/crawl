package org.develz.crawl;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertTrue;

import org.junit.Test;

import java.io.File;
import java.io.FileWriter;
import java.util.Optional;

public class MorgueStatsParserTest {
    private static final String DEATH_MORGUE =
            "Dungeon Crawl Stone Soup version 0.35-a0 (tiles) character file.\n"
            + "\n"
            + "Game seed: 123\n"
            + "\n"
            + "1234 Eivin the Cleaver (level 8, -1/69 HPs)\n"
            + "             Began as a Minotaur Fighter on August 13, 2026.\n"
            + "             Was a Follower of Okawaru.\n"
            + "             Slain by a two-headed ogre\n"
            + "             ... on level 12 of the Dungeon.\n"
            + "             The game lasted 01:02:03 (12345 turns).\n"
            + "\n"
            + "Eivin the Cleaver (Minotaur Fighter)                    Turns: 12345, Time: 01:02:03\n"
            + "\n"
            + "Health: -1/69      AC:  9    Str: 24    XL:     8   Next: 61%\n"
            + "Magic:  7/7        EV: 10    Int:  5    God:    Okawaru [*.....]\n";

    private static final String WIN_MORGUE =
            "Dungeon Crawl Stone Soup version 0.35-a0 (tiles) character file.\n"
            + "\n"
            + "2000000 Eivin the Conqueror (level 27, 200/200 HPs)\n"
            + "             Began as a Gnoll Fighter on August 1, 2026.\n"
            + "             Was the Champion of the Shining One.\n"
            + "             Escaped with the Orb\n"
            + "             ... and 3 runes!\n"
            + "             The game lasted 06:14:19 (76221 turns).\n"
            + "\n"
            + "Eivin the Conqueror (Gnoll Fighter)                    Turns: 76221, Time: 06:14:19\n"
            + "\n"
            + "Health: 200/200    AC: 30    Str: 30    XL:    27\n"
            + "Magic: 30/30      EV: 20    Int: 20    God:    the Shining One [******]\n";

    @Test
    public void recognizesOnlyTimestampedFinalMorgueNames() {
        assertTrue(DCSSMorgueStatsParser.isFinalMorgueName(
                "morgue-Eivin-20260813-185351.txt"));
        assertTrue(DCSSMorgueStatsParser.isFinalMorgueName(
                "morgue-name-with-dashes-20260813-185351.txt"));
        assertFalse(DCSSMorgueStatsParser.isFinalMorgueName("Eivin.txt"));
        assertFalse(DCSSMorgueStatsParser.isFinalMorgueName("morgue-Eivin.txt"));
        assertFalse(DCSSMorgueStatsParser.isFinalMorgueName("morgue-Eivin-20260813-185351.lst"));
        assertFalse(DCSSMorgueStatsParser.isFinalMorgueName("crash-Eivin-20260813-185351.txt"));
    }

    @Test
    public void parsesDeathMorgueFields() throws Exception {
        File morgue = morgueFile("morgue-Eivin-20260813-185351.txt", DEATH_MORGUE);

        Optional<DCSSMorgueGame> parsed = DCSSMorgueStatsParser.parse(morgue);

        assertTrue(parsed.isPresent());
        DCSSMorgueGame game = parsed.get();
        assertEquals(1234L, game.getScore());
        assertEquals("Minotaur", game.getSpecies());
        assertEquals("Fighter", game.getBackground());
        assertEquals("Okawaru", game.getGod());
        assertEquals(8, game.getXl());
        assertEquals("D:12", game.getPlace());
        assertEquals(12345L, game.getTurns());
        assertEquals(3723L, game.getDurationSeconds());
        assertEquals(DCSSMorgueGame.Outcome.LOSS, game.getOutcome());
        assertEquals(0, game.getRunes());
    }

    @Test
    public void parsesWinMorgueFields() throws Exception {
        File morgue = morgueFile("morgue-Eivin-20260813-185352.txt", WIN_MORGUE);

        Optional<DCSSMorgueGame> parsed = DCSSMorgueStatsParser.parse(morgue);

        assertTrue(parsed.isPresent());
        DCSSMorgueGame game = parsed.get();
        assertEquals(2000000L, game.getScore());
        assertEquals("Gnoll", game.getSpecies());
        assertEquals("Fighter", game.getBackground());
        assertEquals("the Shining One", game.getGod());
        assertEquals(27, game.getXl());
        assertEquals("Zot:5", game.getPlace());
        assertEquals(76221L, game.getTurns());
        assertEquals(22459L, game.getDurationSeconds());
        assertEquals(DCSSMorgueGame.Outcome.WIN, game.getOutcome());
        assertEquals(3, game.getRunes());
    }

    @Test
    public void parsesCoreMetadataWithoutInterpretingDeathProse() throws Exception {
        String morgue = DEATH_MORGUE.replace("Slain by a two-headed ogre",
                "A newly worded terminal message from Crawl")
                .replace("Minotaur Fighter", "Vine Stalker Necromancer")
                + "\n# morgue-stats-v1: ktyp=constriction:killer=ball python:ms_species_total=27\n";

        Optional<DCSSMorgueGame> parsed = DCSSMorgueStatsParser.parse(
                morgueFile("morgue-Goofotaacw-20260823-120000.txt", morgue));

        assertTrue(parsed.isPresent());
        assertEquals("Vine Stalker", parsed.get().getSpecies());
        assertEquals("Necromancer", parsed.get().getBackground());
        assertEquals("constriction", parsed.get().getEndText());
        assertEquals(27, parsed.get().getPlayableSpeciesCount());
    }

    @Test
    public void parsesQuitMorgueAsCompletedNonWin() throws Exception {
        String morgue = DEATH_MORGUE.replace("Slain by a two-headed ogre\n"
                + "             ... on level 12 of the Dungeon.",
                "Quit the game on level 12 of the Dungeon.");

        Optional<DCSSMorgueGame> parsed = DCSSMorgueStatsParser.parse(
                morgueFile("morgue-Eivin-20260813-185355.txt", morgue));

        assertTrue(parsed.isPresent());
        assertEquals(DCSSMorgueGame.Outcome.QUIT, parsed.get().getOutcome());
        assertEquals("D:12", parsed.get().getPlace());
    }

    @Test
    public void retainsMorgueWithAnUnknownFutureSpecies() throws Exception {
        String morgue = DEATH_MORGUE.replace("Minotaur Fighter", "Aerial Yak Fighter");

        Optional<DCSSMorgueGame> parsed = DCSSMorgueStatsParser.parse(
                morgueFile("morgue-AerialYak-20260813-185357.txt", morgue));

        assertTrue(parsed.isPresent());
        assertEquals("Aerial Yak", parsed.get().getSpecies());
        assertEquals("Fighter", parsed.get().getBackground());
    }

    @Test
    public void retainsMorgueWithAnUnknownFutureBackground() throws Exception {
        String morgue = DEATH_MORGUE.replace("Minotaur Fighter", "Aerial Yak Temporal Scholar");

        Optional<DCSSMorgueGame> parsed = DCSSMorgueStatsParser.parse(
                morgueFile("morgue-AerialYak-20260813-185358.txt", morgue));

        assertTrue(parsed.isPresent());
        assertEquals("Aerial Yak Temporal Scholar", parsed.get().getSpecies());
        assertEquals("Unknown background", parsed.get().getBackground());
    }

    @Test
    public void parsesGaleCentaurSpecies() throws Exception {
        String morgue = DEATH_MORGUE.replace("Minotaur Fighter", "Gale Centaur Fighter");

        Optional<DCSSMorgueGame> parsed = DCSSMorgueStatsParser.parse(
                morgueFile("morgue-Gale-20260813-185356.txt", morgue));

        assertTrue(parsed.isPresent());
        assertEquals("Gale Centaur", parsed.get().getSpecies());
        assertEquals("Fighter", parsed.get().getBackground());
    }

    @Test
    public void parsesMultiwordSpecies() throws Exception {
        String morgue = DEATH_MORGUE.replace("Minotaur Fighter", "Vine Stalker Earth Elementalist");

        Optional<DCSSMorgueGame> parsed = DCSSMorgueStatsParser.parse(
                morgueFile("morgue-Eivin-20260813-185354.txt", morgue));

        assertTrue(parsed.isPresent());
        assertEquals("Vine Stalker", parsed.get().getSpecies());
        assertEquals("Earth Elementalist", parsed.get().getBackground());
    }

    @Test
    public void rejectsMorgueWithoutFinalSummary() throws Exception {
        File morgue = morgueFile("morgue-Eivin-20260813-185353.txt",
                "Dungeon Crawl Stone Soup version 0.35-a0 character file.\n");

        assertFalse(DCSSMorgueStatsParser.parse(morgue).isPresent());
    }

    @Test
    public void auditReportsSkippedTimestampedFinalMorgues() throws Exception {
        File dir = new File(System.getProperty("java.io.tmpdir"), "morgue-stats-audit");
        dir.mkdirs();
        morgueFileIn(dir, "morgue-Eivin-20260813-185359.txt", DEATH_MORGUE);
        morgueFileIn(dir, "morgue-Eivin-20260813-185400.txt", "incomplete");

        DCSSMorgueLoadResult result = DCSSMorgueStatsParser.loadFinalMorguesWithAudit(dir);

        assertEquals(2, result.getFinalMorgueFiles());
        assertEquals(1, result.getGames().size());
        assertEquals(1, result.getSkippedFiles().size());
    }

    private File morgueFileIn(File dir, String name, String contents) throws Exception {
        File file = new File(dir, name);
        try (FileWriter writer = new FileWriter(file)) {
            writer.write(contents);
        }
        file.deleteOnExit();
        return file;
    }

    private File morgueFile(String name, String contents) throws Exception {
        File dir = new File(System.getProperty("java.io.tmpdir"), "morgue-stats-test");
        dir.mkdirs();
        File file = new File(dir, name);
        try (FileWriter writer = new FileWriter(file)) {
            writer.write(contents);
        }
        file.deleteOnExit();
        return file;
    }
}
