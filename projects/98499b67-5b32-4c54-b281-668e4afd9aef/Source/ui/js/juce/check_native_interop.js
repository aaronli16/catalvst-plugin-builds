/**
 * JUCE native interop check — verifies the backend bridge is available.
 */
if (! window.__JUCE__) {
    console.warn("JUCE native backend not available.");
}
