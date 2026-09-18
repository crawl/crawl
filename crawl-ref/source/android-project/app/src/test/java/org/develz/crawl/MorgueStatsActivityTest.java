package org.develz.crawl;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertTrue;

import org.junit.Test;

import java.util.Arrays;
import java.util.Collections;

public class MorgueStatsActivityTest {
    @Test
    public void keepsAuditOutOfTheNormalStatsView() {
        DCSSMorgueStats stats = new DCSSMorgueStats(Collections.emptyList(),
                new DCSSMorgueRoster(27, 33, 27, 880));
        DCSSMorgueLoadResult audit = new DCSSMorgueLoadResult(Collections.emptyList(), 1,
                Arrays.asList("morgue-bad-20260823-120000.txt"));

        String report = DCSSMorgueStatsActivity.render(stats, audit, "All games");

        assertFalse(report.contains("Audit:"));
        assertFalse(report.contains("Skipped:"));
    }

    @Test
    public void showsAuditTextOnlyInTheAuditView() {
        DCSSMorgueStats stats = new DCSSMorgueStats(Collections.emptyList(),
                new DCSSMorgueRoster(27, 33, 27, 880));
        DCSSMorgueLoadResult audit = new DCSSMorgueLoadResult(Collections.emptyList(), 1,
                Arrays.asList("morgue-bad-20260823-120000.txt"));

        String report = DCSSMorgueStatsActivity.render(stats, audit, "Audit");

        assertTrue(report.contains("Audit: 1 timestamped final files · 0 counted · 1 skipped"));
        assertTrue(report.contains("Skipped: morgue-bad-20260823-120000.txt"));
    }
}
