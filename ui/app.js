// MIMIC UI. Talks to the plugin through JUCE's WebView interop (juce/index.js).
// Outside the plugin (e.g. opened in a browser) it runs on mock state so the design can be previewed.

const inPlugin = typeof window.__JUCE__ !== "undefined" && typeof window.__JUCE__.postMessage === "function";

// ------------------------------------------------------------------ fallback data (preview only)
const PREVIEW_ARTISTS = [
  ["Juice WRLD", "Rap", "#7c5cff", "S\u0131cak, melodik, hafif tune ve geni\u015f plate"],
  ["Playboi Carti", "Rap", "#ff2d55", "Bebek sesi formant, sert tune, kirli ve kuru"],
  ["Lil Uzi Vert", "Rap", "#00e5ff", "\u00c7ok parlak, h\u0131zl\u0131 tune, katl\u0131 ve geni\u015f"],
  ["Travis Scott", "Rap", "#ff8a00", "Karanl\u0131k, uzun delay ve devasa reverb"],
  ["Don Toliver", "Rap", "#ff5ec4", "\u0130peksi tepe frekans, ak\u0131c\u0131 tune"],
  ["Ken Carson", "Rap", "#d0ff00", "Rage: a\u015f\u0131r\u0131 distortion ve ezik kompres"],
  ["Future", "Rap", "#36d399", "Kal\u0131n, robotik hard-tune, koyu delay"],
  ["Ezhel", "T\u00fcrk\u00e7e Rap", "#ffd166", "Temiz, punchy, \u00f6l\u00e7\u00fcl\u00fc tune"],
  ["Uzi", "T\u00fcrk\u00e7e Rap", "#ef476f", "Agresif drill/trap, sert tune, \u00f6ne \u00e7\u0131kan vokal"],
  ["Lvbel C5", "T\u00fcrk\u00e7e Rap", "#06d6a0", "Yo\u011fun autotune, tiz ve enerjik"],
  ["Blok3", "T\u00fcrk\u00e7e Rap", "#118ab2", "Arabesk-trap: duygusal, bol delay ve reverb (Hicaz dene)"],
  ["Billie Eilish", "Pop / R&B", "#9be564", "Yak\u0131n, f\u0131s\u0131lt\u0131l\u0131, yumu\u015fak ve do\u011fal"],
  ["The Weeknd", "Pop / R&B", "#e63946", "80'ler: parlak, katl\u0131, b\u00fcy\u00fck reverb"],
  ["Frank Ocean", "Pop / R&B", "#c9a227", "Bant s\u0131cakl\u0131\u011f\u0131, do\u011fal tune, koyu oda"],
  ["SZA", "Pop / R&B", "#f4a261", "Havadar, yumu\u015fak tune, geni\u015f ve parlak"],
].map(([name, category, accent, tagline], index) => ({ index, name, category, accent, tagline }));

const NOTE_NAMES = ["C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B"];

// ------------------------------------------------------------------ state backends
class Listeners {
  constructor() { this.fns = new Map(); this.id = 0; }
  addListener(fn) { this.fns.set(++this.id, fn); return this.id; }
  callListeners() { for (const fn of this.fns.values()) fn(); }
}
class MockSlider {
  constructor(start, end, def) {
    this.properties = { start, end };
    this.v = (def - start) / (end - start);
    this.valueChangedEvent = new Listeners();
    this.propertiesChangedEvent = new Listeners();
  }
  getNormalisedValue() { return this.v; }
  setNormalisedValue(v) { this.v = v; this.valueChangedEvent.callListeners(); }
  sliderDragStarted() {}
  sliderDragEnded() {}
}
class MockCombo {
  constructor(choices, idx = 0) {
    this.properties = { choices };
    this.i = idx;
    this.valueChangedEvent = new Listeners();
    this.propertiesChangedEvent = new Listeners();
  }
  getChoiceIndex() { return this.i; }
  setChoiceIndex(i) { this.i = i; this.valueChangedEvent.callListeners(); }
}

let Juce = null;
if (inPlugin) Juce = await import("./juce/index.js");

