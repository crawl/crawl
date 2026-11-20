package org.develz.crawl;

import static org.junit.Assert.assertEquals;

import org.junit.Test;

public class ScoreStatsParserTest {
    @Test
    public void parsesCanonicalScoreFieldsWithoutReadingMorgueText() {
        String record = "20260911123456S\u001f12345\u001fMinotaur\u001fFighter\u001fOkawaru"
                + "\u001f12\u001fD:6\u001f1234\u001f600\u001f3\u001fwinning\u001fescaped\u001f0.35";

        DCSSMorgueGame game = DCSSScoreStatsParser.parse(record);

        assertEquals(12345, game.getScore());
        assertEquals("Minotaur", game.getSpecies());
        assertEquals("Fighter", game.getBackground());
        assertEquals(DCSSMorgueGame.Outcome.WIN, game.getOutcome());
        assertEquals("D:6", game.getPlace());
        assertEquals(3, game.getRunes());
    }
}
