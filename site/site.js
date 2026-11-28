/* ██╗  ██╗██████╗  ██████╗ ███████╗
 * ██║ ██╔╝██╔══██╗██╔═══██╗██╔════╝
 * █████╔╝ ██║  ██║██║   ██║███████╗
 * ██╔═██╗ ██║  ██║██║   ██║╚════██║
 * ██║  ██╗██████╔╝╚██████╔╝███████║
 * ╚═╝  ╚═╝╚═════╝  ╚═════╝ ╚══════╝
 * ---------------------------------
 *   kdos.dev — the accent picker, Phosphor mode, the gallery's lightbox,
 *   the code blocks' copy buttons, the phone menu and the 404 page's path
 * ---------------------------------
 *
 * THE PAGE WORKS WITHOUT THIS FILE. Every control it drives is either a link
 * that already goes somewhere (a screenshot, an accent's picture) or marked
 * .js-only and hidden until the head script has run. Nothing here fetches
 * anything; the only state is two keys in the reader's own localStorage,
 * read and written under try/catch because a private window or blocked
 * storage throws.
 *
 * THE ACCENT RULES ARE THE HEAD SCRIPT'S, which mksite.py writes into the
 * page so the first paint is already right: <html data-choice> is the accent
 * the reader picked, <html data-accent> the one in force, and Phosphor mode
 * trades each accent <html data-crt-pairs> names for phosphor, as
 * `kdos phosphor on` does on the desktop. Picking one of those accents
 * while Phosphor mode is on turns it off, since a CRT cannot wear it.
 */
