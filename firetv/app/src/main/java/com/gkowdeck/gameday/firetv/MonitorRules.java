package com.gkowdeck.gameday.firetv;

/** Pure safety rules shared by the background monitor and desktop JVM tests. */
final class MonitorRules {
    static final long COOLDOWN_MS = 20_000;
    static final long STALE_FEED_MS = 75_000;
    static boolean needsBaseline(long now, long lastSuccessfulPoll) {
        return lastSuccessfulPoll <= 0 || now < lastSuccessfulPoll || now - lastSuccessfulPoll > STALE_FEED_MS;
    }
    static boolean shouldCelebrate(boolean armed, boolean gameLive, boolean restoreOutstanding,
                                   boolean sceneBusy, int eventOwner, int favoriteOwner,
                                   long now, long lastEffectAt) {
        return armed && gameLive && !restoreOutstanding && !sceneBusy &&
               eventOwner > 0 && eventOwner == favoriteOwner &&
               (lastEffectAt <= 0 || (now >= lastEffectAt && now - lastEffectAt >= COOLDOWN_MS));
    }
    static int delaySeconds(int requested) { return Math.max(0, Math.min(90, requested)); }
}
