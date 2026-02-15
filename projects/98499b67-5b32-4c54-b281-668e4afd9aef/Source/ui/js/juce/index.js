/**
 * JUCE frontend module — re-exports helpers from the native backend.
 * At runtime the WebView injects `window.__JUCE__`; we surface its API
 * as ES-module exports so the UI can `import { getSliderState } from …`.
 */

const juce = window.__JUCE__;

export function getSliderState(paramId) {
    return juce.getSliderState(paramId);
}

export function getToggleState(paramId) {
    return juce.getToggleState(paramId);
}
