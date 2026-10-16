package org.develz.crawl;

import java.io.File;
import java.util.ArrayList;
import java.util.Collections;
import java.util.Comparator;
import java.util.List;

final class DCSSScoreStatsParser {
    private DCSSScoreStatsParser() {}

    static DCSSMorgueLoadResult loadScores(File savesDirectory) {
        List<DCSSMorgueGame> games = new ArrayList<>();
        List<String> skipped = new ArrayList<>();
        String[] records = DCSSScoreReader.nativeReadScores(savesDirectory.getPath());
        if (records == null)
            return new DCSSMorgueLoadResult(games, 0, skipped);
        for (String record : records) {
            DCSSMorgueGame game = parse(record);
            if (game == null)
                skipped.add(record);
            else
                games.add(game);
        }
        Collections.sort(games, new Comparator<DCSSMorgueGame>() {
            @Override
            public int compare(DCSSMorgueGame left, DCSSMorgueGame right) {
                return right.getTimestamp().compareTo(left.getTimestamp());
            }
        });
        return new DCSSMorgueLoadResult(games, records.length, skipped);
    }

    static DCSSMorgueGame parse(String record) {
        String[] fields = record.split("\\u001f", -1);
        if (fields.length != 13)
            return null;
        try {
            String timestamp = fields[0].substring(0, 14);
            long score = Long.parseLong(fields[1]);
            String species = fields[2];
            String background = fields[3];
            if (timestamp.length() != 14 || species.isEmpty() || background.isEmpty())
                return null;
            String god = fields[4].isEmpty() ? "Atheist" : fields[4];
            int xl = Integer.parseInt(fields[5]);
            String place = fields[6].isEmpty() ? "Unknown" : fields[6];
            long turns = Long.parseLong(fields[7]);
            long duration = Long.parseLong(fields[8]);
            int runes = fields[9].isEmpty() ? 0 : Integer.parseInt(fields[9]);
            String killMethod = fields[10];
            DCSSMorgueGame.Outcome outcome = "winning".equals(killMethod)
                    ? DCSSMorgueGame.Outcome.WIN
                    : ("quitting".equals(killMethod) || "leaving".equals(killMethod))
                    ? DCSSMorgueGame.Outcome.QUIT : DCSSMorgueGame.Outcome.LOSS;
            String endText = fields[11].isEmpty() ? killMethod : fields[11];
            String version = fields[12].isEmpty() ? "Unknown version" : fields[12];
            return new DCSSMorgueGame(new File("scores"), timestamp, score, species, background,
                    god, xl, place, turns, duration, runes, outcome, endText, version);
        } catch (NumberFormatException | IndexOutOfBoundsException ignored) {
            return null;
        }
    }
}
