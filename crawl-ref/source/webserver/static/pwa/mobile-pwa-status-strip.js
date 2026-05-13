(function () {
    "use strict";

    var pwaApi = window.DCSS_PWA = window.DCSS_PWA || {};

    pwaApi.createStatusStrip = function (options) {
        var statusRoot = null;
        var statusObserver = null;
        var observedStats = null;
        var onChange = options.onChange || function () {};

        function statText(id)
        {
            var element = document.getElementById(id);
            return element ? element.textContent.trim() : "";
        }

        function compactPlace()
        {
            return statText("stats_place").replace(/^Dungeon:/, "D:");
        }

        function hasLiveStats()
        {
            var maxHp = parseInt(statText("stats_hp_max"), 10);
            return Number.isFinite(maxHp) && maxHp > 0;
        }

        function sync()
        {
            if (!statusRoot)
                return;

            var items = [
                "HP " + statText("stats_hp") + "/" + statText("stats_hp_max"),
                "MP " + statText("stats_mp") + "/" + statText("stats_mp_max"),
                "XL" + statText("stats_xl"),
                "AC" + statText("stats_ac"),
                "EV" + statText("stats_ev"),
                "SH" + statText("stats_sh"),
                compactPlace()
            ].filter(function (item) {
                return !/\/$/.test(item) && !/[A-Z]$/.test(item);
            });

            statusRoot.textContent = "";
            items.forEach(function (item) {
                var span = document.createElement("span");
                span.textContent = item;
                statusRoot.appendChild(span);
            });

            onChange();
        }

        function render(root)
        {
            statusRoot = document.createElement("div");
            statusRoot.className = "dcss-pwa-status";
            root.appendChild(statusRoot);
            sync();
        }

        function bindObserver()
        {
            var stats = document.getElementById("stats");
            if (!stats)
                return;

            if (stats === observedStats && statusObserver)
                return;

            if (statusObserver)
                statusObserver.disconnect();

            observedStats = stats;
            statusObserver = new MutationObserver(sync);
            statusObserver.observe(stats, {
                childList: true,
                characterData: true,
                subtree: true,
                attributes: true
            });
            sync();
        }

        return {
            bindObserver: bindObserver,
            hasLiveStats: hasLiveStats,
            render: render,
            sync: sync
        };
    };
})();
