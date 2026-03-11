/**
 * Real-Time Subtitle - Mobile Frontend
 * Phase 1: Speech capture → Speech-to-text → Translation (via server)
 */

(function () {
  'use strict';

  const RECONNECT_DELAY_MS = 3000;
  const WS_PATH = '/ws';

  const elements = {
    status: document.getElementById('status'),
    transcript: document.getElementById('transcript'),
    translated: document.getElementById('translated'),
    recordBtn: document.getElementById('recordBtn'),
    sourceLang: document.getElementById('sourceLang'),
    targetLang: document.getElementById('targetLang'),
  };

  let ws = null;
  let recognition = null;
  let isListening = false;
  let useHttpFallback = false;
  let sendDebounceTimer = null;
  let lastSentText = '';
  let finalTranscript = '';
  const SEND_DEBOUNCE_MS = 250;

  function getWsUrl() {
    const params = new URLSearchParams(location.search);
    const server = params.get('server') || location.host;
    const protocol = location.protocol === 'https:' ? 'wss:' : 'ws:';
    return `${protocol}//${server}${WS_PATH}`;
  }

  function setStatus(text, className = '') {
    elements.status.textContent = text;
    elements.status.className = 'status' + (className ? ' ' + className : '');
  }

  function connect() {
    if (ws && ws.readyState === WebSocket.OPEN) return;

    setStatus('Connecting...');
    ws = new WebSocket(getWsUrl());

    ws.onopen = () => {
      useHttpFallback = false;
      setStatus('Connected', 'connected');
      if (recognition) elements.recordBtn.disabled = false;
    };

    ws.onclose = () => {
      setStatus('Disconnected');
      elements.recordBtn.disabled = true;
      if (!useHttpFallback) {
        useHttpFallback = true;
        setStatus('Using HTTP fallback', 'connected');
        if (recognition) elements.recordBtn.disabled = false;
      } else {
        setTimeout(connect, RECONNECT_DELAY_MS);
      }
    };

    ws.onerror = () => {
      setStatus('Error');
    };

    ws.onmessage = (event) => {
      try {
        const data = JSON.parse(event.data);
        if (data.translated !== undefined) {
          elements.translated.textContent = data.translated || '—';
        }
      } catch (e) {
        elements.translated.textContent = 'Invalid response from server.';
      }
    };
  }

  function initSpeechRecognition() {
    const isSecure = window.isSecureContext; // HTTPS or localhost
    const SpeechRecognition = window.SpeechRecognition || window.webkitSpeechRecognition;
    if (!SpeechRecognition) {
      if (!isSecure) {
        elements.transcript.textContent = 'Use the HTTPS link (https://...tunnelmole.net), not HTTP. Microphone requires HTTPS on mobile.';
      } else {
        elements.transcript.textContent = 'Speech recognition not supported. On iPhone, try Safari. On Android, try Chrome.';
      }
      return false;
    }

    recognition = new SpeechRecognition();
    recognition.continuous = true;
    recognition.interimResults = true;
    const langMap = { en: 'en-US', es: 'es-ES', fr: 'fr-FR', de: 'de-DE', zh: 'zh-CN', ja: 'ja-JP', ko: 'ko-KR', ar: 'ar-SA', hi: 'hi-IN' };
    recognition.lang = langMap[elements.sourceLang.value] || elements.sourceLang.value;

    recognition.onresult = (event) => {
      let lastFinal = '';
      let interim = '';
      for (let i = event.resultIndex; i < event.results.length; i++) {
        const result = event.results[i];
        const text = (result[0]?.transcript || '').trim();
        if (!text) continue;
        if (result.isFinal) {
          lastFinal = text;
        } else {
          interim = text;
        }
      }
      if (lastFinal) {
        const prev = finalTranscript.trim();
        if (!prev || lastFinal.startsWith(prev) || lastFinal === prev) {
          finalTranscript = lastFinal;
        } else if (!prev.endsWith(lastFinal)) {
          finalTranscript = prev + ' ' + lastFinal;
        }
        clearTimeout(sendDebounceTimer);
        sendDebounceTimer = setTimeout(() => {
          const toSend = finalTranscript.trim();
          if (toSend && toSend !== lastSentText) {
            lastSentText = toSend;
            sendToServer(toSend);
          }
          sendDebounceTimer = null;
        }, SEND_DEBOUNCE_MS);
      }
      let display = finalTranscript.trim();
      if (interim) {
        display = interim.startsWith(display) ? interim : (display ? display + ' ' + interim : interim);
      }
      elements.transcript.textContent = display || 'Speak to see transcription...';
    };

    recognition.onerror = (event) => {
      if (event.error === 'no-speech') return;
      console.error('Speech recognition error:', event.error);
    };

    recognition.onend = () => {
      if (isListening) {
        try { recognition.start(); } catch (_) {}
      }
    };

    return true;
  }

  elements.sourceLang.addEventListener('change', () => {
    if (recognition) {
      const langMap = { en: 'en-US', es: 'es-ES', fr: 'fr-FR', de: 'de-DE', zh: 'zh-CN', ja: 'ja-JP', ko: 'ko-KR', ar: 'ar-SA', hi: 'hi-IN' };
      recognition.lang = langMap[elements.sourceLang.value] || elements.sourceLang.value;
    }
  });

  async function sendToServer(text) {
    if (!text) return;
    elements.translated.textContent = 'Translating...';
    const sourceLang = elements.sourceLang.value;
    const targetLang = elements.targetLang.value;
    if (useHttpFallback || !ws || ws.readyState !== WebSocket.OPEN) {
      try {
        const r = await fetch('/translate', {
          method: 'POST',
          headers: { 'Content-Type': 'application/json' },
          body: JSON.stringify({ text, sourceLang, targetLang })
        });
        const data = await r.json();
        const txt = data.translated;
        if (!r.ok) {
          elements.translated.textContent = 'Error ' + r.status + ': ' + (txt || r.statusText);
        } else if (txt) {
          elements.translated.textContent = txt;
        } else {
          elements.translated.textContent = '(no translation)';
        }
      } catch (e) {
        elements.translated.textContent = 'Not connected. Try: make run';
      }
      return;
    }
    ws.send(JSON.stringify({ text, sourceLang, targetLang }));
  }

  async function requestMicrophoneThenStart() {
    if (!navigator.mediaDevices || !navigator.mediaDevices.getUserMedia) {
      elements.transcript.textContent = 'Microphone access not available. Use HTTPS or localhost.';
      return;
    }

    setStatus('Requesting microphone...', 'listening');
    elements.transcript.textContent = 'Allow microphone access when your browser prompts...';

    try {
      const stream = await navigator.mediaDevices.getUserMedia({ audio: true });
      stream.getTracks().forEach(t => t.stop());
      elements.transcript.textContent = '';
      elements.translated.textContent = 'Translation will appear here...';
      lastSentText = '';
      finalTranscript = '';
      clearTimeout(sendDebounceTimer);
      sendDebounceTimer = null;
      setStatus('Listening...', 'listening');
      try {
        recognition.start();
      } catch (e) {
        elements.transcript.textContent = 'Speech failed to start. Ensure Dictation is enabled (Settings > General > Keyboard).';
        throw e;
      }
    } catch (err) {
      if (err.name === 'NotAllowedError' || err.name === 'PermissionDeniedError') {
        elements.transcript.textContent = 'Microphone denied. Tap the address bar, then the mic icon, and allow access. Or check Settings > Safari > Microphone.';
      } else {
        elements.transcript.textContent = 'Microphone error: ' + (err.message || 'Use HTTPS or localhost for microphone access.');
      }
      setStatus('Connected', 'connected');
      isListening = false;
      elements.recordBtn.classList.remove('listening');
      elements.recordBtn.querySelector('.btn-label').textContent = 'Start Listening';
    }
  }

  function toggleListening() {
    if (!recognition) return;

    if (isListening) {
      isListening = false;
      elements.recordBtn.classList.remove('listening');
      elements.recordBtn.querySelector('.btn-label').textContent = 'Start Listening';
      setStatus('Connected', 'connected');
      recognition.stop();
      return;
    }

    isListening = true;
    elements.recordBtn.classList.add('listening');
    elements.recordBtn.querySelector('.btn-label').textContent = 'Stop Listening';
    elements.recordBtn.disabled = true;

    requestMicrophoneThenStart().then(() => {
      elements.recordBtn.disabled = false;
    }).catch(() => {
      elements.recordBtn.disabled = false;
      isListening = false;
      elements.recordBtn.classList.remove('listening');
      elements.recordBtn.querySelector('.btn-label').textContent = 'Start Listening';
    });
  }

  function init() {
    initSpeechRecognition();
    useHttpFallback = new URLSearchParams(location.search).get('fallback') === '1';
    if (useHttpFallback) {
      setStatus('HTTP mode', 'connected');
      if (recognition) elements.recordBtn.disabled = false;
    } else {
      connect();
    }
    elements.recordBtn.addEventListener('click', toggleListening);
  }

  if (document.readyState === 'loading') {
    document.addEventListener('DOMContentLoaded', init);
  } else {
    init();
  }
})();
