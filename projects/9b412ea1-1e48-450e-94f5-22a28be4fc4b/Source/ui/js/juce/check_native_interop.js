// JUCE Native Interop Check
// Verifies that the native C++ integration is working

(function() {
    if (typeof __JUCE__ === 'undefined') {
        console.error('JUCE native integration not available');
        return;
    }
    
    if (!__JUCE__.backend) {
        console.error('JUCE backend not initialized');
        return;
    }
    
    console.log('✓ JUCE native integration active');
    console.log('✓ Backend methods:', Object.keys(__JUCE__.backend));
})();
