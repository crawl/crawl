package org.develz.crawl;

import android.os.Bundle;
import android.view.View;
import android.widget.AdapterView;
import android.widget.ArrayAdapter;
import android.widget.Spinner;
import android.widget.TextView;

import androidx.appcompat.app.AppCompatActivity;

import java.io.File;
import java.text.NumberFormat;
import java.util.ArrayList;
import java.util.LinkedHashSet;
import java.util.List;
import java.util.Locale;
import java.util.Set;

public class DCSSMorgueStatsActivity extends AppCompatActivity
        implements AdapterView.OnItemSelectedListener {
    private static final String MORGUE_DIR = "/morgue";
    private DCSSMorgueLoadResult loadResult;
    private TextView text;

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        setContentView(R.layout.morgue_stats);
        findViewById(R.id.morgueStatsBack).setOnClickListener(view -> finish());
        text = findViewById(R.id.morgueStatsText);
        File morgueDir = new File(getExternalFilesDir(null) + MORGUE_DIR);
        loadResult = DCSSMorgueStatsParser.loadFinalMorguesWithAudit(morgueDir);

        Spinner filter = findViewById(R.id.morgueStatsFilter);
        ArrayAdapter<String> adapter = new ArrayAdapter<>(this,
                android.R.layout.simple_spinner_item, filters(loadResult.getGames()));
        adapter.setDropDownViewResource(android.R.layout.simple_spinner_dropdown_item);
        filter.setAdapter(adapter);
        filter.setOnItemSelectedListener(this);
        renderSelection("All games");
    }

    @Override
    public void onItemSelected(AdapterView<?> parent, View view, int position, long id) {
        renderSelection((String) parent.getItemAtPosition(position));
    }

    @Override
    public void onNothingSelected(AdapterView<?> parent) {
    }

    private List<String> filters(List<DCSSMorgueGame> games) {
        Set<String> filters = new LinkedHashSet<>();
        filters.add("All games");
        for (DCSSMorgueGame game : games) {
            filters.add("Species: " + game.getSpecies());
            filters.add("Background: " + game.getBackground());
            filters.add("God: " + game.getGod());
            filters.add("Combo: " + game.getSpecies() + " " + game.getBackground());
        }
        return new ArrayList<>(filters);
    }

    private void renderSelection(String filter) {
        List<DCSSMorgueGame> selected = new ArrayList<>();
        for (DCSSMorgueGame game : loadResult.getGames()) {
            if (matches(filter, game)) {
                selected.add(game);
            }
        }
        text.setText(render(new DCSSMorgueStats(selected), loadResult, filter));
    }

    private static boolean matches(String filter, DCSSMorgueGame game) {
        return filter.equals("All games")
                || filter.equals("Species: " + game.getSpecies())
                || filter.equals("Background: " + game.getBackground())
                || filter.equals("God: " + game.getGod())
                || filter.equals("Combo: " + game.getSpecies() + " " + game.getBackground());
    }

    static String render(DCSSMorgueStats stats, DCSSMorgueLoadResult audit, String filter) {
        StringBuilder text = new StringBuilder("Morgue stats\n\n");
        text.append("Audit: ").append(audit.getFinalMorgueFiles()).append(" timestamped final files · ")
                .append(audit.getGames().size()).append(" counted · ")
                .append(audit.getSkippedFiles().size()).append(" skipped\n");
        if (!audit.getSkippedFiles().isEmpty()) {
            text.append("Skipped: ").append(join(audit.getSkippedFiles(), 3)).append("\n");
        }
        text.append("View: ").append(filter).append("\n\n");
        if (stats.getGames() == 0) {
            return text.append("No final morgues match this view.").toString();
        }
        text.append("Games: ").append(stats.getGames()).append("    Wins: ").append(stats.getWins())
                .append("    Win rate: ").append(percent(stats.getWinRate())).append("\n\n");
        text.append("Best score: ").append(game(stats.getBestScoreGame()))
                .append("\nFastest win (turns): ").append(game(stats.getFastestWinByTurns()))
                .append("\nFastest win (time): ").append(game(stats.getFastestWinByDuration()))
                .append("\nHighest XL: ").append(stats.getHighestXl())
                .append("    Total turns: ").append(number(stats.getTotalTurns()))
                .append("\nTotal time: ").append(duration(stats.getTotalDurationSeconds()))
                .append("    Current / best streak: ").append(stats.getCurrentWinStreak())
                .append(" / ").append(stats.getBestWinStreak()).append("\n");
        text.append("\nAchievements\n")
                .append("Species won: ").append(stats.getDistinctSpeciesWon()).append(" / ")
                .append(stats.getDistinctSpeciesPlayed()).append(" played\n")
                .append("Backgrounds won: ").append(stats.getDistinctBackgroundsWon()).append(" / ")
                .append(stats.getDistinctBackgroundsPlayed()).append(" played\n")
                .append("Combos won: ").append(stats.getDistinctCombosWon()).append(" / ")
                .append(stats.getDistinctCombosPlayed()).append(" played\n");
        appendBreakdowns(text, "Deaths by cause", stats.getDeathBreakdowns(), 8);
        appendBreakdowns(text, "Deaths by place", stats.getDeathPlaceBreakdowns(), 8);
        appendBreakdowns(text, "Timeline by month", stats.getMonthBreakdowns(), 12);
        appendBreakdowns(text, "Games by version", stats.getVersionBreakdowns(), 12);
        appendRecentGames(text, stats.getNewestFirst());
        appendBreakdowns(text, "Wins by species", stats.getSpeciesBreakdowns(), 20);
        appendBreakdowns(text, "Wins by background", stats.getBackgroundBreakdowns(), 20);
        appendBreakdowns(text, "Wins by god", stats.getGodBreakdowns(), 20);
        appendBreakdowns(text, "Wins by combo", stats.getComboBreakdowns(), 20);
        return text.toString();
    }

    private static void appendRecentGames(StringBuilder text, List<DCSSMorgueGame> games) {
        text.append("\nRecent final morgues\n");
        for (int index = 0; index < Math.min(20, games.size()); index++) {
            DCSSMorgueGame game = games.get(index);
            text.append(game.getTimestamp().toLocalDate()).append(" · ").append(game.getOutcome())
                    .append(" · ").append(game.getSpecies()).append(" ").append(game.getBackground())
                    .append(" · XL ").append(game.getXl()).append(" · ").append(game.getPlace())
                    .append(" · ").append(number(game.getScore())).append("\n");
        }
    }

    private static void appendBreakdowns(StringBuilder text, String heading,
                                         List<DCSSMorgueBreakdown> rows, int limit) {
        if (rows.isEmpty()) {
            return;
        }
        text.append("\n").append(heading).append("\n");
        for (int index = 0; index < Math.min(limit, rows.size()); index++) {
            DCSSMorgueBreakdown row = rows.get(index);
            text.append(row.getLabel()).append(": ").append(row.getWins()).append(" W · ")
                    .append(row.getGames()).append(" G · ").append(percent(row.getWinRate()))
                    .append(" · best ").append(number(row.getBestScore())).append("\n");
        }
    }

    private static String game(DCSSMorgueGame game) {
        if (game == null) {
            return "No win yet";
        }
        return number(game.getScore()) + " · " + game.getSpecies() + " " + game.getBackground()
                + " · " + game.getTurns() + " turns · " + duration(game.getDurationSeconds());
    }

    private static String join(List<String> values, int limit) {
        return joinValues(values.subList(0, Math.min(limit, values.size())), values.size());
    }

    private static String joinValues(List<String> values, int total) {
        StringBuilder result = new StringBuilder();
        for (int index = 0; index < values.size(); index++) {
            if (index > 0) result.append(", ");
            result.append(values.get(index));
        }
        if (total > values.size()) result.append(" …");
        return result.toString();
    }

    private static String number(long value) { return NumberFormat.getIntegerInstance(Locale.getDefault()).format(value); }
    private static String percent(double value) { return String.format(Locale.getDefault(), "%.1f%%", value); }
    private static String duration(long seconds) { return String.format(Locale.getDefault(), "%02d:%02d:%02d", seconds / 3600, (seconds / 60) % 60, seconds % 60); }
}
