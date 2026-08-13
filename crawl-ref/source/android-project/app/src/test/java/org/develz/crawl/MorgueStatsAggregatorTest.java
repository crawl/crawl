package org.develz.crawl;

import static org.junit.Assert.assertEquals;

import org.junit.Test;

import java.io.File;
import java.time.LocalDateTime;
import java.util.Arrays;

public class MorgueStatsAggregatorTest {
    @Test
    public void aggregatesWinsBySpeciesAndBackground() {
        DCSSMorgueGame loss = game("2026-08-01T12:00:00", "Minotaur", "Fighter",
                1200, 8, DCSSMorgueGame.Outcome.LOSS, 0);
        DCSSMorgueGame firstWin = game("2026-08-02T12:00:00", "Minotaur", "Fighter",
                2000000, 27, DCSSMorgueGame.Outcome.WIN, 3);
        DCSSMorgueGame secondWin = game("2026-08-03T12:00:00", "Gnoll", "Fighter",
                1500000, 27, DCSSMorgueGame.Outcome.WIN, 3);

        DCSSMorgueStats stats = new DCSSMorgueStats(Arrays.asList(secondWin, firstWin, loss));

        assertEquals(3, stats.getGames());
        assertEquals(2, stats.getWins());
        assertEquals(2, stats.getCurrentWinStreak());
        assertEquals(2, stats.getBestWinStreak());
        assertEquals(2, stats.getSpeciesBreakdowns().size());
        assertEquals("Minotaur", stats.getSpeciesBreakdowns().get(0).getLabel());
        assertEquals(1, stats.getSpeciesBreakdowns().get(0).getWins());
        assertEquals(2, stats.getSpeciesBreakdowns().get(0).getGames());
        assertEquals("Fighter", stats.getBackgroundBreakdowns().get(0).getLabel());
        assertEquals(2, stats.getBackgroundBreakdowns().get(0).getWins());
        assertEquals(3, stats.getBackgroundBreakdowns().get(0).getGames());
    }

    private DCSSMorgueGame game(String timestamp, String species, String background,
                                long score, int xl, DCSSMorgueGame.Outcome outcome, int runes) {
        return new DCSSMorgueGame(new File("morgue-test.txt"), LocalDateTime.parse(timestamp),
                score, species, background, "Okawaru", xl, "D:12", 1000, 600,
                runes, outcome, "test");
    }
}
