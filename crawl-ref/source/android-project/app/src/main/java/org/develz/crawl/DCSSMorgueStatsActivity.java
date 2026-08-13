package org.develz.crawl;

import android.os.Bundle;
import android.widget.TextView;

import androidx.appcompat.app.AppCompatActivity;

import java.io.File;
import java.text.NumberFormat;
import java.util.List;
import java.util.Locale;

public class DCSSMorgueStatsActivity extends AppCompatActivity {
    private static final String MORGUE_DIR = "/morgue";

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        setContentView(R.layout.morgue_stats);
        findViewById(R.id.morgueStatsBack).setOnClickListener(view -> finish());

        TextView text = findViewById(R.id.morgueStatsText);
        File morgueDir = new File(getExternalFilesDir(null) + MORGUE_DIR);
        text.setText(render(new DCSSMorgueStats(DCSSMorgueStatsParser.loadFinalMorgues(morgueDir))));
    }

    static String render(DCSSMorgueStats stats) {
        if (stats.getGames() == 0) {
            return "Morgue stats\n\nNo final morgues found.\n\n"
                    + "Only timestamped final morgues currently kept on this device are counted.";
        }

        StringBuilder text = new StringBuilder("Morgue stats\n\n");
        text.append("Final morgues currently kept on this device\n\n");
        text.append("Games: ").append(stats.getGames())
                .append("    Wins: ").append(stats.getWins())
                .append("    Win rate: ").append(percent(stats.getWinRate())).append("\n\n");
        text.append("Best score: ").append(number(stats.getBestScore()))
                .append("\nTotal score: ").append(number(stats.getTotalScore()))
                .append("\nHighest XL: ").append(stats.getHighestXl())
                .append("\nTotal turns: ").append(number(stats.getTotalTurns()))
                .append("\nTotal time: ").append(duration(stats.getTotalDurationSeconds()))
                .append("\nCurrent win streak: ").append(stats.getCurrentWinStreak())
                .append("\nBest win streak: ").append(stats.getBestWinStreak())
                .append("\nRunes on wins: ").append(stats.getTotalWinRunes())
                .append("    Best: ").append(stats.getBestRunes()).append("\n");
        appendRecentGames(text, stats.getNewestFirst());
        appendBreakdowns(text, "Wins by species", stats.getSpeciesBreakdowns());
        appendBreakdowns(text, "Wins by background", stats.getBackgroundBreakdowns());
        appendBreakdowns(text, "Wins by god", stats.getGodBreakdowns());
        appendBreakdowns(text, "Wins by combo", stats.getComboBreakdowns());
        return text.toString();
    }

    private static void appendRecentGames(StringBuilder text, List<DCSSMorgueGame> games) {
        text.append("\nRecent final morgues\n");
        int shown = Math.min(20, games.size());
        for (int index = 0; index < shown; index++) {
            DCSSMorgueGame game = games.get(index);
            text.append(game.getTimestamp().toLocalDate()).append(" · ")
                    .append(game.getOutcome()).append(" · ")
                    .append(game.getSpecies()).append(" ").append(game.getBackground())
                    .append(" · XL ").append(game.getXl())
                    .append(" · ").append(game.getPlace())
                    .append(" · ").append(number(game.getScore())).append("\n");
        }
    }

    private static void appendBreakdowns(StringBuilder text, String heading,
                                         List<DCSSMorgueBreakdown> rows) {
        text.append("\n").append(heading).append("\n");
        for (DCSSMorgueBreakdown row : rows) {
            text.append(row.getLabel()).append(": ")
                    .append(row.getWins()).append(" W · ")
                    .append(row.getGames()).append(" G · ")
                    .append(percent(row.getWinRate())).append("\n");
        }
    }

    private static String number(long value) {
        return NumberFormat.getIntegerInstance(Locale.getDefault()).format(value);
    }

    private static String percent(double value) {
        return String.format(Locale.getDefault(), "%.1f%%", value);
    }

    private static String duration(long seconds) {
        return String.format(Locale.getDefault(), "%02d:%02d:%02d", seconds / 3600,
                (seconds / 60) % 60, seconds % 60);
    }
}