const SCALE_NAMES = ["Kromatik", "Maj\u00f6r", "Min\u00f6r", "Harmonik Min\u00f6r", "Hicaz", "Min\u00f6r Pentatonik"];
const mocks = {
  artist: new MockCombo(PREVIEW_ARTISTS.map(a => a.name)),
  key: new MockCombo(NOTE_NAMES, 9),
  scale: new MockCombo(SCALE_NAMES, 0),
  amount: new MockSlider(0, 100, 100),
  tune: new MockSlider(0, 100, 50), air: new MockSlider(0, 100, 50), grit: new MockSlider(0, 100, 50),
  space: new MockSlider(0, 100, 50), width: new MockSlider(0, 100, 50),
  formant: new MockSlider(-4, 4, 0), output: new MockSlider(-12, 12, 0),
};
const slider = id => inPlugin ? Juce.getSliderState(id) : mocks[id];
const combo  = id => inPlugin ? Juce.getComboBoxState(id) : mocks[id];

// ------------------------------------------------------------------ parameter formatting
const DEFAULT_NORM = { tune: .5, air: .5, grit: .5, space: .5, width: .5, amount: 1, formant: .5, output: .5 };
const RANGE = { formant: [-4, 4], output: [-12, 12] };

function scaled(id, n) {
  const [a, b] = RANGE[id] || [0, 100];
  return a + n * (b - a);
}
function format(id, n) {
  const v = scaled(id, n);
  if (id === "formant") return (v > 0.05 ? "+" : "") + v.toFixed(1) + " st";
  if (id === "output")  return (v > 0.05 ? "+" : "") + v.toFixed(1) + " dB";
  return Math.round(v) + "%";
}

// ------------------------------------------------------------------ knobs
const SWEEP = 270, START = -135;
const polar = (cx, cy, r, deg) => {
  const a = (deg - 90) * Math.PI / 180;
  return [cx + r * Math.cos(a), cy + r * Math.sin(a)];
};
function arc(cx, cy, r, a0, a1) {
  const [x0, y0] = polar(cx, cy, r, a0), [x1, y1] = polar(cx, cy, r, a1);
  const large = Math.abs(a1 - a0) > 180 ? 1 : 0;
  const sweep = a1 > a0 ? 1 : 0;
  return `M ${x0.toFixed(2)} ${y0.toFixed(2)} A ${r} ${r} 0 ${large} ${sweep} ${x1.toFixed(2)} ${y1.toFixed(2)}`;
}

function buildKnob(el) {
  const id = el.dataset.param;
  const state = slider(id);
  const bipolar = id === "formant" || id === "output";
  el.tabIndex = 0;
  el.setAttribute("role", "slider");
  el.setAttribute("aria-label", el.dataset.label);
  el.title = el.dataset.hint || "";

  const [hx, hy] = polar(50, 50, 47, START + SWEEP * 0.5);
  el.innerHTML = `
    <svg viewBox="0 0 100 100" aria-hidden="true">
      <path class="track" d="${arc(50, 50, 40, START, START + SWEEP)}" fill="none" stroke-width="7" stroke-linecap="round"/>
      <path class="fill" fill="none" stroke-width="7" stroke-linecap="round"/>
      <circle class="home" cx="${hx}" cy="${hy}" r="2.2"/>
      <circle class="cap" cx="50" cy="50" r="27"/>
      <line class="needle" x1="50" y1="50" x2="50" y2="30" stroke-width="3.5" stroke-linecap="round"/>
    </svg>
    <span class="knob-label">${el.dataset.label}</span>
    <span class="knob-value"></span>`;
  const fill = el.querySelector(".fill"), needle = el.querySelector(".needle"), value = el.querySelector(".knob-value");

  const render = () => {
    const n = state.getNormalisedValue();
    const ang = START + SWEEP * n;
    const from = bipolar ? START + SWEEP * 0.5 : START;
    fill.setAttribute("d", Math.abs(ang - from) < 0.5 ? "" : arc(50, 50, 40, Math.min(from, ang), Math.max(from, ang)));
    const [x, y] = polar(50, 50, 21, ang);
    needle.setAttribute("x2", x.toFixed(2));
    needle.setAttribute("y2", y.toFixed(2));
    value.textContent = format(id, n);
    el.setAttribute("aria-valuetext", value.textContent);
  };
  state.valueChangedEvent.addListener(render);
  state.propertiesChangedEvent?.addListener(render);
  render();

  attachDrag(el, state, id, (dx, dy, fine) => -dy * (fine ? 0.0012 : 0.0055));
}

