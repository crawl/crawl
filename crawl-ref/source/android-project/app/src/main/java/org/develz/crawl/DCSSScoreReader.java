package org.develz.crawl;

final class DCSSScoreReader {
    private DCSSScoreReader() {}

    static native String[] nativeReadScores(String savesDirectory);
}
