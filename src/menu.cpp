/*
    Copyright 2022 Hydr8gon

    This file is part of Project DS.

    Project DS is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published
    by the Free Software Foundation, either version 3 of the License,
    or (at your option) any later version.

    Project DS is distributed in the hope that it will be useful, but
    WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
    General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with Project DS. If not, see <https://www.gnu.org/licenses/>.
*/

#include <algorithm>
#include <dirent.h>
#include <vector>

#include <nds.h>

#include "menu.h"
#include "audio.h"
#include "game.h"

static const char a[] = {' ', '>'};

static const std::string ends[] =
{
    "_easy.dsc",
    "_normal.dsc",
    "_hard.dsc",
    "_extreme.dsc",
    "_extreme_1.dsc"
};

static std::string songs[1000];
static std::vector<std::string> charts[5];

void menuInit()
{
    // Scan database files (.txt) for English song names
    if (DIR *dir = opendir("/project-ds"))
    {
        while (dirent *entry = readdir(dir))
        {
            std::string name = (std::string)"/project-ds/" + entry->d_name;
            if (name.substr(name.length() - 4) == ".txt")
            {
                if (FILE *file = fopen(name.c_str(), "r"))
                {
                    char line[512];
                    while (fgets(line, 512, file))
                    {
                        std::string str = line;
                        if (str.length() > 20 && str.substr(7, 12) == "song_name_en")
                        {
                            // Set a song name, stripped of invalid characters and trimmed to fit on-screen
                            std::string &song = songs[std::stoi(str.substr(3, 3))];
                            song = str.substr(20);
                            song.erase(remove_if(song.begin(), song.end(),
                                [](const char &c) { return c >= 128; }), song.end());
                            song = song.substr(0, 31);
                        }
                    }

                    fclose(file);
                }
            }
        }

        closedir(dir);
    }

    // Scan chart files (.dsc) and build song ID lists for each difficulty
    if (DIR *dir = opendir("/project-ds/dsc"))
    {
        while (dirent *entry = readdir(dir))
        {
            std::string name = entry->d_name;
            if (name.length() > 6 && name.substr(0, 3) == "pv_" && name[3] >= '0' && name[3] <= '9' &&
                name[4] >= '0' && name[4] <= '9' && name[5] >= '0' && name[5] <= '9')
            {
                for (int i = 0; i < 5; i++)
                {
                    if (name.substr(6) == ends[i])
                    {
                        charts[i].push_back(name.substr(3, 3));
                        break;
                    }
                }
            }
        }

        closedir(dir);
    }

    for (int i = 0; i < 5; i++)
        sort(charts[i].begin(), charts[i].end());
}