// Shared pointer / wheel / keyboard handling for knobs and the Amount slider.
function attachDrag(el, state, id, deltaFn, onDown) {
  const set = n => state.setNormalisedValue(Math.min(1, Math.max(0, n)));
  let last = null, wheelTimer = null;

  el.addEventListener("pointerdown", e => {
    if (e.button !== 0) return;
    el.setPointerCapture(e.pointerId);
    el.classList.add("dragging");
    state.sliderDragStarted();
    last = { x: e.clientX, y: e.clientY };
    if (onDown) set(onDown(e));
  });
  el.addEventListener("pointermove", e => {
    if (!last) return;
    const d = deltaFn(e.clientX - last.x, e.clientY - last.y, e.shiftKey, e);
    last = { x: e.clientX, y: e.clientY };
    if (d !== null) set(state.getNormalisedValue() + d);
  });
  const end = () => {
    if (!last) return;
    last = null;
    el.classList.remove("dragging");
    state.sliderDragEnded();
  };
  el.addEventListener("pointerup", end);
  el.addEventListener("pointercancel", end);

  el.addEventListener("dblclick", () => {
    state.sliderDragStarted(); set(DEFAULT_NORM[id]); state.sliderDragEnded();
  });
  el.addEventListener("wheel", e => {
    e.preventDefault();
    if (!wheelTimer) state.sliderDragStarted();
    clearTimeout(wheelTimer);
    set(state.getNormalisedValue() - Math.sign(e.deltaY) * (e.shiftKey ? 0.005 : 0.02));
    wheelTimer = setTimeout(() => { wheelTimer = null; state.sliderDragEnded(); }, 300);
  }, { passive: false });
  el.addEventListener("keydown", e => {
    const step = e.shiftKey ? 0.005 : 0.02;
    const map = { ArrowUp: step, ArrowRight: step, ArrowDown: -step, ArrowLeft: -step, PageUp: 0.1, PageDown: -0.1 };
    if (e.key in map) {
      e.preventDefault();
      state.sliderDragStarted(); set(state.getNormalisedValue() + map[e.key]); state.sliderDragEnded();
    } else if (e.key === "Home" || e.key === "Enter") {
      e.preventDefault();
      state.sliderDragStarted(); set(DEFAULT_NORM[id]); state.sliderDragEnded();
    }
  });
}

document.querySelectorAll(".knob").forEach(buildKnob);

// ------------------------------------------------------------------ Amount slider
{
  const el = document.getElementById("amountSlider");
  const state = slider("amount");
  const fill = el.querySelector(".hslider-fill"), thumb = el.querySelector(".hslider-thumb");
  const label = document.getElementById("amountValue");
  const render = () => {
    const n = state.getNormalisedValue();
    fill.style.width = (n * 100) + "%";
    thumb.style.left = (n * 100) + "%";
    label.textContent = format("amount", n);
    el.setAttribute("aria-valuetext", label.textContent);
  };
  state.valueChangedEvent.addListener(render);
  state.propertiesChangedEvent?.addListener(render);
  render();
  const toNorm = e => {
    const r = el.getBoundingClientRect();
    return (e.clientX - r.left) / r.width;
  };
  attachDrag(el, state, "amount", (dx, dy, fine, e) => {
    if (fine) return dx * 0.001;
    state.setNormalisedValue(Math.min(1, Math.max(0, toNorm(e))));
    return null;
  }, toNorm);
}

// ------------------------------------------------------------------ key / scale selects
function bindSelect(selectId, paramId) {
  const sel = document.getElementById(selectId);
  const state = combo(paramId);
  const fill = () => {
    const choices = state.properties?.choices || [];
    if (sel.options.length !== choices.length)
      sel.innerHTML = choices.map((c, i) => `<option value="${i}">${c}</option>`).join("");
    sel.value = String(state.getChoiceIndex());
  };
  state.propertiesChangedEvent.addListener(fill);
  state.valueChangedEvent.addListener(fill);
  sel.addEventListener("change", () => state.setChoiceIndex(Number(sel.value)));
  fill();
}
bindSelect("keySelect", "key");
bindSelect("scaleSelect", "scale");

// ------------------------------------------------------------------ artist roster
let artists = PREVIEW_ARTISTS;
if (inPlugin) {
  try {
    const list = await Juce.getNativeFunction("getArtists")();
    if (Array.isArray(list) && list.length) artists = list;
  } catch (e) { /* keep fallback table */ }
}

const artistState = combo("artist");
const listEl = document.getElementById("artistList");
const nameEl = document.getElementById("artistName");
const tagEl = document.getElementById("artistTagline");

{
  let html = "", lastCat = null;
  for (const a of artists) {
    if (a.category !== lastCat) {
      html += `<div class="group-title">${a.category}</div>`;
      lastCat = a.category;
    }
    html += `<button class="artist" data-index="${a.index}" style="--dot:${a.accent}"><i></i>${a.name}</button>`;
  }
  listEl.innerHTML = html;
  listEl.addEventListener("click", e => {
    const b = e.target.closest(".artist");
    if (b) artistState.setChoiceIndex(Number(b.dataset.index));
  });
}

