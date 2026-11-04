package org.develz.crawl;

import java.util.ArrayList;
import java.util.Collections;
import java.util.List;

public final class DCSSMorgueLoadResult {
    private final List<DCSSMorgueGame> games;
    private final int finalMorgueFiles;
    private final List<String> skippedFiles;

    public DCSSMorgueLoadResult(List<DCSSMorgueGame> games, int finalMorgueFiles,
                                List<String> skippedFiles) {
        this.games = Collections.unmodifiableList(new ArrayList<>(games));
        this.finalMorgueFiles = finalMorgueFiles;
        this.skippedFiles = Collections.unmodifiableList(new ArrayList<>(skippedFiles));
    }

    public List<DCSSMorgueGame> getGames() { return games; }
    public int getFinalMorgueFiles() { return finalMorgueFiles; }
    public List<String> getSkippedFiles() { return skippedFiles; }
}