void songList()
{
    // Clear any sprites that were set
    oamClear(&oamSub, 0, 0);
    oamUpdate(&oamSub);

    // Ensure there are files present
    if (charts[0].empty() && charts[1].empty() && charts[2].empty() && charts[3].empty() && charts[4].empty())
    {
        consoleClear();
        printf("No DSC files found.\n");
        printf("Place them in '/project-ds/dsc'.\n");

        // Do nothing since there are no files to show
        while (true)
            swiWaitForVBlank();
    }

    static size_t difficulty = 1;
    static size_t selection = 0;
    uint8_t frames = 1;

    // Show the file browser
    while (true)
    {
        // Calculate the offset to display the files from
        size_t offset = 0;
        if (charts[difficulty].size() > 21)
        {
            if (selection >= charts[difficulty].size() - 10)
                offset = charts[difficulty].size() - 21;
            else if (selection > 10)
                offset = selection - 10;
        }

        consoleClear();

        // Display a section of files around the current selection
        for (size_t i = offset; i < offset + std::min(charts[difficulty].size(), 21U); i++)
        {
            std::string song = songs[std::stoi(charts[difficulty][i])];
            if (song == "") song = "pv_" + charts[difficulty][i];
            printf("\x1b[%d;0H%c%s\n", i - offset, a[i == selection], song.c_str());
        }

        // Display the difficulty tabs
        printf("\x1b[22;0H%cEasy %cNormal %cHard %cExtrm %cExEx", a[difficulty == 0],
            a[difficulty == 1], a[difficulty == 2], a[difficulty == 3], a[difficulty == 4]);

        uint16_t down = 0;
        uint16_t held = 0;
        keysDown();

        // Wait for button input
        while (!(down & (KEY_A | KEY_LEFT | KEY_RIGHT)) && !(held & (KEY_UP | KEY_DOWN)))
        {
            scanKeys();
            down = keysDown();
            held = keysHeld();

            // On the first frame inputs are released, start playing a song preview
            if (!(held & (KEY_UP | KEY_DOWN)) && frames > 0)
            {
                frames = 0;
                std::string name = "/project-ds/pcm/pv_" + charts[difficulty][selection] + ".pcm";
                playSong(name);
            }

            updateSong();
            swiWaitForVBlank();
        }

        stopSong();

        if (down & KEY_A)
        {
            // Select the current file and proceed to load it
            if (!charts[difficulty].empty())
            {
                consoleClear();
                break;
            }
        }
        else if (down & KEY_LEFT)
        {
            // Move the difficulty selection left with wraparound
            if (frames++ == 0)
            {
                selection = 0;
                if (difficulty-- == 0)
                    difficulty = 4;
            }
        }
        else if (down & KEY_RIGHT)
        {
            // Move the difficulty selection right with wraparound
            if (frames++ == 0)
            {
                selection = 0;
                if (++difficulty == 5)
                    difficulty = 0;
            }
        }
        else if (held & KEY_UP)
        {
            // Decrement the current selection with wraparound, continuously after 30 frames
            if ((frames > 30 || frames++ == 0) && selection-- == 0)
                selection = charts[difficulty].size() - 1;
        }
        else if (held & KEY_DOWN)
        {
            // Increment the current selection with wraparound, continuously after 30 frames
            if ((frames > 30 || frames++ == 0) && ++selection == charts[difficulty].size())
                selection = 0;
        }
    }

    // Infer names for all the files that might need to be accessed
    std::string dscName = "/project-ds/dsc/pv_" + charts[difficulty][selection] + ends[difficulty];
    std::string oggName = "/project-ds/ogg/pv_" + charts[difficulty][selection] + ".ogg";
    std::string pcmName = "/project-ds/pcm/pv_" + charts[difficulty][selection] + ".pcm";

    // Try to convert the song if it wasn't found
    FILE *song = fopen(pcmName.c_str(), "rb");
    bool retry = (!song && convertSong(oggName, pcmName));
    fclose(song);

    loadChart(dscName, pcmName, difficulty, retry);
}

void retryMenu()
{
    stopSong();

    size_t selection = 0;
    uint8_t frames = 1;

    while (true)
    {
        // Draw the menu items
        printf("\x1b[10;13H%cRetry", a[selection == 0]);
        printf("\x1b[12;6H%cReturn to Song List", a[selection == 1]);

        uint16_t down = 0;
        uint16_t held = 0;
        keysDown();

        // Wait for button input
        while (!(down & KEY_A) && !(held & (KEY_UP | KEY_DOWN)))
        {
            scanKeys();
            down = keysDown();
            held = keysHeld();
            if (!(held & (KEY_UP | KEY_DOWN)))
                frames = 0;
            swiWaitForVBlank();
        }

        if (down & KEY_A)
        {
            // Handle the selected item
            switch (selection)
            {
                case 1: // Return to Song List
                    songList();
                case 0: // Retry
                    gameReset();
                    break;
            }

            // Return to the game
            consoleClear();
            return;
        }
        else if (held & KEY_UP)
        {
            // Decrement the current selection with wraparound, continuously after 30 frames
            if ((frames > 30 || frames++ == 0) && selection-- == 0)
                selection = 2 - 1;
        }
        else if (held & KEY_DOWN)
        {
            // Increment the current selection with wraparound, continuously after 30 frames
            if ((frames > 30 || frames++ == 0) && ++selection == 2)
                selection = 0;
        }
    }
}
