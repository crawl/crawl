package org.develz.crawl;

public final class DCSSMorgueRoster {
    private final int playableSpecies;
    private final int playableBackgrounds;
    private final int availableGods;
    private final int playableCombos;

    DCSSMorgueRoster(int playableSpecies, int playableBackgrounds, int availableGods,
                     int playableCombos) {
        this.playableSpecies = playableSpecies;
        this.playableBackgrounds = playableBackgrounds;
        this.availableGods = availableGods;
        this.playableCombos = playableCombos;
    }

    public static DCSSMorgueRoster current() {
        for (String library : DungeonCrawlStoneSoup.LIBRARIES) {
            System.loadLibrary(library);
        }
        int[] counts = nativeCurrentCounts();
        if (counts == null || counts.length != 4) {
            throw new IllegalStateException("Crawl roster counts are unavailable");
        }
        return new DCSSMorgueRoster(counts[0], counts[1], counts[2], counts[3]);
    }

    private static native int[] nativeCurrentCounts();

    int getPlayableSpecies() { return playableSpecies; }
    int getPlayableBackgrounds() { return playableBackgrounds; }
    int getAvailableGods() { return availableGods; }
    int getPlayableCombos() { return playableCombos; }
}
