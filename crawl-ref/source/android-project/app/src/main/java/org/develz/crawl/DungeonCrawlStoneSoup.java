package org.develz.crawl;

import org.libsdl.app.SDLActivity;

/*
 * SDLActivity for Dungeon Crawl Stone Soup
 */
public class DungeonCrawlStoneSoup extends SDLActivity {

    @Override
    protected String[] getLibraries() {
        return new String[] {
                "SDL2",
                "SDL2_image",
                "SDL2_mixer",
                "mikmod",
                "c++_shared",
                "smpeg2",
                "sqlite",
                "lua",
                "zlib",
                "main"
        };
    }

}
