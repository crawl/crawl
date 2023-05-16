package org.develz.crawl;

import org.libsdl.app.SDLActivity;

/*
 * SDLActivity for Dungeon Crawl Stone Soup
 */
public class DungeonCrawlStoneSoup extends SDLActivity {

    @Override
    protected String[] getLibraries() {
        return new String[] {
                "c++_shared",
                "SDL2",
                "SDL2_image",
                "mikmod",
                "smpeg2",
                "SDL2_mixer",
                "sqlite",
                "lua",
                "zlib",
                "main"
        };
    }

}
