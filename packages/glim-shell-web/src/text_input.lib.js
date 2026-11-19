// @ts-nocheck
// The web shell's text bridge (TextInput.cpp). A hidden <input> holds the
// page's focus while a field edits: the browser's keyboard, IME, copy, cut
// and paste act on it, and each edit is sent to the field instead of
// applied. Positions are UTF-16, as the DOM counts them.
addToLibrary({
  glim_web_text_publish__deps: ['$UTF8ToString', '$stringToNewUTF8', 'free', 'glim_web_text', 'glim_web_text_focus'],
  glim_web_text_publish: (active, secure, text, start, end, x, y, h) => {
    let el = Module.glimTextInput;
    if (!el) {
      el = document.createElement("input");
      el.setAttribute("autocomplete", "off");
      el.setAttribute("aria-hidden", "true");
      el.style.cssText = "position:fixed;opacity:0;pointer-events:none;width:1px;height:1px;border:0;padding:0";
      const send = (kind, s, a, b) => {
        const p = s ? stringToNewUTF8(s) : 0;
        _glim_web_text(kind, p, a | 0, b | 0);
        if (p) _free(p);
      };
      el.addEventListener("beforeinput", (e) => {
        if (e.isComposing || e.inputType === "insertCompositionText") return;
        e.preventDefault();
        const t = e.inputType;
        if (t.startsWith("insert")) {
          const data = e.data ?? (e.dataTransfer ? e.dataTransfer.getData("text/plain") : "");
          if (data) send(0, data);
        } else if (t === "deleteContentForward" || t === "deleteWordForward") {
          send(4);
        } else if (t.startsWith("delete")) {
          send(3);
        }
      });
      el.addEventListener("compositionstart", () => { el.glimComposing = true; });
      el.addEventListener("compositionupdate", (e) => send(1, e.data || ""));
      el.addEventListener("compositionend", (e) => {
        el.glimComposing = false;
        if (e.data) send(0, e.data); else send(5);
      });
      document.addEventListener("selectionchange", () => {
        if (document.activeElement !== el || el.glimComposing || el.glimSetting) return;
        if (el.selectionStart === el.glimStart && el.selectionEnd === el.glimEnd) return;
        send(2, null, el.selectionStart, el.selectionEnd);
      });
      el.addEventListener("focus", () => _glim_web_text_focus(1));
      el.addEventListener("blur", () => _glim_web_text_focus(0));
      document.body.appendChild(el);
      Module.glimTextInput = el;
    }
    if (!active) {
      if (document.activeElement === el) el.blur();
      return;
    }
    el.type = secure ? "password" : "text";
    el.style.left = x + "px";
    el.style.top = y + "px";
    el.style.height = h + "px";
    if (!el.glimComposing) {
      const value = UTF8ToString(text);
      el.glimSetting = true;
      if (el.value !== value) el.value = value;
      el.glimStart = start;
      el.glimEnd = end;
      el.setSelectionRange(start, end);
      el.glimSetting = false;
    }
    if (document.activeElement !== el) el.focus({preventScroll: true});
  },

  // The system clipboard takes text asynchronously; reads are not offered,
  // since a synchronous read is impossible in the browser.
  glim_web_clipboard_write__deps: ['$UTF8ToString'],
  glim_web_clipboard_write: (text) => {
    if (!navigator.clipboard || !navigator.clipboard.writeText) return 0;
    navigator.clipboard.writeText(UTF8ToString(text)).catch(() => {});
    return 1;
  },

  // The canvas's right-click is Glim's: no browser context menu over it.
  glim_web_canvas_init__deps: ['$UTF8ToString'],
  glim_web_canvas_init: (selector) => {
    const canvas = document.querySelector(UTF8ToString(selector));
    if (canvas) canvas.addEventListener('contextmenu', (e) => e.preventDefault());
  },
});
