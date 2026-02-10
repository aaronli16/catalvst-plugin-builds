// JUCE Frontend Integration Library
// This file provides the bridge between HTML/JS UI and C++ audio parameters

export function getSliderState(parameterID) {
    if (typeof __JUCE__ === 'undefined' || !__JUCE__.backend) {
        console.error('JUCE backend not available');
        return createFallbackState(parameterID);
    }
    return __JUCE__.backend.getSliderState(parameterID);
}

export function getToggleState(parameterID) {
    if (typeof __JUCE__ === 'undefined' || !__JUCE__.backend) {
        console.error('JUCE backend not available');
        return createFallbackToggleState(parameterID);
    }
    return __JUCE__.backend.getToggleState(parameterID);
}

// Fallback for when JUCE backend isn't loaded yet
function createFallbackState(parameterID) {
    let value = 0.0;
    const listeners = [];
    
    return {
        parameterID,
        getNormalisedValue: () => value,
        setNormalisedValue: (newValue) => {
            value = Math.max(0, Math.min(1, newValue));
            listeners.forEach(listener => listener());
        },
        valueChangedEvent: {
            addListener: (callback) => listeners.push(callback)
        }
    };
}

function createFallbackToggleState(parameterID) {
    let value = false;
    const listeners = [];
    
    return {
        parameterID,
        getValue: () => value,
        setValue: (newValue) => {
            value = !!newValue;
            listeners.forEach(listener => listener());
        },
        valueChangedEvent: {
            addListener: (callback) => listeners.push(callback)
        }
    };
}

// Wait for JUCE backend to be ready
if (typeof __JUCE__ !== 'undefined') {
    console.log('JUCE backend detected');
} else {
    console.warn('JUCE backend not detected - using fallback mode');
}
