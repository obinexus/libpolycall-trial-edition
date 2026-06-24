// src/utils/functional/filter.js
const curry = require('./curry');

/**
 * Filters an array based on a predicate function
 * Pure point-free array filtering
 * @param {Function} predicate - Filter predicate
 * @param {Array} arr - Array to filter
 * @returns {Array} New filtered array
 */
const filter = curry((predicate, arr) => {
    if (!Array.isArray(arr)) {
        throw new TypeError('Second argument must be an array');
    }
    
    const result = [];
    for (let i = 0; i < arr.length; i++) {
        if (predicate(arr[i], i, arr)) {
            result.push(arr[i]);
        }
    }
    return result;
});

module.exports = filter;