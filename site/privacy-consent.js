// SPDX-FileCopyrightText: 2026 Paul Woisard
// SPDX-License-Identifier: GPL-3.0-or-later

(function () {
  "use strict";
  var key = "galleoncore-external-v1";
  var root = document.documentElement;
  // Self-hosted, cookie-free Matomo loads only with the visitor's consent.
  var matomoLoaded = false;
  function applyAnalytics(enabled) {
    if (!enabled) {
      if (matomoLoaded) window._paq.push(["forgetConsentGiven"]);
      return;
    }
    if (matomoLoaded) {
      window._paq.push(["setConsentGiven"]);
      window._paq.push(["trackPageView"]);
      return;
    }
    matomoLoaded = true;
    var queue = window._paq = window._paq || [];
    queue.push(["requireConsent"]);
    queue.push(["disableCookies"]);
    queue.push(["setTrackerUrl", "https://stats.social-hub.fr/matomo.php"]);
    queue.push(["setSiteId", "5"]);
    queue.push(["setConsentGiven"]);
    queue.push(["trackPageView"]);
    queue.push(["enableLinkTracking"]);
    var script = document.createElement("script");
    script.async = true;
    script.src = "https://stats.social-hub.fr/matomo.js";
    document.head.appendChild(script);
  }
  var choice = null;
  try { choice = localStorage.getItem(key); } catch (error) {}

  // Localize page titles and accessible labels as well as visible paragraphs.
  function localizePage() {
    var language = root.getAttribute("lang") || "en";
    document.querySelectorAll("[data-localized]").forEach(function (element) {
      var value = element.getAttribute("data-" + language) || element.getAttribute("data-en");
      var target = element.getAttribute("data-localized");
      if (!value) return;
      if (target === "text") element.textContent = value;
      else element.setAttribute(target, value);
      if (target === "alt" && element.nextElementSibling &&
          element.nextElementSibling.classList.contains("badge-fallback")) {
        element.nextElementSibling.textContent = value;
      }
    });
  }
  document.addEventListener("galleoncore:language-change", localizePage);

  if (document.body.classList.contains("legal-page")) {
    var languages = ["en", "fr", "de", "es"];
    var savedLanguage = null;
    try { savedLanguage = localStorage.getItem("galleoncore-lang"); } catch (error) {}
    var browserLanguage = (navigator.language || "en").toLowerCase().slice(0, 2);
    var language = languages.indexOf(savedLanguage) >= 0 ? savedLanguage
      : (languages.indexOf(browserLanguage) >= 0 ? browserLanguage : "en");
    var select = document.querySelector("[data-legal-lang-select]");
    function setLegalLanguage(value) {
      root.setAttribute("data-lang", value);
      root.setAttribute("lang", value);
      if (select) select.value = value;
      document.dispatchEvent(new CustomEvent("galleoncore:language-change"));
    }
    setLegalLanguage(language);
    if (select) select.addEventListener("change", function () {
      if (languages.indexOf(select.value) < 0) return;
      setLegalLanguage(select.value);
      try { localStorage.setItem("galleoncore-lang", select.value); } catch (error) {}
    });
  }

  document.querySelectorAll(".badges img[data-external-src]").forEach(function (image) {
    var fallback = document.createElement("span");
    fallback.className = "badge-fallback";
    fallback.textContent = image.alt;
    image.after(fallback);
  });

  localizePage();

  function applyExternal(enabled) {
    root.dataset.externalAllowed = enabled ? "yes" : "no";
    applyAnalytics(enabled);
    document.querySelectorAll("link[data-external-href]").forEach(function (link) {
      if (enabled) {
        link.disabled = false;
        link.href = link.dataset.externalHref;
      } else {
        link.disabled = true;
        link.removeAttribute("href");
      }
    });
    document.querySelectorAll("img[data-external-src]").forEach(function (image) {
      if (enabled) image.src = image.dataset.externalSrc;
      else image.removeAttribute("src");
    });
    document.dispatchEvent(new CustomEvent("galleoncore:external-choice", {
      detail: { enabled: enabled }
    }));
  }

  var banner = document.createElement("aside");
  banner.className = "privacy-banner";
  banner.setAttribute("aria-label", "Privacy choices / Choix de confidentialité / Datenschutzeinstellungen / Opciones de privacidad");
  banner.innerHTML = '<p><span lang="en">This site stores your language and privacy choice on this device. Optional fonts, status badges, live download counts and cookie-free Matomo visitor statistics contact external services only if you allow them. You can change your choice at any time. <a href="privacy.html">Privacy details</a>.</span><span lang="fr">Ce site mémorise ta langue et ton choix de confidentialité sur cet appareil. Les polices, badges, compteurs en direct et statistiques de fréquentation Matomo (sans cookies) sont activés uniquement avec ton accord. Tu peux modifier ton choix à tout moment. <a href="privacy.html">Détails sur la confidentialité</a>.</span><span lang="de">Diese Website speichert deine Sprache und deine Datenschutzeinstellungen auf diesem Gerät. Optionale Schriften, Status-Badges, Live-Download-Zähler und cookiefreie Matomo-Besucherstatistiken kontaktieren externe Dienste nur, wenn du es erlaubst. Du kannst deine Auswahl jederzeit ändern. <a href="privacy.html">Datenschutzdetails</a>.</span><span lang="es">Este sitio guarda tu idioma y tu elección de privacidad en este dispositivo. Las fuentes opcionales, las insignias de estado, los contadores de descargas en directo y las estadísticas de visitas de Matomo sin cookies solo contactan con servicios externos si lo permites. Puedes cambiar tu elección en cualquier momento. <a href="privacy.html">Detalles de privacidad</a>.</span></p><div class="privacy-actions"><button type="button" data-choice="no"><span lang="en">Reject optional content</span><span lang="fr">Refuser le contenu facultatif</span><span lang="de">Optionale Inhalte ablehnen</span><span lang="es">Rechazar el contenido opcional</span></button><button type="button" data-choice="yes"><span lang="en">Allow optional content</span><span lang="fr">Autoriser le contenu facultatif</span><span lang="de">Optionale Inhalte erlauben</span><span lang="es">Permitir el contenido opcional</span></button></div>';
  document.body.append(banner);
  banner.hidden = choice === "yes" || choice === "no";
  applyExternal(choice === "yes");

  banner.querySelectorAll("[data-choice]").forEach(function (button) {
    button.addEventListener("click", function () {
      choice = button.dataset.choice;
      try { localStorage.setItem(key, choice); } catch (error) {}
      applyExternal(choice === "yes");
      banner.hidden = true;
    });
  });
  document.querySelectorAll("[data-open-privacy-choices]").forEach(function (button) {
    button.addEventListener("click", function () {
      banner.hidden = false;
      banner.querySelector("[data-choice]").focus();
    });
  });
})();
