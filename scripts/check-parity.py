#!/usr/bin/env python3
"""Cross-platform parity gate for GlowDeck GameDay.

Checks shared, user-visible behavior and common NHL team colors across
ESP32, Fire TV and local browser demo. Hardware-specific features and the
separately hosted Lovable demo still need acceptance testing.
"""
import re
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]


def read(name):
    return (ROOT / name).read_text(encoding="utf-8")


def require(ok, description):
    if not ok:
        raise AssertionError(description)
    print("PASS " + description)


def colors_cpp(s):
    return dict((team, rgb.upper()) for team, rgb in
                re.findall(r'\{"([A-Z]{3})",0x([0-9a-fA-F]{6})\}', s))


def colors_java(s):
    return dict((team, rgb.upper()) for team, rgb in
                re.findall(r'case "([A-Z]{3})":\s*return 0x([0-9a-fA-F]{6})', s))


def main():
    cpp = read("src/GameCore.cpp")
    java = read("firetv/app/src/main/java/com/gkowdeck/gameday/firetv/NhlApi.java")
    esp = read("src/GoalEngine.cpp")
    fire = read("firetv/app/src/main/java/com/gkowdeck/gameday/firetv/GameDayService.java")
    selector = read("firetv/app/src/main/java/com/gkowdeck/gameday/firetv/CelebrationSelection.java")
    ui = read("src/Ui.cpp")
    screen = read("firetv/app/src/main/java/com/gkowdeck/gameday/firetv/MainActivity.java")
    nhl = read("src/NhlClient.cpp")
    web = read("simulator/web/index.html")
    a, b = colors_cpp(cpp), colors_java(java)
    require(len(a) == 32 and a == b, "ESP32 and Fire TV 32 NHL team colors match")
    require(all(v in ui for v in ['"BOTH"', 'CelebrationMode::Away',
                                   'CelebrationMode::Home', 'CelebrationMode::Both']),
            "ESP32 touchscreen shows Away/Home/Both")
    require(all(v in screen for v in ['"BOTH TEAMS"', '"AWAY: "', '"HOME: "']),
            "Fire TV interface shows Away/Home/Both")
    require('case BOTH' in selector and 'CelebrationMode::Both' in esp,
            "Both-mode engine accepts either valid scoring team")
    require('pollGoals(' in nhl and '/play-by-play' in nhl and
            'eventOwnerTeamId' in nhl and 'eventId' in nhl and
            'NhlApi.plays(' in fire,
            "Both firmware paths detect NHL goal event IDs")
    require('8000U' in esp and '},8,TimeUnit.SECONDS' in fire and
            '},8000)' in web,
            "Celebrations last eight seconds across controllers and demo")
    require('20000U' in esp and '20_000' in
            read("firetv/app/src/main/java/com/gkowdeck/gameday/firetv/MonitorRules.java"),
            "Both controllers use 20-second cooldown")
    require('PBP_INTERVAL_MS = 25000' in read("src/main.cpp") and
            '25, 25, TimeUnit.SECONDS' in fire,
            "Both controllers poll play-by-play at approximately 25 seconds")
    require('delaySeconds_ = seconds > 90 ? 90 : seconds' in esp and
            'Math.min(90, requested)' in
            read("firetv/app/src/main/java/com/gkowdeck/gameday/firetv/MonitorRules.java"),
            "Both controllers cap user TV delay to 90 seconds")
    require('putUChar((String("m")+uiState.selectedGameId)' in read("src/main.cpp")
            and 'celebrationMode."+g.id' in screen,
            "Both platforms remember selection per NHL game ID")
    require('persistAutoBackup' in read("src/WledClient.cpp") and
            'restoreSavedState()' in fire,
            "Both controllers journal and restore WLED state")
    require("chooseMode('both')" in web and "s.mode==='both'" in web and
            "v0.3" in web,
            "Local browser demo includes v0.3 Both mode")
    print("ALL CHECKS PASSED: cross-platform parity contract v0.3")


if __name__ == "__main__":
    main()