let shown = -1;
function renderArtist() {
  const idx = artistState.getChoiceIndex();
  const a = artists.find(x => x.index === idx) || artists[0];
  if (!a || idx === shown) return;
  const first = shown === -1;
  shown = idx;
  document.documentElement.style.setProperty("--accent", a.accent);
  nameEl.textContent = a.name;
  tagEl.textContent = a.tagline;
  // Shrink very long names so they always fit on one line
  nameEl.style.fontSize = "";
  requestAnimationFrame(() => {
    const max = nameEl.parentElement.clientWidth;
    if (nameEl.scrollWidth > max) {
      const cur = parseFloat(getComputedStyle(nameEl).fontSize);
      nameEl.style.fontSize = Math.floor(cur * max / nameEl.scrollWidth) + "px";
    }
  });
  if (!first) {
    nameEl.classList.remove("swap");
    void nameEl.offsetWidth;
    nameEl.classList.add("swap");
  }
  listEl.querySelectorAll(".artist").forEach(b => {
    const on = Number(b.dataset.index) === idx;
    b.setAttribute("aria-current", on ? "true" : "false");
    if (on && !first) b.scrollIntoView({ block: "nearest" });
  });
}
artistState.valueChangedEvent.addListener(renderArtist);
artistState.propertiesChangedEvent.addListener(renderArtist);
renderArtist();
// Non-ASCII text is \u-escaped so the page decodes identically in every WebView.
// The UI is laid out on a 960x620 canvas and zoomed to whatever size the host window is.
function fit() {
  document.body.style.zoom = String(Math.min(innerWidth / 960, innerHeight / 620));
}
addEventListener("resize", fit);
fit();

// ------------------------------------------------------------------ telemetry (meters + pitch)
const meterIn = document.getElementById("meterIn"), meterOut = document.getElementById("meterOut");
const noteName = document.getElementById("noteName"), noteSub = document.getElementById("noteSub");
const centsSung = document.getElementById("centsSung"), centsOut = document.getElementById("centsOut");
const pitchEl = document.querySelector(".pitch");
const compEl = document.getElementById("compGr"), deessEl = document.getElementById("deessGr");

const noteLabel = m => NOTE_NAMES[((m % 12) + 12) % 12] + (Math.floor(m / 12) - 1);
const meterPct = db => Math.max(0, Math.min(100, (db + 60) / 60 * 100));
const centsPos = c => 50 + Math.max(-50, Math.min(50, c));
let silentFrames = 0;

function onTelemetry(t) {
  meterIn.style.width = meterPct(t.in) + "%";
  meterOut.style.width = meterPct(t.out) + "%";
  compEl.textContent = (t.comp > 0.05 ? "\u2212" : "") + t.comp.toFixed(1) + " dB";
  deessEl.textContent = (t.deess > 0.05 ? "\u2212" : "") + t.deess.toFixed(1) + " dB";

  if (t.detected > 0) {
    silentFrames = 0;
    const target = Math.round(t.target);
    noteName.textContent = noteLabel(target);
    const sungC = (t.detected - target) * 100;
    const outC = ((t.output > 0 ? t.output : t.detected) - target) * 100;
    noteSub.textContent = `S\u00f6ylenen ${sungC >= 0 ? "+" : "\u2212"}${Math.abs(Math.round(sungC))} cent`;
    centsSung.style.left = centsPos(sungC) + "%";
    centsOut.style.left = centsPos(outC) + "%";
    pitchEl.classList.add("live");
  } else if (++silentFrames > 12) {
    pitchEl.classList.remove("live");
    noteName.textContent = "\u2013";
    noteSub.textContent = "Ses bekleniyor";
  }
}

if (inPlugin) {
  window.__JUCE__.backend.addEventListener("telemetry", onTelemetry);
} else {
  // preview: a sung note drifting around A3, with the tuner pulling it in
  let ph = 0;
  setInterval(() => {
    ph += 0.06;
    const det = 57 + 0.32 * Math.sin(ph) + 0.08 * Math.sin(ph * 3.1);
    const tuneN = mocks.tune.getNormalisedValue();
    const out = det + (57 - det) * Math.min(1, tuneN * 1.6);
    onTelemetry({ in: -14 + 6 * Math.sin(ph * 2), out: -10 + 5 * Math.sin(ph * 2), comp: 4.2 + Math.sin(ph), deess: 1.1,
                  detected: det, target: 57, output: out });
  }, 33);
}