(function () {
  'use strict';

  var root = document.documentElement;
  var pairs = (root.getAttribute('data-crt-pairs') || '').split(/\s+/).filter(Boolean);
  var motion = !window.matchMedia || !window.matchMedia('(prefers-reduced-motion: reduce)').matches;
  var state = {
    choice: root.getAttribute('data-choice') || root.getAttribute('data-accent'),
    phosphor: root.getAttribute('data-phosphor') === 'on'
  };

  function all(sel, ctx) {
    return Array.prototype.slice.call((ctx || document).querySelectorAll(sel));
  }

  function save(key, value) {
    try { window.localStorage.setItem(key, value); } catch (e) { /* storage refused */ }
  }

  // An accent as prose names it, capitalised as mksite.py titles it; a
  // terminal on the page names it as the command does, in lower case.
  function title(name) {
    return name.charAt(0).toUpperCase() + name.slice(1);
  }

  function inForce() {
    return state.phosphor && pairs.indexOf(state.choice) >= 0 ? 'phosphor' : state.choice;
  }

  var monitor = document.querySelector('[data-accent-shots]');
  var shots = {};
  if (monitor) {
    try { shots = JSON.parse(monitor.getAttribute('data-accent-shots')); } catch (e) { shots = {}; }
  }

  function showShot(accent, animate) {
    if (!monitor) return;
    var key = state.phosphor && shots.crt ? 'crt' : accent;
    var s = shots[key];
    if (!s) return;
    var img = monitor.querySelector('[data-accent-shot]');
    var cap = monitor.querySelector('[data-accent-caption]');
    if (img && img.getAttribute('src') !== s.src) {
      img.src = s.src;
      img.alt = s.alt;
      img.width = s.w;
      img.height = s.h;
      if (animate && motion) {
        var screen = img.parentNode;
        screen.classList.remove('is-degauss');
        void screen.offsetWidth;
        screen.classList.add('is-degauss');
      }
    }
    if (cap) cap.textContent = s.caption;
  }

  function apply(animate) {
    var accent = inForce();
    root.setAttribute('data-choice', state.choice);
    root.setAttribute('data-accent', accent);
    root.setAttribute('data-phosphor', state.phosphor ? 'on' : 'off');

    all('[data-accent-src]').forEach(function (img) {
      var src = img.getAttribute('data-accent-src').replace('{accent}', accent);
      if (img.getAttribute('src') !== src) img.setAttribute('src', src);
    });
    all('[data-accent-set]').forEach(function (el) {
      el.setAttribute('aria-pressed', el.getAttribute('data-accent-set') === state.choice ? 'true' : 'false');
    });
    all('[data-accent-name]').forEach(function (el) { el.textContent = accent; });
    all('[data-accent-title]').forEach(function (el) { el.textContent = title(accent); });
    // An accent Phosphor mode replaces stays the one chosen, and the label
    // says so, as the pressed swatch does.
    all('[data-accent-via]').forEach(function (el) {
      el.textContent = accent !== state.choice ? '(' + title(state.choice) + ' + CRT)' : '';
    });
    all('[data-theme-row]').forEach(function (el) {
      var on = el.getAttribute('data-theme-row') === accent;
      el.textContent = (on ? '*' : ' ') + el.textContent.slice(1);
    });
    all('[data-phosphor-toggle]').forEach(function (el) {
      el.setAttribute('aria-pressed', state.phosphor ? 'true' : 'false');
    });
    showShot(accent, animate);

    var meta = document.querySelector('meta[name="theme-color"]');
    if (meta && window.getComputedStyle) {
      var deep = window.getComputedStyle(root).getPropertyValue('--deep').trim();
      if (deep) meta.setAttribute('content', deep);
    }
  }

  function pick(name) {
    state.choice = name;
    if (state.phosphor && pairs.indexOf(name) >= 0) {
      state.phosphor = false;
      save('kdos-phosphor', 'off');
    }
    save('kdos-accent', name);
    apply(true);
  }

  all('[data-accent-set]').forEach(function (el) {
    if (el.tagName === 'A') {
      // A link to the accent's picture without this script; a toggle with it.
      el.setAttribute('role', 'button');
      el.addEventListener('keydown', function (e) {
        if (e.key === ' ' || e.key === 'Spacebar') {
          e.preventDefault();
          pick(el.getAttribute('data-accent-set'));
        }
      });
    }
    el.addEventListener('click', function (e) {
      e.preventDefault();
      pick(el.getAttribute('data-accent-set'));
    });
  });

  all('[data-phosphor-toggle]').forEach(function (el) {
    el.addEventListener('click', function () {
      state.phosphor = !state.phosphor;
      save('kdos-phosphor', state.phosphor ? 'on' : 'off');
      if (state.phosphor && motion) {
        root.classList.remove('crt-power');
        void root.offsetWidth;
        root.classList.add('crt-power');
        window.setTimeout(function () { root.classList.remove('crt-power'); }, 700);
      }
      apply(true);
    });
  });

  apply(false);

  // ── the lightbox ─────────────────────────────────────────────────────
  var links = all('[data-gallery] a.shot');
  if (links.length && typeof window.HTMLDialogElement === 'function') {
    var dlg = document.createElement('dialog');
    dlg.className = 'lightbox';
    dlg.setAttribute('aria-labelledby', 'lightbox-title');
    // The caption is shown and the title only named, since nearly every
    // caption opens with its title's words. An animation opens as its still
    // (data-still) and moves only while Play is pressed.
    dlg.innerHTML =
      '<div class="lightbox__bar">' +
        '<span class="lightbox__count" aria-live="polite"></span>' +
        '<span class="lightbox__tools">' +
        '<button type="button" class="lightbox__play" data-lb="play" aria-pressed="false" hidden>Play</button>' +
        '<button type="button" class="lightbox__close" data-lb="close">Close <kbd>Esc</kbd></button>' +
        '</span>' +
      '</div>' +
      '<figure class="lightbox__fig">' +
        '<img class="lightbox__img" alt="">' +
        '<figcaption class="lightbox__cap"><span class="sr-only" id="lightbox-title"></span>' +
        '<span class="lightbox__text"></span></figcaption>' +
      '</figure>' +
      '<button type="button" class="lightbox__nav lightbox__prev" data-lb="prev" aria-label="Previous picture">&#9664;</button>' +
      '<button type="button" class="lightbox__nav lightbox__next" data-lb="next" aria-label="Next picture">&#9654;</button>';
    document.body.appendChild(dlg);

    var img = dlg.querySelector('.lightbox__img');
    var lbTitle = dlg.querySelector('#lightbox-title');
    var text = dlg.querySelector('.lightbox__text');
    var count = dlg.querySelector('.lightbox__count');
    var play = dlg.querySelector('.lightbox__play');
    var at = 0;
    var opener = null;

    var picture = function (a) {
      return a.getAttribute('data-still') || a.getAttribute('href');
    };

    var playing = function (on) {
      var a = links[at];
      img.src = on ? a.getAttribute('href') : picture(a);
      play.setAttribute('aria-pressed', on ? 'true' : 'false');
      play.textContent = on ? 'Stop' : 'Play';
    };

    var show = function (i) {
      at = (i + links.length) % links.length;
      var a = links[at];
      var thumb = a.querySelector('img');
      play.hidden = !a.getAttribute('data-still');
      playing(false);
      img.alt = thumb ? thumb.alt : '';
      img.width = +a.getAttribute('data-w');
      img.height = +a.getAttribute('data-h');
      lbTitle.textContent = a.getAttribute('data-title');
      text.textContent = a.getAttribute('data-caption');
      count.textContent = (at + 1) + ' / ' + links.length;
      [at - 1, at + 1].forEach(function (j) {
        new Image().src = picture(links[(j + links.length) % links.length]);
      });
    };

    links.forEach(function (a, i) {
      a.addEventListener('click', function (e) {
        if (e.button || e.ctrlKey || e.metaKey || e.shiftKey || e.altKey) return;
        e.preventDefault();
        opener = a;
        show(i);
        dlg.showModal();
      });
    });

    dlg.addEventListener('click', function (e) {
      var b = e.target.closest ? e.target.closest('[data-lb]') : null;
      if (b) {
        var what = b.getAttribute('data-lb');
        if (what === 'close') dlg.close();
        else if (what === 'play') playing(play.getAttribute('aria-pressed') !== 'true');
        else show(at + (what === 'next' ? 1 : -1));
      } else if (e.target === dlg) {
        dlg.close();
      }
    });
    dlg.addEventListener('keydown', function (e) {
      if (e.key === 'ArrowRight') { e.preventDefault(); show(at + 1); }
      else if (e.key === 'ArrowLeft') { e.preventDefault(); show(at - 1); }
    });
    dlg.addEventListener('close', function () {
      img.removeAttribute('src');
      if (opener) opener.focus();
    });
  }

  // ── the phone menu ───────────────────────────────────────────────────
  // A <details>: without this file it opens and closes on its summary alone.
  // A section picked from it, Escape and a tap outside it close it, so it
  // never stays over the section it sent the reader to.
  var menu = document.querySelector('details.menu');
  if (menu) {
    all('.menu__nav a', menu).forEach(function (a) {
      a.addEventListener('click', function () { menu.open = false; });
    });
    document.addEventListener('keydown', function (e) {
      if (e.key === 'Escape' && menu.open) {
        menu.open = false;
        menu.querySelector('summary').focus();
      }
    });
    document.addEventListener('click', function (e) {
      if (menu.open && !menu.contains(e.target)) menu.open = false;
    });
  }

  // ── the 404 page ─────────────────────────────────────────────────────
  all('[data-404-path]').forEach(function (el) {
    el.textContent = window.location.pathname;
  });

  // ── copy buttons ─────────────────────────────────────────────────────
  // A button's own text changing is not reliably read out, so a status
  // region says that the copy worked.
  if (navigator.clipboard && window.isSecureContext) {
    var status = document.createElement('p');
    status.className = 'sr-only';
    status.setAttribute('role', 'status');
    document.body.appendChild(status);
    all('.code').forEach(function (box) {
      var code = box.querySelector('code');
      if (!code) return;
      var b = document.createElement('button');
      b.type = 'button';
      b.className = 'code__copy';
      b.textContent = 'Copy';
      b.setAttribute('aria-label', 'Copy these commands');
      b.addEventListener('click', function () {
        navigator.clipboard.writeText(code.textContent).then(function () {
          b.textContent = 'Copied';
          b.classList.add('is-done');
          status.textContent = 'Copied to the clipboard';
          window.setTimeout(function () {
            b.textContent = 'Copy';
            b.classList.remove('is-done');
            status.textContent = '';
          }, 1600);
        }, function () {
          b.textContent = 'Select to copy';
          status.textContent = 'Copying was refused: select the commands to copy them';
        });
      });
      box.appendChild(b);
    });
  }
}());
