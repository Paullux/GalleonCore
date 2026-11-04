// SPDX-FileCopyrightText: 2026 Paul Woisard
// SPDX-License-Identifier: GPL-3.0-or-later

(function () {
  "use strict";

  var DIRECT = ["windows", "ubuntu", "fedora", "arch", "appimage"];
  var API = "https://api.github.com/repos/Paullux/GalleonCore/releases?per_page=100&page=";

  var LANGUAGES = ["en", "fr", "de", "es"];
  var LABELS = {
    en: { snapshot: "Daily snapshot", stale: "outdated" },
    fr: { snapshot: "Relevé quotidien", stale: "ancien" },
    de: { snapshot: "Täglicher Stand", stale: "veraltet" },
    es: { snapshot: "Instantánea diaria", stale: "desactualizado" }
  };

  function repositoryDates(checked, today, language) {
    var words = LABELS[language] || LABELS.en;
    return ["ppa", "copr"].map(function (name) {
      var last = checked[name];
      var stale = !last || last !== today;
      return name.toUpperCase() + " " + (last || "—") + (stale ? " (" + words.stale + ")" : "");
    }).join("; ");
  }

  function platformFor(name) {
    var lower = typeof name === "string" ? name.toLowerCase() : "";
    if (lower.endsWith("-setup.exe") || lower.endsWith("-windows-x86_64.zip")) return "windows";
    if (lower.endsWith(".deb") && !lower.endsWith(".ddeb")) return "ubuntu";
    if (lower.endsWith(".rpm") && !lower.includes(".src.") &&
        !lower.includes("-debuginfo-") && !lower.includes("-debugsource-")) return "fedora";
    if (lower.endsWith(".pkg.tar.zst") && !lower.includes("-debug-")) return "arch";
    if (lower.endsWith(".appimage")) return "appimage";
    return null;
  }

  function assetsFromReleases(releases) {
    var assets = {};
    releases.forEach(function (release) {
      if (!release || typeof release !== "object") return;
      if (release.draft || release.prerelease) return;
      (Array.isArray(release.assets) ? release.assets : []).forEach(function (asset) {
        if (!asset || typeof asset !== "object") return;
        var platform = platformFor(asset.name || "");
        if (platform && Number.isSafeInteger(asset.id) &&
            Number.isSafeInteger(asset.download_count) && asset.download_count >= 0) {
          assets[String(asset.id)] = { platform: platform, count: asset.download_count };
        }
      });
    });
    return assets;
  }

  function totals(assets) {
    var result = {};
    DIRECT.forEach(function (name) { result[name] = 0; });
    Object.keys(assets || {}).forEach(function (id) {
      var asset = assets[id];
      if (asset && DIRECT.includes(asset.platform) &&
          Number.isSafeInteger(asset.count) && asset.count >= 0) {
        result[asset.platform] += asset.count;
      }
    });
    return result;
  }

  function sevenDayDeltas(current, baseline) {
    var result = {};
    DIRECT.forEach(function (name) { result[name] = 0; });
    Object.keys(current).forEach(function (id) {
      var item = current[id];
      if (!item || !Number.isSafeInteger(item.count) || item.count < 0) return;
      var old = (baseline || {})[id];
      var previous = old && Number.isSafeInteger(old.count) && old.count >= 0 ? old.count : 0;
      if (DIRECT.includes(item.platform)) result[item.platform] += Math.max(0, item.count - previous);
    });
    return result;
  }

  if (typeof module !== "undefined") {
    module.exports = { platformFor: platformFor, assetsFromReleases: assetsFromReleases,
      totals: totals, sevenDayDeltas: sevenDayDeltas, approximateRecent: approximateRecent,
      recentEstimate: recentEstimate, repositoryDates: repositoryDates };
  }
  if (typeof document === "undefined") return;

  var published = null;
  var controller = null;
  var note = document.getElementById("download-stats-note");

  function setNumber(selector, value) {
    if (Number.isSafeInteger(value) && value >= 0) {
      var element = document.querySelector(selector);
      if (element) element.textContent = value.toLocaleString();
    }
  }

  function baselineAssets(stats) {
    if (!stats.since || !Array.isArray(stats.history)) return null;
    var entry = stats.history.find(function (item) { return item.date === stats.since; });
    return entry && entry.assets && typeof entry.assets === "object" ? entry.assets : null;
  }

  function approximateRecent(direct, repositories) {
    if (!direct || !repositories) return null;
    var values = DIRECT.map(function (name) { return direct[name]; })
      .concat(["ppa", "copr"].map(function (name) { return repositories[name]; }));
    if (!values.every(function (value) { return Number.isSafeInteger(value) && value >= 0; })) return null;
    return values.reduce(function (sum, value) { return sum + value; }, 0);
  }

  // Keep the approximation all-channel: prefer seven days, then a complete day.
  function recentEstimate(weekly, repositoryWeekly, daily, repositoryDaily) {
    var week = approximateRecent(weekly, repositoryWeekly);
    if (week !== null) return { value: week, window: "7d" };
    var day = approximateRecent(daily, repositoryDaily);
    if (day !== null) return { value: day, window: "24h" };
    return { value: null, window: null };
  }

  function previousDayAssets(stats) {
    var history = stats && Array.isArray(stats.history) ? stats.history : [];
    if (history.length < 2) return null;
    var current = history[history.length - 1], previous = history[history.length - 2];
    if (!current || !previous || !current.date || !previous.date) return null;
    if (Date.parse(current.date + "T00:00:00Z") - Date.parse(previous.date + "T00:00:00Z") !== 86400000) return null;
    return previous.assets && typeof previous.assets === "object" ? previous.assets : null;
  }

  function setDirect(assets, live) {
    var current = totals(assets);
    var baseline = published ? baselineAssets(published) : null;
    var weekly = baseline ? sevenDayDeltas(assets, baseline) : (published && published.by_channel);
    var yesterday = published ? previousDayAssets(published) : null;
    var daily = yesterday ? sevenDayDeltas(assets, yesterday) : (published && published.daily);
    DIRECT.forEach(function (name) {
      setNumber('[data-download-stat="' + name + '"]', current[name]);
      if (weekly) setNumber('[data-download-week="' + name + '"]', weekly[name]);
      if (daily) setNumber('[data-download-day="' + name + '"]', daily[name]);
    });
    var repositories = published && (published.repository_weekly || published.by_channel);
    var estimate = recentEstimate(weekly, repositories, daily, published && published.daily);
    var totalElement = document.querySelector('[data-download-stat="total"]');
    if (totalElement) {
      if (estimate.value !== null) totalElement.textContent = estimate.value.toLocaleString();
      else totalElement.innerHTML = '<span lang="en">Pending</span><span lang="fr">En attente</span><span lang="de">Ausstehend</span><span lang="es">Pendiente</span>';
    }
    var windowElement = document.querySelector("[data-download-total-window]");
    if (windowElement) windowElement.textContent = estimate.window ? " · " + estimate.window : "";
    document.getElementById("direct-count-source").innerHTML = live ? "GitHub API" : LANGUAGES.map(function (language) {
      return '<span lang="' + language + '">' + LABELS[language].snapshot + '</span>';
    }).join("");
  }

  function setNote(stats, live) {
    var checked = stats.repository_checked_at || {};
    var today = new Date().toISOString().slice(0, 10);
    note.innerHTML = '<span lang="en">GitHub totals: <span data-source></span>. PPA/COPR collection daily at 03:23 UTC. GitHub totals and recent deltas refresh live when allowed. The approximate recent total includes GitHub, PPA and COPR over seven days when complete, otherwise over a complete daily interval. Missing measurements never count as zero. AUR votes fall back to the daily snapshot if its live API is unavailable. PPA: package downloads, including updates. COPR: cumulative RPM downloads. Last successful collection: <span data-repo-dates></span>. These are not unique installations or users. AUR measures votes.</span><span lang="fr">Totaux GitHub : <span data-source></span>. Collecte PPA/COPR quotidienne à 03 h 23 UTC. Totaux et variations GitHub actualisés en direct si autorisés. Le total récent approximatif additionne GitHub, PPA et COPR sur sept jours si les références sont complètes, sinon sur un intervalle quotidien complet. Une mesure absente n\'est jamais comptée comme zéro. Les votes AUR utilisent le relevé quotidien si leur API en direct est inaccessible. PPA : téléchargements de paquets, mises à jour comprises. COPR : téléchargements RPM cumulés. Dernière collecte réussie : <span data-repo-dates></span>. Ce ne sont pas des installations ou utilisateurs uniques. AUR mesure les votes.</span><span lang="de">GitHub-Summen: <span data-source></span>. Die PPA/COPR-Erhebung erfolgt täglich um 03:23 UTC. GitHub-Summen und aktuelle Änderungen werden live aktualisiert, sofern erlaubt. Die ungefähre aktuelle Summe addiert GitHub, PPA und COPR über sieben Tage, wenn die Referenzwerte vollständig sind, andernfalls über ein vollständiges Tagesintervall. Eine fehlende Messung wird nie als null gezählt. AUR-Stimmen fallen auf den täglichen Schnappschuss zurück, wenn die Live-API nicht erreichbar ist. PPA: Paket-Downloads, einschließlich Updates. COPR: kumulierte RPM-Downloads. Letzte erfolgreiche Erhebung: <span data-repo-dates></span>. Das sind keine eindeutigen Installationen oder Nutzer. Der AUR misst Stimmen.</span><span lang="es">Totales de GitHub: <span data-source></span>. La recopilación de PPA/COPR es diaria a las 03:23 UTC. Los totales y las variaciones recientes de GitHub se actualizan en directo si se permite. El total reciente aproximado suma GitHub, PPA y COPR durante siete días cuando las referencias están completas; si no, durante un intervalo diario completo. Una medición ausente nunca se cuenta como cero. Los votos del AUR recurren a la instantánea diaria si su API en directo no está disponible. PPA: descargas de paquetes, incluidas las actualizaciones. COPR: descargas de RPM acumuladas. Última recopilación correcta: <span data-repo-dates></span>. No son instalaciones ni usuarios únicos. El AUR mide votos.</span>';
    note.querySelectorAll("[data-source]").forEach(function (span) {
      var language = span.closest("[lang]").getAttribute("lang");
      span.textContent = live ? "GitHub API" : stats.generated_at || LABELS[language].snapshot;
    });
    note.querySelectorAll("[data-repo-dates]").forEach(function (span) {
      span.textContent = repositoryDates(checked, today, span.closest("[lang]").getAttribute("lang"));
    });
  }

  function renderSnapshot(stats) {
    published = stats;
    var history = Array.isArray(stats.history) ? stats.history : [];
    var latest = history[history.length - 1];
    if (latest && latest.assets) setDirect(latest.assets, false);
    var repositoryWeekly = stats.repository_weekly || stats.by_channel || {};
    ["ppa", "copr"].forEach(function (name) {
      setNumber('[data-download-stat="' + name + '"]', latest ? (latest.repositories || {})[name] : null);
      setNumber('[data-download-week="' + name + '"]', repositoryWeekly[name]);
      if (stats.daily) setNumber('[data-download-day="' + name + '"]', stats.daily[name]);
    });
    setNumber('[data-download-stat="aur"]', stats.aur_votes);
    setNote(stats, false);
    refreshLive();
  }

  async function fetchLiveReleases(signal) {
    var releases = [];
    for (var page = 1; page <= 10; page++) {
      var response = await fetch(API + page, { cache: "no-store", signal: signal });
      if (!response.ok) throw new Error("GitHub API unavailable");
      var batch = await response.json();
      if (!Array.isArray(batch)) throw new Error("Invalid GitHub API response");
      releases.push.apply(releases, batch);
      if (batch.length < 100) return releases;
    }
    throw new Error("Too many releases for a complete count");
  }

  async function fetchLiveAurVotes(signal) {
    var response = await fetch("https://aur.archlinux.org/rpc/v5/info?arg%5B%5D=galleoncore",
      { cache: "no-store", credentials: "omit", signal: signal });
    if (!response.ok) throw new Error("AUR API unavailable");
    var data = await response.json();
    var entry = data && Array.isArray(data.results) ? data.results.find(function (item) {
      return item.Name === "galleoncore";
    }) : null;
    if (!entry || !Number.isSafeInteger(entry.NumVotes) || entry.NumVotes < 0)
      throw new Error("Invalid AUR vote count");
    return entry.NumVotes;
  }

  async function refreshLive() {
    if (!published || document.documentElement.dataset.externalAllowed !== "yes" || controller) return;
    controller = new AbortController();
    var active = controller;
    try {
      await Promise.allSettled([
        fetchLiveReleases(active.signal).then(function (releases) {
          if (document.documentElement.dataset.externalAllowed === "yes") {
            setDirect(assetsFromReleases(releases), true);
            setNote(published, true);
          }
        }),
        fetchLiveAurVotes(active.signal).then(function (votes) {
          if (document.documentElement.dataset.externalAllowed === "yes")
            setNumber('[data-download-stat="aur"]', votes);
        })
      ]);
    } finally {
      if (controller === active) controller = null;
    }
  }

  document.addEventListener("galleoncore:external-choice", function (event) {
    if (event.detail.enabled) refreshLive();
    else {
      if (controller) controller.abort();
      if (published) renderSnapshot(published);
    }
  });

  fetch("./downloads-weekly.json", { cache: "no-store" }).then(function (response) {
    if (!response.ok) throw new Error("Download counts unavailable");
    return response.json();
  }).then(renderSnapshot).catch(function () {
    note.innerHTML = '<span lang="en">Download counts are temporarily unavailable.</span><span lang="fr">Les compteurs de téléchargement sont temporairement indisponibles.</span><span lang="de">Die Download-Zähler sind vorübergehend nicht verfügbar.</span><span lang="es">Los contadores de descargas no están disponibles temporalmente.</span>';
  });
})();
