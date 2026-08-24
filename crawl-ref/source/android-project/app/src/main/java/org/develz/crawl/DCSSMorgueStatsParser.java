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
    private static final Pattern VERSION = Pattern.compile(
            "^Dungeon Crawl Stone Soup version (.+?)(?: \\(.*)? character file\\.$");
    private static final Pattern MORGUE_STATS = Pattern.compile(
            "^# morgue-stats-v1: (.*)$");
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
    private static final String[] BACKGROUNDS = {
            "Earth Elementalist", "Fire Elementalist", "Air Elementalist", "Ice Elementalist",
            "Abyssal Knight", "Cinder Acolyte", "Chaos Knight", "Death Knight", "Hedge Wizard",
            "Shapeshifter", "Forgewright", "Necromancer", "Hexslinger", "Alchemist", "Artificer",
            "Berserker", "Enchanter", "Gladiator", "Conjurer", "Summoner", "Wanderer", "Brigand",
            "Fighter", "Stalker", "Delver", "Healer", "Hunter", "Jester", "Priest", "Reaver",
            "Warper", "Skald", "Monk"
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

        String version = "Unknown version";
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
        String killMethod = null;
        int playableSpeciesCount = 0;

        try (BufferedReader reader = new BufferedReader(new FileReader(morgueFile))) {
            String line;
            while ((line = reader.readLine()) != null) {
                String trimmed = line.trim();
                Matcher metadata = MORGUE_STATS.matcher(trimmed);
                if (metadata.matches()) {
                    killMethod = xlogField(metadata.group(1), "ktyp");
                    String speciesCount = xlogField(metadata.group(1), "ms_species_total");
                    if (speciesCount != null) {
                        playableSpeciesCount = Integer.parseInt(speciesCount);
                    }
                    continue;
                }
                Matcher versionMatcher = VERSION.matcher(trimmed);
                if (versionMatcher.matches()) {
                    version = versionMatcher.group(1);
                    continue;
                }
                Matcher character = CHARACTER.matcher(trimmed);
                if (character.matches()) {
                    score = Long.parseLong(character.group(1));
                    continue;
                }
                Matcher began = BEGAN.matcher(trimmed);
                if (began.matches()) {
                    String combo = began.group(1);
                    for (String candidate : BACKGROUNDS) {
                        String suffix = " " + candidate;
                        if (combo.endsWith(suffix)) {
                            species = combo.substring(0, combo.length() - suffix.length());
                            background = candidate;
                            break;
                        }
                    }
                    if (species == null) {
                        species = combo;
                        background = "Unknown background";
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
                if (trimmed.startsWith("Quit the game")) {
                    outcome = DCSSMorgueGame.Outcome.QUIT;
                    endText = trimmed;
                    Matcher level = LEVEL.matcher(trimmed);
                    if (level.matches()) {
                        place = "D:" + level.group(1);
                    }
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
            }
        } catch (IOException | NumberFormatException e) {
            return Optional.empty();
        }

        if (killMethod != null) {
            outcome = outcomeForKillMethod(killMethod);
            endText = killMethod;
        }
        if (score < 0 || species == null || background == null || xl < 0 || turns < 0
                || durationSeconds < 0) {
            return Optional.empty();
        }
        if (outcome == null) {
            outcome = DCSSMorgueGame.Outcome.LOSS;
            endText = "Unknown death cause";
        }
        if (place == null) {
            place = "Unknown";
        }
        return Optional.of(new DCSSMorgueGame(morgueFile, timestamp, score, species,
                background, god, xl, place, turns, durationSeconds, runes, outcome, endText,
                version, playableSpeciesCount));
    }

    private static String xlogField(String fields, String name) {
        for (String field : fields.split(":")) {
            int equals = field.indexOf('=');
            if (equals > 0 && field.substring(0, equals).equals(name)) {
                return field.substring(equals + 1);
            }
        }
        return null;
    }

    private static DCSSMorgueGame.Outcome outcomeForKillMethod(String killMethod) {
        if ("winning".equals(killMethod)) {
            return DCSSMorgueGame.Outcome.WIN;
        }
        if ("quitting".equals(killMethod) || "leaving".equals(killMethod)) {
            return DCSSMorgueGame.Outcome.QUIT;
        }
        return DCSSMorgueGame.Outcome.LOSS;
    }

    public static List<DCSSMorgueGame> loadFinalMorgues(File morgueDir) {
        return loadFinalMorguesWithAudit(morgueDir).getGames();
    }

    public static DCSSMorgueLoadResult loadFinalMorguesWithAudit(File morgueDir) {
        List<DCSSMorgueGame> games = new ArrayList<>();
        List<String> skipped = new ArrayList<>();
        File[] files = morgueDir.listFiles();
        if (files == null) {
            return new DCSSMorgueLoadResult(games, 0, skipped);
        }
        int finalMorgues = 0;
        for (File file : files) {
            if (!isFinalMorgueName(file.getName())) {
                continue;
            }
            finalMorgues++;
            Optional<DCSSMorgueGame> game = parse(file);
            if (game.isPresent()) {
                games.add(game.get());
            } else {
                skipped.add(file.getName());
            }
        }
        games.sort(Comparator.comparing(DCSSMorgueGame::getTimestamp).reversed());
        return new DCSSMorgueLoadResult(games, finalMorgues, skipped);
    }
}
