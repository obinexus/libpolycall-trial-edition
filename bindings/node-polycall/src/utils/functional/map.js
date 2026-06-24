// src/utils/functional/map.js
const curry = require('./curry');

/**
 * Maps a function over an array, returning a new array
 * Pure point-free array transformation
 * @param {Function} fn - Mapping function
 * @param {Array} arr - Array to map over
 * @returns {Array} New mapped array
 */
const map = curry((fn, arr) => {
    if (!Array.isArray(arr)) {
        throw new TypeError('Second argument must be an array');
    }
    
    const result = new Array(arr.length);
    for (let i = 0; i < arr.length; i++) {
        result[i] = fn(arr[i], i, arr);
    }
    return result;
});

module.exports = map;