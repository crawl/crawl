package org.develz.crawl;

import java.io.BufferedReader;
import java.io.File;
import java.io.FileReader;
import java.io.IOException;
import java.time.LocalDateTime;
import java.time.format.DateTimeFormatter;
import java.time.format.DateTimeParseException;
import java.util.ArrayList;
import java.util.Comparator;
import java.util.List;
import java.util.Optional;
import java.util.regex.Matcher;
import java.util.regex.Pattern;

public final class DCSSMorgueStatsParser {
    private static final Pattern FINAL_NAME = Pattern.compile(
            "^morgue-.+-(\\d{8}-\\d{6})\\.txt$");
    private static final Pattern CHARACTER = Pattern.compile(
            "^(\\d+) (.+) \\((.+) (.+)\\)$");
    private static final Pattern BEGAN = Pattern.compile(
            "^Began as a (.+) on .+\\.$");
    private static final Pattern GOD = Pattern.compile(
            "^(?:Was (?:a|an|the) |Worshipped )(?:Follower|Priest|High Priest|Elder|Champion)?"
            + "(?: of )?(.+?)\\.$");
    private static final Pattern LEVEL = Pattern.compile(
            "^.* on level (\\d+) of the Dungeon\\.$");
    private static final Pattern DURATION = Pattern.compile(
            "^The game lasted (\\d+):(\\d{2}):(\\d{2}) \\((\\d+) turns\\)\\.$");
    private static final Pattern XL = Pattern.compile("^Health:.*\\bXL:\\s*(\\d+).*$");
    private static final Pattern RUNES = Pattern.compile("^\\.\\.\\. and (\\d+) runes?!\\s*$");
    private static final String[] SPECIES = {
            "Armataur", "Barachi", "Centaur", "Coglin", "Deep Elf", "Demigod",
            "Demonspawn", "Djinni", "Draconian", "Felid", "Formicid", "Gargoyle",
            "Ghoul", "Gnoll", "Hill Orc", "Human", "Kobold", "Merfolk", "Minotaur",
            "Mountain Dwarf", "Mummy", "Naga", "Octopode", "Oni", "Spriggan",
            "Tengu", "Troll", "Vine Stalker", "Vampire"
    };
    private static final DateTimeFormatter TIMESTAMP =
            DateTimeFormatter.ofPattern("yyyyMMdd-HHmmss");

    private DCSSMorgueStatsParser() {}

    public static boolean isFinalMorgueName(String fileName) {
        return FINAL_NAME.matcher(fileName).matches();
    }

    public static Optional<DCSSMorgueGame> parse(File morgueFile) {
        Matcher nameMatcher = FINAL_NAME.matcher(morgueFile.getName());
        if (!morgueFile.isFile() || !nameMatcher.matches()) {
            return Optional.empty();
        }

        LocalDateTime timestamp;
        try {
            timestamp = LocalDateTime.parse(nameMatcher.group(1), TIMESTAMP);
        } catch (DateTimeParseException e) {
            return Optional.empty();
        }

        long score = -1;
        String species = null;
        String background = null;
        String god = "Atheist";
        int xl = -1;
        String place = null;
        long turns = -1;
        long durationSeconds = -1;
        int runes = 0;
        DCSSMorgueGame.Outcome outcome = null;
        String endText = null;

        try (BufferedReader reader = new BufferedReader(new FileReader(morgueFile))) {
            String line;
            while ((line = reader.readLine()) != null) {
                String trimmed = line.trim();
                Matcher character = CHARACTER.matcher(trimmed);
                if (character.matches()) {
                    score = Long.parseLong(character.group(1));
                    continue;
                }
                Matcher began = BEGAN.matcher(trimmed);
                if (began.matches()) {
                    String combo = began.group(1);
                    for (String candidate : SPECIES) {
                        String prefix = candidate + " ";
                        if (combo.startsWith(prefix)) {
                            species = candidate;
                            background = combo.substring(prefix.length());
                            break;
                        }
                    }
                    continue;
                }
                Matcher godMatcher = GOD.matcher(trimmed);
                if (godMatcher.matches()) {
                    god = godMatcher.group(1);
                    continue;
                }
                Matcher duration = DURATION.matcher(trimmed);
                if (duration.matches()) {
                    durationSeconds = Long.parseLong(duration.group(1)) * 3600
                            + Long.parseLong(duration.group(2)) * 60
                            + Long.parseLong(duration.group(3));
                    turns = Long.parseLong(duration.group(4));
                    continue;
                }
                Matcher xlMatcher = XL.matcher(trimmed);
                if (xlMatcher.matches()) {
                    xl = Integer.parseInt(xlMatcher.group(1));
                    continue;
                }
                Matcher level = LEVEL.matcher(trimmed);
                if (level.matches()) {
                    place = "D:" + level.group(1);
                    continue;
                }
                Matcher runeMatcher = RUNES.matcher(trimmed);
                if (runeMatcher.matches()) {
                    outcome = DCSSMorgueGame.Outcome.WIN;
                    runes = Integer.parseInt(runeMatcher.group(1));
                    place = "Zot:5";
                    endText = "Escaped with the Orb";
                    continue;
                }
                if (trimmed.startsWith("Escaped with the Orb")) {
                    outcome = DCSSMorgueGame.Outcome.WIN;
                    endText = "Escaped with the Orb";
                    continue;
                }
                if (trimmed.startsWith("Quit the game")) {
                    outcome = DCSSMorgueGame.Outcome.QUIT;
                    endText = trimmed;
                    continue;
                }
                if (isDeathSummary(trimmed)) {
                    outcome = DCSSMorgueGame.Outcome.LOSS;
                    endText = trimmed;
                }
            }
        } catch (IOException | NumberFormatException e) {
            return Optional.empty();
        }

        if (score < 0 || species == null || background == null || xl < 0 || turns < 0
                || durationSeconds < 0 || outcome == null || endText == null) {
            return Optional.empty();
        }
        if (place == null) {
            place = "Unknown";
        }
        return Optional.of(new DCSSMorgueGame(morgueFile, timestamp, score, species,
                background, god, xl, place, turns, durationSeconds, runes, outcome, endText));
    }

    private static boolean isDeathSummary(String line) {
        return line.startsWith("Slain by ") || line.startsWith("Mangled by ")
                || line.startsWith("Killed by ") || line.startsWith("Annihilated by ")
                || line.startsWith("Drowned by ") || line.startsWith("Drowned in ")
                || line.startsWith("Burnt to a crisp") || line.startsWith("Killed from afar")
                || line.startsWith("Blown up by ") || line.startsWith("Succumbed to ")
                || line.startsWith("Demolished by ") || line.startsWith("Engulfed by ");
    }

    public static List<DCSSMorgueGame> loadFinalMorgues(File morgueDir) {
        List<DCSSMorgueGame> games = new ArrayList<>();
        File[] files = morgueDir.listFiles();
        if (files == null) {
            return games;
        }
        for (File file : files) {
            parse(file).ifPresent(games::add);
        }
        games.sort(Comparator.comparing(DCSSMorgueGame::getTimestamp).reversed());
        return games;
    }
}
