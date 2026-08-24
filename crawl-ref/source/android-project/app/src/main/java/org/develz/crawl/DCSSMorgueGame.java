package org.develz.crawl;

import java.io.File;
import java.time.LocalDateTime;

public final class DCSSMorgueGame {
    public enum Outcome {
        WIN,
        LOSS,
        QUIT
    }

    private final File file;
    private final LocalDateTime timestamp;
    private final long score;
    private final String species;
    private final String background;
    private final String god;
    private final int xl;
    private final String place;
    private final long turns;
    private final long durationSeconds;
    private final int runes;
    private final Outcome outcome;
    private final String endText;
    private final String version;
    private final int playableSpeciesCount;
    private final int playableBackgroundCount;
    private final int availableGodCount;
    private final int playableComboCount;

    public DCSSMorgueGame(File file, LocalDateTime timestamp, long score,
                          String species, String background, String god, int xl,
                          String place, long turns, long durationSeconds, int runes,
                          Outcome outcome, String endText) {
        this(file, timestamp, score, species, background, god, xl, place, turns,
                durationSeconds, runes, outcome, endText, "Unknown version", 0, 0, 0, 0);
    }

    public DCSSMorgueGame(File file, LocalDateTime timestamp, long score,
                          String species, String background, String god, int xl,
                          String place, long turns, long durationSeconds, int runes,
                          Outcome outcome, String endText, String version) {
        this(file, timestamp, score, species, background, god, xl, place, turns,
                durationSeconds, runes, outcome, endText, version, 0, 0, 0, 0);
    }

    public DCSSMorgueGame(File file, LocalDateTime timestamp, long score,
                          String species, String background, String god, int xl,
                          String place, long turns, long durationSeconds, int runes,
                          Outcome outcome, String endText, String version,
                          int playableSpeciesCount, int playableBackgroundCount,
                          int availableGodCount, int playableComboCount) {
        this.file = file;
        this.timestamp = timestamp;
        this.score = score;
        this.species = species;
        this.background = background;
        this.god = god;
        this.xl = xl;
        this.place = place;
        this.turns = turns;
        this.durationSeconds = durationSeconds;
        this.runes = runes;
        this.outcome = outcome;
        this.endText = endText;
        this.version = version;
        this.playableSpeciesCount = playableSpeciesCount;
        this.playableBackgroundCount = playableBackgroundCount;
        this.availableGodCount = availableGodCount;
        this.playableComboCount = playableComboCount;
    }

    public File getFile() { return file; }
    public LocalDateTime getTimestamp() { return timestamp; }
    public long getScore() { return score; }
    public String getSpecies() { return species; }
    public String getBackground() { return background; }
    public String getGod() { return god; }
    public int getXl() { return xl; }
    public String getPlace() { return place; }
    public long getTurns() { return turns; }
    public long getDurationSeconds() { return durationSeconds; }
    public int getRunes() { return runes; }
    public Outcome getOutcome() { return outcome; }
    public String getEndText() { return endText; }
    public String getVersion() { return version; }
    public int getPlayableSpeciesCount() { return playableSpeciesCount; }
    public int getPlayableBackgroundCount() { return playableBackgroundCount; }
    public int getAvailableGodCount() { return availableGodCount; }
    public int getPlayableComboCount() { return playableComboCount; }
}
